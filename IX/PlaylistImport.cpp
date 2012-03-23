/******************************************************************************
/ PlaylistImport.cpp
/
/ Copyright (c) 2012 Philip S. Considine (IX)
/
/ Permission is hereby granted, free of charge, to any person obtaining a copy
/ of this software and associated documentation files (the "Software"), to deal
/ in the Software without restriction, including without limitation the rights to
/ use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
/ of the Software, and to permit persons to whom the Software is furnished to
/ do so, subject to the following conditions:
/ 
/ The above copyright notice and this permission notice shall be included in all
/ copies or substantial portions of the Software.
/ 
/ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
/ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
/ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
/ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
/ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
/ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
/ OTHER DEALINGS IN THE SOFTWARE.
/
******************************************************************************/

#include "stdafx.h"
#include "IX.h"


#define IXPLAYLISTSTRING	"Import m3u/pls playlist"
#define IXPLAYLISTID		"SWS/IX: "IXPLAYLISTSTRING

struct SPlaylistEntry
{
	SPlaylistEntry() : length(0) { }
	~SPlaylistEntry() { }

	double length;
	string title;
	string path;
};

// Fill outbuf with filenames retrieved from playlist
void ParseM3U(string listpath, vector<SPlaylistEntry> &filelist)
{
	char buf[1024] = {0};
	ifstream file(listpath.c_str(), ios_base::in);
	
	if(file.is_open())
	{
		// m3u playlist file should start with the string "#EXTM3U"
		file.getline(buf, sizeof(buf));
		string str = buf;
		if(str.find("#EXTM3U") != 0)
		{
			return;
		}

		string pathroot = listpath.substr(0, listpath.find_last_of("\\") + 1);
		
		// Each entry in the playlist consists of two lines.
		// The first contains the length in seconds and the title, the second should be the file path relative to the playlist location.
		//	#EXTINF:331,Bernard Pretty Purdie - Hap'nin'
		//	Music\Bernard Pretty Purdie\Lialeh\07 - Hap'nin'.mp3
		// Streaming media should have length -1
		while(!file.eof())
		{
			SPlaylistEntry e;

			file.getline(buf, sizeof(buf));
			string str = buf;

			if(str.find("#EXTINF:") == 0)
			{
				str = str.substr(str.find_first_of(":") + 1); // Remove "#EXTINF:"

				e.length = atof(str.substr(0, str.find_first_of(",")).c_str());

				if(e.length < 0) continue; // Streaming media should have length -1

				e.title = str.substr(str.find_first_of(",") + 1);

				file.getline(buf, sizeof(buf));
				e.path = buf;

				// Make path absolute if not already
				if(e.path.find(':') == string::npos)
				{
					e.path.insert(0, pathroot);
				}

				filelist.push_back(e);
			}
		}
	}

	file.close();
}

// Fill filelist with info retrieved from the playlist
void ParsePLS(string listpath, vector<SPlaylistEntry> &filelist)
{
	char buf[1024] = {0};
	ifstream file(listpath.c_str(), ios_base::in);
	
	if(file.is_open())
	{
		// pls playlist file should start with the string "[playlist]"
		file.getline(buf, sizeof(buf));
		string str = buf;
		if(str.find("[playlist]") == string::npos)
		{
			return;
		}

		string pathroot = listpath.substr(0, listpath.find_last_of("\\") + 1);

		// Find number of entries
		int count = 0;
		while(!file.eof())
		{
			file.getline(buf, sizeof(buf));
			str = buf;
			if(str.find("NumberOfEntries") != string::npos)
			{
				count = atoi(str.substr(str.find_first_of("=") + 1).c_str());
				filelist.resize(count);
				break;
			}
		}

		if(count == 0) return;
		
		// Each entry in the playlist consists of three lines:
		//	File1=E:\Music\James Brown\20 all time greatest hits!\13 - Get On The Good Foot.mp3
		//	Title1=James Brown - Get On The Good Foot
		//	Length1=215
		// Streaming media should have length -1
		file.seekg(0);
		while(!file.eof())
		{
			file.getline(buf, sizeof(buf));
			string str = buf;

			if(str.empty()) continue;

			str = str.erase(str.find_last_of('\r')); // Winamp pls files have extra carriage returns which screw up file_exists()

			if(str.find("File") == 0)
			{
				str = str.substr(4); // Strip "File"
				int index = atoi(str.substr(0, str.find_first_of("=")).c_str()) - 1;

				if(index > -1 && index < count)
				{
					SPlaylistEntry &e = filelist.at(index);
					e.path = str.substr(str.find_first_of("=") + 1);

					// Make path absolute if not already
					if(e.path.find(':') == string::npos)
					{
						e.path.insert(0, pathroot);
					}
				}
			}
			else if(str.find("Title") == 0)
			{
				str = str.substr(5); // Strip "Title"
				int index = atoi(str.substr(0, str.find_first_of("=")).c_str()) - 1;

				if(index > -1 && index < count)
				{
					SPlaylistEntry &e = filelist.at(index);
					e.title = str.substr(str.find_first_of("=") + 1);
				}
			}
			else if(str.find("Length") == 0)
			{
				str = str.substr(6); // Strip "Length"
				int index = atoi(str.substr(0, str.find_first_of("=")).c_str()) - 1;

				if(index > -1 && index < count)
				{
					SPlaylistEntry &e = filelist.at(index);
					e.length = atoi(str.substr(str.find_first_of("=") + 1).c_str());
				}
			}
		}
	}

	// Remove streaming media
	vector<SPlaylistEntry>::iterator it = filelist.begin();
	while(it != filelist.end())
	{
		if(it->length < 0)
		{
			it = filelist.erase(it);
		}
		else
		{
			++it;
		}
	}

	file.close();
}

