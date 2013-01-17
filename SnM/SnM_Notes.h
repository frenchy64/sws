/******************************************************************************
/ SnM_Notes.h
/
/ Copyright (c) 2010-2013 Jeffos
/ http://www.standingwaterstudios.com/reaper
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

//#pragma once

#ifndef _SNM_NOTES_H_
#define _SNM_NOTES_H_

enum
{
  SNM_NOTES_PROJECT=0,
  SNM_NOTES_ITEM,
  SNM_NOTES_TRACK,
  SNM_NOTES_REGION_NAME,
  SNM_NOTES_REGION_SUBTITLES,
  SNM_NOTES_ACTION_HELP // must remain the last item: no OSX support yet 
};


class SNM_TrackNotes {
public:
	SNM_TrackNotes(MediaTrack* _tr, const char* _notes)
		: m_tr(_tr),m_notes(_notes ? _notes : "") {}
	MediaTrack* m_tr; WDL_FastString m_notes;
};

class SNM_RegionSubtitle {
public:
	SNM_RegionSubtitle(int _id, const char* _notes) 
		: m_id(_id),m_notes(_notes ? _notes : "") {}
	int m_id; WDL_FastString m_notes;
};

class NotesUpdateJob : public SNM_ScheduledJob {
public:
	NotesUpdateJob() : SNM_ScheduledJob(SNM_SCHEDJOB_NOTEHLP_UPDATE, 150) {}
	void Perform();
};

class NotesMarkerRegionSubscriber : public SNM_MarkerRegionSubscriber {
public:
	NotesMarkerRegionSubscriber() : SNM_MarkerRegionSubscriber() {}
	void NotifyMarkerRegionUpdate(int _updateFlags);
};

class SNM_NotesHelpWnd : public SWS_DockWnd
{
public:
	SNM_NotesHelpWnd();
	~SNM_NotesHelpWnd();

	void SetType(int _type);
	void SetText(const char* _str, bool _addRN = true);
	void RefreshGUI();
	void CSurfSetTrackTitle();
	void CSurfSetTrackListChange();
	void OnCommand(WPARAM wParam, LPARAM lParam);
	void ToggleLock();

	void SaveCurrentText(int _type);
	void SaveCurrentPrjNotes();
	void SaveCurrentHelp();
	void SaveCurrentItemNotes();
	void SaveCurrentTrackNotes();
	void SaveCurrentMkrRgnNameOrNotes(bool _name);

	void Update(bool _force = false);
	int UpdateActionHelp();
	int UpdateItemNotes();
	int UpdateTrackNotes();
	int UpdateMkrRgnNameOrNotes(bool _name);

protected:
	void OnInitDlg();
	void OnDestroy();
	bool IsActive(bool bWantEdit = false);
	HMENU OnContextMenu(int x, int y, bool* wantDefaultItems);
	int OnKey(MSG* msg, int iKeyState);
	void OnTimer(WPARAM wParam=0);
	void OnResize();
	void DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight = NULL);
	bool GetToolTipString(int _xpos, int _ypos, char* _bufOut, int _bufOutSz);

	WDL_VirtualComboBox m_cbType;
	WDL_VirtualIconButton m_btnLock;
	SNM_ToolbarButton m_btnAlr, m_btnActionList, m_btnImportSub, m_btnExportSub;
	WDL_VirtualStaticText m_txtLabel;
	SNM_DynSizedText m_bigNotes;

	NotesMarkerRegionSubscriber m_mkrRgnSubscriber;
};


void LoadHelp(const char* _cmdName, char* _buf, int _bufSize);
void SaveHelp(const char* _cmdName, const char* _help);
bool GetStringFromNotesChunk(WDL_FastString* _notes, char* _buf, int _bufMaxSize);
bool GetNotesChunkFromString(const char* _buf, WDL_FastString* _notes, const char* _startLine = NULL);

extern SWSProjConfig<WDL_PtrList_DeleteOnDestroy<SNM_TrackNotes> > g_pTrackNotes;

void SetActionHelpFilename(COMMAND_T*);
int NotesHelpViewInit();
void NotesHelpViewExit();
void ImportSubTitleFile(COMMAND_T*);
void ExportSubTitleFile(COMMAND_T*);
void OpenNotesHelpView(COMMAND_T*);
bool IsNotesHelpViewDisplayed(COMMAND_T*);
void ToggleNotesHelpLock(COMMAND_T*);
bool IsNotesHelpLocked(COMMAND_T*);

#endif