// Import local files from m3u/pls playlists onto a new track
void PlaylistImport(COMMAND_T* ct)
{
	char cPath[256];
	vector<SPlaylistEntry> filelist;

	GetProjectPath(cPath, 256);
	string listpath = BrowseForFiles("Import playlist", cPath, NULL, false, "Playlist files (*.m3u,*.pls)\0*.m3u;*.pls\0All Files (*.*)\0*.*\0");
	string ext = ParseFileExtension(listpath);

	// Decide what kind of playlist we have
	if(ext == "m3u")
	{
		ParseM3U(listpath, filelist);
	}
	else if(ext == "pls")
	{
		ParsePLS(listpath, filelist);
	}

	if(filelist.empty())
	{
		ShowMessageBox("Failed to import playlist. No files found.", "Import playlist", 0);
		return;
	}

	// Validate files
	vector<string> badfiles;
	for(int i = 0, c = filelist.size(); i < c; i++)
	{
		SPlaylistEntry e = filelist[i];
		if(!file_exists(e.path.c_str()))
		{
			badfiles.push_back(e.path);
		}
	}

	// If files can't be found, ask user what to do.
	bool includeMissing = false;
	if(!badfiles.empty())
	{
		stringstream ss;
		ss << "Cannot find some files. Create items anyway?\n";

		unsigned int limit = min(badfiles.size(), 9); // avoid enormous messagebox
		for(unsigned int i = 0; i < limit; i++)
		{
			ss << "\n " << badfiles[i];
		}
		if(badfiles.size() > limit)
		{
			ss << "\n +" << badfiles.size() - limit << " more files";
		}

		switch(ShowMessageBox(ss.str().c_str(), "Import playlist", 3))
		{
		case 6 : // Yes
			includeMissing = true;
			break;

		case 7 : // No
			break;

		default :
			return;
		}
	}

	Undo_BeginBlock2(NULL);

	// Add new track
	int idx = GetNumTracks();
	int panMode = 5;		// Stereo pan
	double panLaw = 1.0;	// 0dB
	InsertTrackAtIndex(idx, false);
	MediaTrack *pTrack = GetTrack(NULL, idx);
	GetSetMediaTrackInfo(pTrack, "P_NAME", (void*) listpath.c_str());
	GetSetMediaTrackInfo(pTrack, "I_PANMODE", (void*) &panMode);
	GetSetMediaTrackInfo(pTrack, "D_PANLAW", (void*) &panLaw);
	SetOnlyTrackSelected(pTrack);

	// Add new items to track
	double pos = 0.0;
	for(int i = 0, c = filelist.size(); i < c; i++)
	{
		SPlaylistEntry e = filelist[i];

		//TODO: Would be better if missing files were offline rather than just empty items.
		PCM_source *pSrc = PCM_Source_CreateFromFile(e.path.c_str());
		if(pSrc || includeMissing)
		{
			MediaItem *pItem = AddMediaItemToTrack(pTrack);
			if(pItem)
			{
				MediaItem_Take *pTake = AddTakeToMediaItem(pItem);
				if(pTake)
				{
					GetSetMediaItemTakeInfo(pTake, "P_SOURCE", pSrc);
					GetSetMediaItemTakeInfo(pTake, "P_NAME", (void*) e.title.c_str());
					SetMediaItemPosition(pItem, pos, false);
					SetMediaItemLength(pItem, e.length, false);
					pos += e.length;
				}
			}
		}
	}

	Undo_EndBlock2(NULL, IXPLAYLISTID, UNDO_STATE_ITEMS|UNDO_STATE_TRACKCFG);

	TrackList_AdjustWindows(false);
	UpdateTimeline();

	Main_OnCommand(40047, 0); // Build missing peaks
}

static COMMAND_T g_commandTable[] = 
{
	{ { DEFACCEL, IXPLAYLISTID },	"IX_PLAYLIST_IMPORT",	PlaylistImport,	IXPLAYLISTSTRING, 0},

	{ {}, LAST_COMMAND, }, // Denote end of table
};

int PlaylistImportInit()
{
	SWSRegisterCommands(g_commandTable);

	return 1;
}