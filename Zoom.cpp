/******************************************************************************
/ Zoom.cpp
/
/ Copyright (c) 2009 Tim Payne (SWS)
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

#include "stdafx.h"
#include "ObjectState/TrackEnvelope.h"

#define SUPERCOLLAPSED_VIEW_SIZE 3
#define COLLAPSED_VIEW_SIZE 24
#define SMALLISH_SIZE 49
#define NORMAL_SIZE 72
#define MIN_MASTER_SIZE 74
#define VZOOM_RANGE 40
#define VMAX_OFFSET 60

void ZoomTool(COMMAND_T* = NULL);
void SaveZoomSlice(bool bSWS);
#define WM_CANCEL_ZOOM	4000
#define WM_START_ZOOM	4001

void SetReaperWndSize(COMMAND_T* = NULL)
{
	int x = GetPrivateProfileInt("REAPER", "setwndsize_x", 640, get_ini_file());
	int y = GetPrivateProfileInt("REAPER", "setwndsize_y", 480, get_ini_file());
	HWND hwnd = GetMainHwnd();
	SetWindowPos(hwnd, NULL, 0, 0, x, y, SWP_NOMOVE | SWP_NOZORDER);
}

int TrackHeightFromVZoomIndex(HWND hwnd, MediaTrack* tr, int iZoom, SWS_TrackEnvelopes* pTE)
{
	if (!(GetTrackVis(tr) & 2))
		return 0;

	bool bIsMaster = CSurf_TrackToID(tr, false) ? false : true;
	int ret = bIsMaster ? MIN_MASTER_SIZE : NORMAL_SIZE;
	bool bRecArmed = *(int*)GetSetMediaTrackInfo(tr, "I_RECARM", NULL) ? true : false;
	int supercompactmode = *(int*)GetSetMediaTrackInfo(tr, "I_FOLDERCOMPACT", NULL);
	// Todo ^^^ add config var bit
  
	if (iZoom < 4)
	{
		if (/*!g_config_zoomrecshowwhenarmed ||*/ !bRecArmed)
		{
			if      (iZoom==0) ret=COLLAPSED_VIEW_SIZE;
			else if (iZoom==1) ret=(COLLAPSED_VIEW_SIZE+SMALLISH_SIZE)/2;
			else if (iZoom==2) ret=(SMALLISH_SIZE);
			else if (iZoom==3) ret=(SMALLISH_SIZE+NORMAL_SIZE)/2;
		}
	}
	else
	{
		RECT r;
		GetClientRect(hwnd, &r);
		int vz = iZoom;
		int cutoff = VZOOM_RANGE*3/4;
		if (vz >= cutoff)
			vz = cutoff+(vz-cutoff)*3;
		int mv=(VZOOM_RANGE*3)/2;
		vz-=4;
		ret= NORMAL_SIZE + ((r.bottom-NORMAL_SIZE)*vz) / (mv-4);
	}

	if (bIsMaster)
	{
		if (ret<MIN_MASTER_SIZE) ret=MIN_MASTER_SIZE;
		return ret;
	}

	// todo Find the parent first???
	else
	{
		if (supercompactmode == 2 && ret > SUPERCOLLAPSED_VIEW_SIZE)
		{
			ret = SUPERCOLLAPSED_VIEW_SIZE;
		}
		else if (supercompactmode == 1 && ret > COLLAPSED_VIEW_SIZE)
		{
			ret = COLLAPSED_VIEW_SIZE;
		}
	}

	return ret + pTE->GetLanesHeight(ret);
}

void SetHorizPos(HWND hwnd, double dPos, double dOffset = 0.0)
{
	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(hwnd, SB_HORZ, &si);
	si.nPos = (int)(dPos * GetHZoomLevel());
	if (dOffset)
		si.nPos -= (int)(dOffset * si.nPage);
	CoolSB_SetScrollInfo(hwnd, SB_HORZ, &si, true);
	SendMessage(hwnd, WM_HSCROLL, SB_THUMBPOSITION, NULL);
}

void SetVertPos(HWND hwnd, int iTrack, bool bPixels, int iExtra = 0) // 1 based track index!
{
	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(hwnd, SB_VERT, &si);

	int oldmax = si.nMax;
	si.nMax = 0;
	if (bPixels)
		si.nPos = iTrack;
	else
		si.nPos = iExtra;

	for (int i = 0; i <= GetNumTracks(); i++)
	{
		int iHeight = *(int*)GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_WNDH", NULL);
		if (!bPixels && i < iTrack)
			si.nPos += iHeight;
		si.nMax += iHeight;
	}
	si.nMax += 65;

	if (oldmax == si.nPage && (UINT)si.nMax > si.nPage)
		si.nPage++;
	else if ((UINT)si.nMax < si.nPage)
		si.nMax = si.nPage;

	CoolSB_SetScrollInfo(hwnd, SB_VERT, &si, true);
	SendMessage(hwnd, WM_VSCROLL, si.nPos << 16 | SB_THUMBPOSITION, NULL);
}

void VertZoomRange(int iFirst, int iNum, bool* bZoomed, bool bMinimizeOthers)
{
	HWND hTrackView = GetTrackWnd();
	if (!hTrackView || iNum == 0)
		return;

	// Zoom in vertically
	RECT rect;
	GetClientRect(hTrackView, &rect);
	int iTotalHeight = (int)(rect.bottom * 0.99); // Take off a percent

	SWS_TrackEnvelopes* pTES = new SWS_TrackEnvelopes[iNum];
	for (int i = 0; i < iNum; i++)
		pTES[i].SetTrack(CSurf_TrackFromID(i+iFirst, false));

	if (bMinimizeOthers)
	{
		*(int*)GetConfigVar("vzoom2") = 0;
		for (int i = 0; i <= GetNumTracks(); i++)
			GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_HEIGHTOVERRIDE", &g_i0);
		Main_OnCommand(40112, 0); // Zoom out vert to minimize lanes too (calls refresh)
		//TrackList_AdjustWindows(false);
		//UpdateTimeline();

		// Get the size of shown but not zoomed tracks
		int iNotZoomedSize = 0;
		int iZoomed = 0;
		for (int i = 0; i < iNum; i++)
		{
			MediaTrack* tr = CSurf_TrackFromID(i+iFirst, false);
			if (bZoomed[i])
				iZoomed++;
			else
				iNotZoomedSize += TrackHeightFromVZoomIndex(hTrackView, tr, 0, &pTES[i]);
		}
		// Pixels we have to work with will all the sel tracks and their envelopes
		iTotalHeight -= iNotZoomedSize;
		int iEachHeight = iTotalHeight / iZoomed;
		while (true)
		{
			int iLanesHeight = 0;
			// Calc envelope height
			for (int i = 0; i < iNum; i++)
				if (bZoomed[i])
					iLanesHeight += pTES[i].GetLanesHeight(iEachHeight);
			if (iEachHeight * iZoomed + iLanesHeight <= iTotalHeight)
				break;
			
			// Guess new iEachHeight - may be wrong!
			int iNewEachHeight = (int)(iTotalHeight / ((double)iLanesHeight / iEachHeight + iZoomed));
			if (iNewEachHeight == iEachHeight)
				iEachHeight--;
			else
				iEachHeight = iNewEachHeight;
			if (iEachHeight <= MINTRACKHEIGHT)
				break;
		}

		if (iEachHeight > MINTRACKHEIGHT)
		{
			for (int i = 0; i < iNum; i++)
				if (bZoomed[i])
					GetSetMediaTrackInfo(CSurf_TrackFromID(i+iFirst, false), "I_HEIGHTOVERRIDE", &iEachHeight);
			TrackList_AdjustWindows(false);
			UpdateTimeline();
		}
	}
	else
	{
		int iZoom = VZOOM_RANGE+1;
		int iHeight;
		do
		{
			iZoom--;
			iHeight = 0;
			for (int i = 0; i < iNum; i++)
			{
				MediaTrack* tr = CSurf_TrackFromID(i+iFirst, false);
				iHeight += TrackHeightFromVZoomIndex(hTrackView, tr, iZoom, &pTES[i]);
			}
		} while (iHeight > iTotalHeight && iZoom > 0);

		// Reset custom track sizes
		for (int i = 0; i <= GetNumTracks(); i++)
			GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_HEIGHTOVERRIDE", &g_i0);
		*(int*)GetConfigVar("vzoom2") = iZoom;
		TrackList_AdjustWindows(false);
		UpdateTimeline();
	}

	delete [] pTES;
	SetVertPos(hTrackView, iFirst, false);
}

// iOthers == 0 nothing, 1 minimize, 2 hide from TCP
void VertZoomSelTracks(int iOthers)
{
	// Find first and last selected tracks
	int iFirstSel = -1;
	int iLastSel = -1;
	WDL_TypedBuf<bool> hbSelected;
	hbSelected.Resize(GetNumTracks()+1);
	for (int i = 0; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (GetTrackVis(tr) & 2 && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			if (iFirstSel == -1)
				iFirstSel = i;
			iLastSel = i;
			hbSelected.Get()[i] = true;
		}
		else
			hbSelected.Get()[i] = false;
	}
	if (iFirstSel == -1)
		return;

	// Hide tracks from TCP only after making sure there's actually something selected 
	if (iOthers == 2)
		for (int i = 1; i <= GetNumTracks(); i++)
			if (!hbSelected.Get()[i-1])
			{
				MediaTrack* tr = CSurf_TrackFromID(i, false);
				SetTrackVis(tr, GetTrackVis(tr) & 1);
			}

	VertZoomRange(iFirstSel, 1+iLastSel-iFirstSel, hbSelected.Get()+iFirstSel, iOthers == 1);
	SaveZoomSlice(true);
}

// iOthers == 0 nothing, 1 minimize, 2 hide from TCP
void VertZoomSelItems(int iOthers)
{
	// Find the tracks of the first and last selected item
	int y1 = -1, y2 = -1;
	WDL_TypedBuf<bool> hbVisOnTrack;
	hbVisOnTrack.Resize(GetNumTracks());
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		hbVisOnTrack.Get()[i-1] = false;
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (GetTrackVis(tr) & 2)
		{
			for (int j = 0; j < GetTrackNumMediaItems(tr); j++)
			{
				MediaItem* mi = GetTrackMediaItem(tr, j);
				if (*(bool*)GetSetMediaItemInfo(mi, "B_UISEL", NULL))
				{
					if (y1 == -1)
						y1 = i;
					y2 = i;
					hbVisOnTrack.Get()[i-1] = true;
					break;
				}
			}
		}
		if (iOthers == 2 && !hbVisOnTrack.Get()[i-1])
			SetTrackVis(tr, GetTrackVis(tr) & 1);
	}

	if (y1 == -1)
		return;

	// Hide tracks from TCP only after making sure there's actually something selected 
	if (iOthers == 2)
		for (int i = 1; i <= GetNumTracks(); i++)
			if (!hbVisOnTrack.Get()[i-1])
			{
				MediaTrack* tr = CSurf_TrackFromID(i, false);
				SetTrackVis(tr, GetTrackVis(tr) & 1);
			}

	VertZoomRange(y1, 1+y2-y1, hbVisOnTrack.Get()+y1-1, iOthers == 1);
	SaveZoomSlice(true);
}

void HorizZoomSelItems(bool bTimeSel = false)
{
	HWND hTrackView = GetTrackWnd();
	if (!hTrackView)
		return;

	RECT r;
	GetClientRect(hTrackView, &r);

	double x1, x2;
	GetSet_LoopTimeRange(false, false, &x1, &x2, false);
	if (!bTimeSel || x1 == x2)
	{
		// Find the coordinates of the first and last selected item
		x1 = DBL_MAX; x2 = -DBL_MAX;
		for (int i = 1; i <= GetNumTracks(); i++)
		{
			MediaTrack* tr = CSurf_TrackFromID(i, false);
			if (GetTrackVis(tr) & 2)
				for (int j = 0; j < GetTrackNumMediaItems(tr); j++)
				{
					MediaItem* mi = GetTrackMediaItem(tr, j);
					if (*(bool*)GetSetMediaItemInfo(mi, "B_UISEL", NULL))
					{
						double d = *(double*)GetSetMediaItemInfo(mi, "D_POSITION", NULL);
						if (d < x1)
							x1 = d;
						d += *(double*)GetSetMediaItemInfo(mi, "D_LENGTH", NULL);
						if (d > x2)
							x2 = d;
					}
				}
		}

		if (x1 == DBL_MAX)
			return;
	}

	adjustZoom(0.94 * r.right / (x2 - x1), 1, false, -1);
	SetHorizPos(hTrackView, x1, 0.03);
	SaveZoomSlice(true);
}

void CursorTo10(COMMAND_T* = NULL)
{
	SetHorizPos(GetTrackWnd(), GetCursorPosition(), 0.10);
}

void CursorTo50(COMMAND_T* = NULL)
{
	SetHorizPos(GetTrackWnd(), GetCursorPosition(), 0.50);
}

void PCursorTo50(COMMAND_T* = NULL)
{
	SetHorizPos(GetTrackWnd(), GetPlayPosition(), 0.50);
}

void HorizScroll(COMMAND_T* ctx)
{
	HWND hwnd = GetTrackWnd();
	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(hwnd, SB_HORZ, &si);
	si.nPos += (int)((double)ctx->user * si.nPage / 100.0);
	if (si.nPos < 0) si.nPos = 0;
	else if (si.nPos > si.nMax) si.nPos = si.nMax;
	CoolSB_SetScrollInfo(hwnd, SB_HORZ, &si, true);
	SendMessage(hwnd, WM_HSCROLL, SB_THUMBPOSITION, NULL);
}

void ZoomToSelItems(COMMAND_T* = NULL)		{ VertZoomSelItems(0); HorizZoomSelItems(); }
void ZoomToSelItemsMin(COMMAND_T* = NULL)	{ VertZoomSelItems(1);  HorizZoomSelItems(); }
void VZoomToSelItems(COMMAND_T* = NULL)		{ VertZoomSelItems(0); }
void VZoomToSelItemsMin(COMMAND_T* = NULL)	{ VertZoomSelItems(1); }
void HZoomToSelItems(COMMAND_T* = NULL)		{ HorizZoomSelItems(); }
void ZoomToSIT(COMMAND_T* = NULL)			{ VertZoomSelItems(0); HorizZoomSelItems(true); }
void ZoomToSITMin(COMMAND_T* = NULL)		{ VertZoomSelItems(1); HorizZoomSelItems(true); }
void FitSelTracks(COMMAND_T* = NULL)		{ VertZoomSelTracks(0); }
void FitSelTracksMin(COMMAND_T* = NULL)		{ VertZoomSelTracks(1); }

class ArrangeState
{
private:
	double dHZoom;
	int iVZoom;
	int iXPos;
	int iYPos;
	WDL_TypedBuf<int>  hbTrackHeights;
	WDL_TypedBuf<int>  hbTrackVis;
	WDL_TypedBuf<int>  hbEnvHeights;
	WDL_TypedBuf<bool> hbEnvVis;
	bool m_bHoriz;
	bool m_bVert;

public:
	ArrangeState():hbTrackHeights(256),hbTrackVis(256),hbEnvHeights(256),hbEnvVis(256)  { Clear(); }
	void Clear()
	{
		dHZoom = 0.0; iXPos = 0; iYPos = 0; m_bHoriz = false; m_bVert = false;
		hbTrackHeights.Resize(0, false);
		hbTrackVis.Resize(0, false);
		hbEnvHeights.Resize(0, false);
		hbEnvVis.Resize(0, false);
	}
	void Save(bool bSaveHoriz, bool bSaveVert)
	{
		HWND hTrackView = GetTrackWnd();
		if (!hTrackView)
			return;

		m_bHoriz = bSaveHoriz;
		m_bVert  = bSaveVert;
		SCROLLINFO si = { sizeof(SCROLLINFO), };
		si.fMask = SIF_ALL;

		// Horiz
		if (m_bHoriz)
		{
			dHZoom = GetHZoomLevel();
			CoolSB_GetScrollInfo(hTrackView, SB_HORZ, &si);
			iXPos = si.nPos;
		}

		// Vert
		if (m_bVert)
		{
			iVZoom = *(int*)GetConfigVar("vzoom2");
			hbTrackHeights.Resize(GetNumTracks()+1, false);
			hbTrackVis.Resize(GetNumTracks()+1, false);
			hbEnvHeights.Resize(0, false);
			hbEnvVis.Resize(0, false);
			for (int i = 0; i <= GetNumTracks(); i++)
			{
				MediaTrack* tr = CSurf_TrackFromID(i, false);
				hbTrackHeights.Get()[i] = *(int*)GetSetMediaTrackInfo(tr, "I_HEIGHTOVERRIDE", NULL);
				hbTrackVis.Get()[i] = GetTrackVis(tr);
				int iNumEnvelopes = CountTrackEnvelopes(tr);
				if (iNumEnvelopes)
				{
					int iEnv = hbEnvHeights.GetSize();
					hbEnvHeights.Resize(iEnv + iNumEnvelopes);
					hbEnvVis.Resize(iEnv + iNumEnvelopes);
					for (int j = 0; j < iNumEnvelopes; j++)
					{
						SWS_TrackEnvelope te(GetTrackEnvelope(tr, j));
						hbEnvHeights.Get()[iEnv] = te.GetHeight(0);
						hbEnvVis.Get()[iEnv] = te.GetVis();
						iEnv++;
					}
				}
			}
			CoolSB_GetScrollInfo(hTrackView, SB_VERT, &si);
			iYPos = si.nPos;
		}
	}

	void Restore()
	{
		HWND hTrackView = GetTrackWnd();
		if (!hTrackView || !(m_bHoriz || m_bVert))
			return;

		// Horiz zoom
		if (m_bHoriz)
			adjustZoom(dHZoom, 1, false, -1);

		// Vert zoom
		if (m_bVert)
		{
			*(int*)GetConfigVar("vzoom2") = iVZoom;
			int iSaved = hbTrackHeights.GetSize();
			int iEnvPtr = 0;
			for (int i = 0; i <= GetNumTracks(); i++)
			{
				MediaTrack* tr = CSurf_TrackFromID(i, false);
				if (i < iSaved)
				{
					SetTrackVis(tr, hbTrackVis.Get()[i]);
					GetSetMediaTrackInfo(tr, "I_HEIGHTOVERRIDE", hbTrackHeights.Get()+i);
				}
				else
					GetSetMediaTrackInfo(tr, "I_HEIGHTOVERRIDE", &g_i0);

				for (int j = 0; j < CountTrackEnvelopes(tr); j++)
				{
					SWS_TrackEnvelope te(GetTrackEnvelope(tr, j));
					if (iEnvPtr < hbEnvHeights.GetSize())
					{
						te.SetVis(hbEnvVis.Get()[iEnvPtr]);
						te.SetHeight(hbEnvHeights.Get()[iEnvPtr]);
						iEnvPtr++;
					}
					else
						te.SetHeight(0);
				}
			}

			TrackList_AdjustWindows(false);
			UpdateTimeline();

			SetVertPos(hTrackView, iYPos, true);
		}

		if (m_bHoriz)
			SetHorizPos(hTrackView, (double)iXPos / dHZoom);

		SaveZoomSlice(true);
	}
};

static SWSProjConfig<ArrangeState> g_stdAS;
static SWSProjConfig<ArrangeState> g_togAS;
static bool g_bASToggled = false;

// iType == 0 tracks, 1 items or time sel, 2 just items, 3 just horiz; iOthers == 0 nothing, 1 minimize, 2 hide from TCP
void TogZoom(int iType, int iOthers)
{
	if (g_bASToggled)
	{
		g_togAS.Get()->Restore();
		g_bASToggled = false;
		return;
	}

	g_bASToggled = true;
	g_togAS.Get()->Save(true, iType != 3);

	if (iType == 0)
	{
		Main_OnCommand(40031, 0);
		VertZoomSelTracks(iOthers);
	}
	else
	{
		double d1, d2;
		GetSet_LoopTimeRange(false, false, &d1, &d2, false);
		if (d1 != d2 && (iType == 1 || iType == 3))
			Main_OnCommand(40031, 0);
		else
			HorizZoomSelItems();
		if (iType != 3)
			VertZoomSelItems(iOthers);
	}
}

void TogZoomTT(COMMAND_T* = NULL)			{ TogZoom(0, 0); }
void TogZoomTTMin(COMMAND_T* = NULL)		{ TogZoom(0, 1); }
void TogZoomTTHide(COMMAND_T* = NULL)		{ TogZoom(0, 2); }
void TogZoomItems(COMMAND_T* = NULL)		{ TogZoom(1, 0); }
void TogZoomItemsMin(COMMAND_T* = NULL)		{ TogZoom(1, 1); }
void TogZoomItemsHide(COMMAND_T* = NULL)	{ TogZoom(1, 2); }
void TogZoomItemsOnly(COMMAND_T* = NULL)	{ TogZoom(2, 0); }
void TogZoomItemsOnlyMin(COMMAND_T* = NULL)	{ TogZoom(2, 1); }
void TogZoomItemsOnlyHide(COMMAND_T* = NULL){ TogZoom(2, 2); }
void TogZoomHoriz(COMMAND_T* = NULL)		{ TogZoom(3, 0); }
void SaveArngView(COMMAND_T* = NULL)		{ g_stdAS.Get()->Save(true, true); }
void RestoreArngView(COMMAND_T* = NULL)		{ g_stdAS.Get()->Restore(); }

bool g_bSmoothScroll = false;
void TogSmoothScroll(COMMAND_T*)	{ g_bSmoothScroll = !g_bSmoothScroll; }
bool IsSmoothScroll(COMMAND_T*)		{ return g_bSmoothScroll; }

// Returns the track at a point on the track view window
// Point is in client coords
MediaTrack* TrackAtPoint(HWND hTrackView, int iY, int* iOffset, int* iYMin, int* iYMax)
{
	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(hTrackView, SB_VERT, &si);

	int iVPos = -si.nPos; // Account for current scroll pos
	int iTrack = 0;
	int iTrackH = 0;
	
	// Find the current track #
	while (iTrack <= GetNumTracks())
	{
		iTrackH = *(int*)GetSetMediaTrackInfo(CSurf_TrackFromID(iTrack, false), "I_WNDH", NULL);
		if (iVPos + iTrackH > iY)
			break;
		iVPos += iTrackH;
		iTrack++;
	}

	// Set extents if outside of std region
	if (iYMin)
		*iYMin = iVPos;
	if (iYMax)
		*iYMax = iVPos;

	if (iTrack <= GetNumTracks())
	{
		if (iYMin)
			*iYMin = iVPos;
		if (iYMax)
			*iYMax = iVPos + iTrackH;
		if (iOffset)
			*iOffset = iY - iVPos;
		return CSurf_TrackFromID(iTrack, false);
	}
	return NULL;
}

// Returns the (first) item at the point p in the trackview.
// p is in client coords; rExtents can extend well past the ClientRect
MediaItem* ItemAtPoint(HWND hTrackView, POINT p, RECT* rExtents, MediaTrack** pTr)
{
	if (rExtents) rExtents->left = rExtents->right = 0; // Init rExtents

	// First get the track
	int iY1, iY2;
	MediaTrack* tr = TrackAtPoint(hTrackView, p.y, NULL, &iY1, &iY2);
	if (pTr)
		*pTr = tr;
	if (rExtents)
	{
		rExtents->top = iY1;
		rExtents->bottom = iY2;
	}
	if (!tr)
		return NULL;

	// Convert x coord to time
	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(hTrackView, SB_HORZ, &si); // Get the current scroll pos
	RECT r;
	GetClientRect(hTrackView, &r);	// Get the window width
	double dPos = (p.x + si.nPos) / GetHZoomLevel();

	// Then, maybe find an item
	for (int i = 0; i < GetTrackNumMediaItems(tr); i++)
	{
		MediaItem* mi = GetTrackMediaItem(tr, i);
		double dStart = *(double*)GetSetMediaItemInfo(mi, "D_POSITION", NULL);
		double dEnd   = *(double*)GetSetMediaItemInfo(mi, "D_LENGTH", NULL) + dStart;
		if (dPos >= dStart && dPos <= dEnd)
		{
			if (rExtents)
			{
				rExtents->left  = (int)(GetHZoomLevel() * dStart + 0.5) - si.nPos;
				rExtents->right = (int)(GetHZoomLevel() * dEnd + 0.5) - si.nPos;
			}
			return mi;
		}
	}
	return NULL;
}

// Class for saving/restoring the zoom state.  This is a lighter-weight version
// of the arrange state class above.
// To be perfect you need to GetSetObjectState (for envelope info) which is extremely costly with some plugs
// This class ignores envelope stuffs and other things at the risk of the restore being "off" if tracks
// are added, deleted, or if envs are added, deleted, sizes changed, etc.  It does support track height however.
class ZoomState
{
private:
	// Zoom
	WDL_TypedBuf<int> m_iTrackHeights;
	double m_dHZoom;
	int m_iVZoom;
	bool m_bProjExtents;

	// Pos
	MediaTrack* m_trVPos;
	int m_iVPos, m_iHPos;

public:
	ZoomState():m_iTrackHeights(256),m_dHZoom(0.0),m_iVZoom(0),m_iVPos(0),m_iHPos(0),m_trVPos(NULL),m_bProjExtents(false) {}
	void SaveZoom()
	{
		// Save the track heights
		m_iTrackHeights.Resize(0, false);
		int* pHeights = m_iTrackHeights.Resize(GetNumTracks()+1); // +1 for master
		for (int i = 0; i <= GetNumTracks(); i++)
			pHeights[i] = *(int*)GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_HEIGHTOVERRIDE", NULL);

		m_dHZoom = GetHZoomLevel();
		m_iVZoom = *(int*)GetConfigVar("vzoom2");
		m_bProjExtents = false;
	}
	void ZoomToProject()
	{
		VertZoomRange(0, GetNumTracks()+1, NULL, false);
		Main_OnCommand(40295, 0);
		SaveZoom();
		SavePos();
		m_bProjExtents = true;
	}
	bool IsZoomedToProject() { return m_bProjExtents; }
	void SavePos()
	{
		HWND hTrackView = GetTrackWnd();
		if (!hTrackView)
			return;

		m_trVPos = TrackAtPoint(hTrackView, 0, &m_iVPos, NULL, NULL);

		SCROLLINFO si = { sizeof(SCROLLINFO), };
		si.fMask = SIF_ALL;
		CoolSB_GetScrollInfo(hTrackView, SB_HORZ, &si);
		m_iHPos = si.nPos;
	}

	void Restore()
	{
		if (m_bProjExtents)
			ZoomToProject();

		HWND hTrackView = GetTrackWnd();
		if (!hTrackView || m_dHZoom == 0.0 && m_iVZoom == 0)
			return;

		adjustZoom(m_dHZoom, 1, false, -1);
		*(int*)GetConfigVar("vzoom2") = m_iVZoom;

		// Restore track heights, ignoring the fact that tracks could have been added/removed
		for (int i = 0; i < m_iTrackHeights.GetSize() && i <= GetNumTracks(); i++)
			GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_HEIGHTOVERRIDE", &(m_iTrackHeights.Get()[i]));

		TrackList_AdjustWindows(false);
		UpdateTimeline();

		// Restore positions
		int iTrack = CSurf_TrackToID(m_trVPos, false);
		if (iTrack > 0)
			SetVertPos(hTrackView, iTrack, false, m_iVPos);
		SetHorizPos(hTrackView, (double)m_iHPos / m_dHZoom);
		SaveZoomSlice(true);
	}

	bool IsZoomEqual(ZoomState* zs)
	{	// Ignores the horiz/vert positions!
		if (zs->m_dHZoom != m_dHZoom || zs->m_iVZoom != m_iVZoom)
			return false;
		if (zs->m_iTrackHeights.GetSize() != m_iTrackHeights.GetSize())
			return false;
		for (int i = 0; i < m_iTrackHeights.GetSize(); i++)
			if (zs->m_iTrackHeights.Get()[i] != m_iTrackHeights.Get()[i])
				return false;
		
		return true;
	}
	// Debug
	//void Print(int i)
	//{
	//	dprintf("ZoomState %d: H: %.2f @ %d, V: %d @ %d, Proj: %s\n", i, m_dHZoom, m_iHPos, m_iVZoom, m_iVPos, m_bProjExtents ? "yes" : "no");
	//}
};

static SWSProjConfig<WDL_PtrList<ZoomState> > g_zoomStack;
static SWSProjConfig<int> g_zoomLevel;

// Undo zoom prefs
static bool g_bUndoSWSOnly = false;
static bool g_bLastUndoProj = false;

// Save the current zoom state, called from the slice
void SaveZoomSlice(bool bSWS)
{
	bool bNewZoom = false;

	// Do the initialization of g_zoomLevel, as project changes can result in new PtrLists
	if (g_zoomStack.Get()->GetSize() == 0)
		*g_zoomLevel.Get() = 0;

	static ZoomState* zs = NULL;
	if (!zs)
		zs = new ZoomState;

	zs->SaveZoom();

	if (g_zoomStack.Get()->GetSize() == 0 || !g_zoomStack.Get()->Get(*g_zoomLevel.Get())->IsZoomEqual(zs))
	{	// The zoom level has been changed by the user!
		if (bSWS || !g_bUndoSWSOnly || g_zoomStack.Get()->GetSize() == 0)
		{
			while (*g_zoomLevel.Get())
			{
				g_zoomStack.Get()->Delete(0, true);
				*g_zoomLevel.Get() -= 1;
			}

			// Trim the bottom of the stack if it's huge.
			if (g_zoomStack.Get()->GetSize() == 50)
				g_zoomStack.Get()->Delete(49, true);

			g_zoomStack.Get()->Insert(0, zs);
		}
		else
		{
			delete g_zoomStack.Get()->Get(*g_zoomLevel.Get());
			g_zoomStack.Get()->Set(*g_zoomLevel.Get(), zs);
		}

		zs = NULL;
	}
	
	// Save the position for every call
	if (g_zoomStack.Get()->GetSize())
		g_zoomStack.Get()->Get(*g_zoomLevel.Get())->SavePos();
}

void UndoZoom(COMMAND_T* = NULL)
{
	if (*g_zoomLevel.Get() + 1 < g_zoomStack.Get()->GetSize())
	{
		*g_zoomLevel.Get() += 1;
		g_zoomStack.Get()->Get(*g_zoomLevel.Get())->Restore();
	}
	else if (g_bLastUndoProj)
	{
		if (!g_zoomStack.Get()->GetSize() || !g_zoomStack.Get()->Get(*g_zoomLevel.Get())->IsZoomedToProject())
		{
			*g_zoomLevel.Get() += 1;
			g_zoomStack.Get()->Add(new ZoomState);
		}
		g_zoomStack.Get()->Get(g_zoomStack.Get()->GetSize()-1)->ZoomToProject();
	}
}

void RedoZoom(COMMAND_T*)
{
	if (*g_zoomLevel.Get() > 0)
	{
		*g_zoomLevel.Get() -= 1;
		g_zoomStack.Get()->Get(*g_zoomLevel.Get())->Restore();
	}
}

void CreateZoomRect(HWND h, RECT* newR, POINT* p1, POINT* p2)
{
	RECT r;
	if (p1->x < p2->x)	{ r.left = p1->x; r.right = p2->x; }
	else				{ r.left = p2->x; r.right = p1->x; }
	if (p1->y < p2->y)	{ r.top = p1->y; r.bottom = p2->y; }
	else				{ r.top = p2->y; r.bottom = p1->y; }
	RECT clientR;
	GetClientRect(h, &clientR);
	IntersectRect(newR, &r, &clientR);
}

// Zoom into a given rectangle on Reaper's track view 
// If dX1 != dX2, use those as times in s to horiz zoom to
void ZoomToRect(HWND hTrackView, RECT* rZoom, double dX1, double dX2)
{
	RECT r;
	GetClientRect(hTrackView, &r); // Need for width of view wnd

	if (dX1 != dX2)
	{	// Use these doubles as the x points (more accurate for item zooms!)
		//  Add 3% on each side for some "breathing" room, just like in Reaper zooms
		adjustZoom(0.94 * (r.right - r.left) / (dX2 - dX1), 1, false, -1);
		SetHorizPos(hTrackView, dX1, 0.03);
	}
	else if (rZoom->left != rZoom->right)
	{	// Convert the rect left/right points to pixels, where 0 is the start of the project.
		// These are same units as the scroll bar.
		int iX1, iX2;
		SCROLLINFO si = { sizeof(SCROLLINFO), };
		si.fMask = SIF_ALL;
		CoolSB_GetScrollInfo(hTrackView, SB_HORZ, &si); // Get the current scroll pos
		iX1 = rZoom->left + si.nPos;
		dX1 = (double)iX1 / GetHZoomLevel();
		iX2 = rZoom->right + si.nPos; 
		adjustZoom(GetHZoomLevel() * r.right / (iX2 - iX1), 1, false, -1);
		SetHorizPos(hTrackView, dX1);
	}

	// Now, find the track(s) at the rect top/bottom and zoom in
	if (rZoom->top != rZoom->bottom)
	{
		MediaTrack* tr = TrackAtPoint(hTrackView, rZoom->top, NULL, NULL, NULL);
		if (tr)
		{	// If no track returned, user zoomed over the "dead" area at the bottom
			int iFirst = CSurf_TrackToID(tr, false);
			int iNum;
			tr = TrackAtPoint(hTrackView, rZoom->bottom-1, NULL, NULL, NULL);
			if (tr)
				iNum = CSurf_TrackToID(tr, false) - iFirst + 1;
			else
				iNum = GetNumTracks() - iFirst + 1;
			VertZoomRange(iFirst, iNum, NULL, false);
		}
	}
	SaveZoomSlice(true);
}

static bool g_bZooming = false;
static WNDPROC g_ReaperTrackWndProc = NULL;
static HCURSOR g_hZoomCur = NULL;

// Zoom tool Prefs
// (defaults are actually set in ZoomInit)
static bool g_bMidMouseButton = false;
static int  g_iMidMouseModifier = 0;
static bool g_bItemZoom = false;
static bool g_bUndoZoom = false;
static bool g_bUnzoomMode = false;

LRESULT CALLBACK ZoomWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN32
	static RECT rDraw;
#endif
	static RECT rZoom;
	static MediaItem* itemZoom;

	if (uMsg == WM_START_ZOOM || (g_bMidMouseButton && uMsg == WM_MBUTTONDOWN && SWS_GetModifiers() == g_iMidMouseModifier))
	{
		POINT p;
		RECT r;
		GetCursorPos(&p);
		GetWindowRect(hwnd, &r);
		if (PtInRect(&r, p) && g_hZoomCur)
			SetCursor(g_hZoomCur);
		g_bZooming = true;
		RefreshToolbar(SWSGetCommandID(ZoomTool));
		itemZoom = NULL;
		rZoom.left = 0; rZoom.top = 0; rZoom.right = 0; rZoom.bottom = 0;
#ifdef _WIN32
		rDraw.left = 0; rDraw.top = 0; rDraw.right = 0; rDraw.bottom = 0;
#endif
	}
	if (g_bZooming)
	{
#ifdef _WIN32
		static LICE_SysBitmap* bmStd = NULL;
#else
		static void* pFocusRect = NULL;
#endif
		static POINT pStart;
		static bool bMBDown = false;

		if (uMsg == WM_LBUTTONDOWN || (g_bMidMouseButton && uMsg == WM_MBUTTONDOWN && SWS_GetModifiers() == g_iMidMouseModifier))
		{
			GetCursorPos(&pStart);
			ScreenToClient(hwnd, &pStart);
			SetCapture(hwnd);
			bMBDown = true;
		}
		else if (uMsg == WM_SETCURSOR)
		{
			if (GetCursor() != g_hZoomCur && g_hZoomCur)
				SetCursor(g_hZoomCur);
			return 1;
		}
#ifdef _WIN32
		else if (uMsg == WM_PAINT)
		{
			// If there's a paint request then let std wnd handle, then get the data
			g_ReaperTrackWndProc(hwnd, uMsg, wParam, lParam);
			delete bmStd;
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(hwnd, &ps);
			RECT r;
			GetClientRect(hwnd, &r);
			bmStd = new LICE_SysBitmap(r.right - r.left, r.bottom - r.top);
			BitBlt(bmStd->getDC(), 0, 0, bmStd->getWidth(), bmStd->getHeight(), dc, 0, 0, SRCCOPY);
			EndPaint(hwnd, &ps);
			return 0;
		}
#endif
		if (uMsg == WM_MOUSEMOVE || uMsg == WM_LBUTTONDOWN || (g_bMidMouseButton && uMsg == WM_MBUTTONDOWN && SWS_GetModifiers() == g_iMidMouseModifier) || uMsg == WM_START_ZOOM)
		{
			// There's a lot of rects here:
			// rBox - the drawn rectangle (win only)
			// rZoom - the area you're zooming into, also the shaded area
			// rDraw - a union of rBox and rZoom to get the full draw area.
			//         The union takes care of outside of track area case.
			//         Also U the old draw area to erase old stuff properly
			// rLastDraw - previous mouse move draw area
			// finally r, rDraw U rLastDraw, to erase the old and add some new

			static RECT rBox;
			POINT p;
			GetCursorPos(&p);
			ScreenToClient(hwnd, &p);
			if (!bMBDown)
				pStart = p;
			CreateZoomRect(hwnd, &rBox, &pStart, &p);

			rZoom = rBox; // Zoom area starts as the drawn box
			itemZoom = NULL;
			
			// Check for really small move, and do item zoom mode if so
			if (rBox.right - rBox.left <= 2 && rBox.bottom - rBox.top <= 2)
			{
				if (g_bItemZoom)
				{	// Change zoom area to match item
					itemZoom = ItemAtPoint(hwnd, p, &rZoom, NULL);
					if (itemZoom)
					{
						rZoom.left++;
						rZoom.right++;
					}
					else
						rZoom.top = rZoom.bottom;
				}
			}
			else
			{
				int y;
				TrackAtPoint(hwnd, rZoom.top, NULL, &y, NULL);
				rZoom.top = y;
				TrackAtPoint(hwnd, rZoom.bottom, NULL, NULL, &y);
				rZoom.bottom = y;
				rZoom.right++;
			}

#ifdef _WIN32			
			HDC dc = GetDC(hwnd);
			if (!dc)
				return 0;
			
			// Get the entire screen
			if (!bmStd)
			{
				RECT r;
				GetClientRect(hwnd, &r);
				bmStd = new LICE_SysBitmap(r.right - r.left, r.bottom - r.top);
				BitBlt(bmStd->getDC(), 0, 0, bmStd->getWidth(), bmStd->getHeight(), dc, 0, 0, SRCCOPY);
			}
			
			// Are we in "unzoom" mode?
			if (g_bUnzoomMode && pStart.y - p.y > 2)
			{
				// Adjust "rBox" to be around the mini-view
				RECT rMini;
				rMini.left = pStart.x - bmStd->getWidth() / 20;
				rMini.right = rMini.left + bmStd->getWidth() / 10;
				rMini.top = pStart.y - bmStd->getHeight() / 20;
				rMini.bottom = rMini.top + bmStd->getHeight() / 10;
				UnionRect(&rBox, &rBox, &rMini);
				rBox.bottom += rMini.top - rBox.top;
				rBox.right += rMini.left - rBox.left;

				GetClientRect(hwnd, &rDraw);

				rZoom.left = (rBox.left - rMini.left) * 10;
				rZoom.right = bmStd->getWidth() + (rBox.right - rMini.right) * 10;
				rZoom.top = (rBox.top - rMini.top) * 10;
				rZoom.bottom = bmStd->getHeight() + (rBox.bottom - rMini.bottom) * 10;

				LICE_SysBitmap bm(bmStd->getWidth(), bmStd->getHeight());
				LICE_Copy(&bm, bmStd);				
				LICE_FillRect(&bm, 0, 0, bm.getWidth(), bm.getHeight(), LICE_RGBA(0, 0, 0, 255), 0.7f, LICE_BLIT_MODE_COPY); 
				LICE_ScaledBlit(&bm, bmStd, rMini.left, rMini.top, rMini.right - rMini.left, rMini.bottom - rMini.top, 0.0, 0.0, (float)bmStd->getWidth(), (float)bmStd->getHeight(), 1.0, LICE_BLIT_MODE_COPY);
				LICE_DrawRect(&bm, rBox.left, rBox.top, rBox.right - rBox.left - 1, rBox.bottom - rBox.top - 1, LICE_RGBA(255, 255, 255, 150), 1.0, LICE_BLIT_MODE_COPY);
				BitBlt(dc, 0, 0, bm.getWidth(), bm.getHeight(), bm.getDC(), 0, 0, SRCCOPY);

				// Now that we're done drawing, adjust rZoom to 0 height if necessary
				// This is to avoid unintentional vert zoom per mbn http://code.google.com/p/sws-extension/issues/detail?id=178#c14
				if (rMini.top == rBox.top && rMini.bottom == rBox.bottom)
					rZoom.top = rZoom.bottom;
			}
			else
			{
				RECT rLastDraw = rDraw;
				UnionRect(&rDraw, &rBox, &rZoom);
				UnionRect(&rDraw, &rDraw, &rLastDraw);
				
				LICE_SysBitmap bm(rDraw.right - rDraw.left + 1, rDraw.bottom - rDraw.top + 1);
				LICE_Blit(&bm, bmStd, 0, 0, rDraw.left, rDraw.top, bm.getWidth(), bm.getHeight(), 1.0, LICE_BLIT_MODE_COPY);
				
				// Don't draw small boxes
				if (rZoom.right - rZoom.left > 1 && rZoom.bottom - rZoom.top > 1)
					LICE_FillRect(&bm, rZoom.left - rDraw.left, rZoom.top - rDraw.top, rZoom.right - rZoom.left, rZoom.bottom - rZoom.top, LICE_RGBA(128, 128, 110, 150), 1.0, LICE_BLIT_MODE_HSVADJ); 
				if (rBox.right - rBox.left > 1 && rBox.bottom - rBox.top > 1)
					LICE_DrawRect(&bm, rBox.left - rDraw.left, rBox.top - rDraw.top, rBox.right - rBox.left - 1, rBox.bottom - rBox.top - 1, LICE_RGBA(255, 255, 255, 150), 1.0, LICE_BLIT_MODE_COPY);
				BitBlt(dc, rDraw.left, rDraw.top, bm.getWidth(), bm.getHeight(), bm.getDC(), 0, 0, SRCCOPY);
			}
		
			ReleaseDC(hwnd, dc);
#else
			RECT r, rDraw = rZoom;
			GetClientRect(hwnd, &r);
			if (rDraw.left   < r.left)   rDraw.left   = r.left;  // You'd think IntersectRect would work here, but it doesnt for some reason.
			if (rDraw.right  > r.right)  rDraw.right  = r.right;
			if (rDraw.top    < r.top)    rDraw.top    = r.top;
			if (rDraw.bottom > r.bottom) rDraw.bottom = r.bottom;
			SWELL_DrawFocusRect(hwnd, &rDraw, &pFocusRect);
#endif
			return 0;
		}
		// Done with the zoom
		else if (uMsg == WM_LBUTTONUP || uMsg == WM_RBUTTONUP || uMsg == WM_MBUTTONUP || uMsg == WM_CANCEL_ZOOM)
		{
			if (GetCapture() == hwnd)
				ReleaseCapture();

#ifdef _WIN32
			delete bmStd;
			bmStd = NULL;
#else
			SWELL_DrawFocusRect(hwnd, NULL, &pFocusRect);
#endif
			g_bZooming = false;
			SendMessage(hwnd, WM_SETCURSOR, (WPARAM)hwnd, 0);
			RefreshToolbar(SWSGetCommandID(ZoomTool));

			if (bMBDown && (uMsg == WM_LBUTTONUP || (g_bMidMouseButton && uMsg == WM_MBUTTONUP)))
			{
				if (itemZoom)
				{
					double dX1 = *(double*)GetSetMediaItemInfo(itemZoom, "D_POSITION", NULL);
					double dX2 = *(double*)GetSetMediaItemInfo(itemZoom, "D_LENGTH", NULL) + dX1;
					ZoomToRect(hwnd, &rZoom, dX1, dX2);
				}
				else if (rZoom.right - rZoom.left > 1 || rZoom.bottom - rZoom.top > 1)
					ZoomToRect(hwnd, &rZoom, 0.0, 0.0);
				else if (g_bUndoZoom)
					UndoZoom();
				else
					UpdateTimeline();
			}
			else
				UpdateTimeline();

			bMBDown = false;
			return 0;
		}
	}

	return g_ReaperTrackWndProc(hwnd, uMsg, wParam, lParam);
}

void ZoomTool(COMMAND_T*)
{
	HWND hTrackView = GetTrackWnd();
	if (!hTrackView)
		return;

	// Handle start/cancel from the wnd proc to keep things in one place
	if (!g_bZooming)
		SendMessage(hTrackView, WM_START_ZOOM, 0, 0);
	else
		SendMessage(hTrackView, WM_CANCEL_ZOOM, 0, 0);
}

bool IsZoomMode(COMMAND_T*)
{
	return g_bZooming;
}

#define ZOOMPREFS_WNDPOS_KEY "ZoomPrefs WndPos"
static INT_PTR WINAPI ZoomPrefsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			CheckDlgButton(hwndDlg, IDC_MMZOOM, g_bMidMouseButton);
			CheckDlgButton(hwndDlg, IDC_ITEMZOOM, g_bItemZoom);
			CheckDlgButton(hwndDlg, IDC_UNDOZOOM, g_bUndoZoom);
#ifdef _WIN32
			CheckDlgButton(hwndDlg, IDC_UNZOOMMODE, g_bUnzoomMode);
#else
			ShowWindow(GetDlgItem(hwndDlg, IDC_UNZOOMMODE), SW_HIDE);
#endif
			CheckDlgButton(hwndDlg, IDC_UNDOSWSONLY, g_bUndoSWSOnly);
			CheckDlgButton(hwndDlg, IDC_LASTUNDOPROJ, g_bLastUndoProj);
			for (int i = 0; i < NUM_MODIFIERS; i++)
				SendMessage(GetDlgItem(hwndDlg, IDC_MMMODIFIER), CB_ADDSTRING, 0, (LPARAM)g_modifiers[i].cDesc);
			for (int i = 0; i < NUM_MODIFIERS; i++)
				if (g_iMidMouseModifier == g_modifiers[i].iModifier)
				{
					SendMessage(GetDlgItem(hwndDlg, IDC_MMMODIFIER), CB_SETCURSEL, (WPARAM)i, 0);
					break;
				}
			EnableWindow(GetDlgItem(hwndDlg, IDC_MMMODIFIER), g_bMidMouseButton);

			RestoreWindowPos(hwndDlg, ZOOMPREFS_WNDPOS_KEY, false);
			return 0;
		}
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDC_MMZOOM:
					EnableWindow(GetDlgItem(hwndDlg, IDC_MMMODIFIER), IsDlgButtonChecked(hwndDlg, IDC_MMZOOM) == BST_CHECKED);
					break;

				case IDOK:
				{
					g_bMidMouseButton = IsDlgButtonChecked(hwndDlg, IDC_MMZOOM) == BST_CHECKED;
					g_bItemZoom       = IsDlgButtonChecked(hwndDlg, IDC_ITEMZOOM) == BST_CHECKED;
					g_bUndoZoom       = IsDlgButtonChecked(hwndDlg, IDC_UNDOZOOM) == BST_CHECKED;
#ifdef _WIN32
					g_bUnzoomMode     = IsDlgButtonChecked(hwndDlg, IDC_UNZOOMMODE) == BST_CHECKED;
#endif
					g_bUndoSWSOnly    = IsDlgButtonChecked(hwndDlg, IDC_UNDOSWSONLY) == BST_CHECKED;
					g_bLastUndoProj   = IsDlgButtonChecked(hwndDlg, IDC_LASTUNDOPROJ) == BST_CHECKED;
					g_iMidMouseModifier = g_modifiers[SendMessage(GetDlgItem(hwndDlg, IDC_MMMODIFIER), CB_GETCURSEL, 0, 0)].iModifier;
				}
				// Fall through to cancel to save/end
				case IDCANCEL:
					SaveWindowPos(hwndDlg, ZOOMPREFS_WNDPOS_KEY);
					EndDialog(hwndDlg, 0);
					break;
			}
			break;
	}
	return 0;
}


void ZoomPrefs(COMMAND_T*)
{
	DialogBox(g_hInst,MAKEINTRESOURCE(IDD_ZOOMPREFS), g_hwndParent, ZoomPrefsProc);
}

static bool ProcessExtensionLine(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	return false;
}
static void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
}

static void BeginLoadProjectState(bool isUndo, struct project_config_extension_t *reg)
{
	g_stdAS.Get()->Clear();
	g_stdAS.Cleanup();
	g_togAS.Get()->Clear();
	g_togAS.Cleanup();
	g_bASToggled = false;
	g_zoomStack.Cleanup();
}

bool IsTogZoomed(COMMAND_T*)
{
	return g_bASToggled;
}

static project_config_extension_t g_projectconfig = { ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL };

static COMMAND_T g_commandTable[] = 
{
	{ { DEFACCEL, "SWS: Set reaper window size to reaper.ini setwndsize" },			"SWS_SETWINDOWSIZE",	SetReaperWndSize,	NULL, },
	{ { DEFACCEL, "SWS: Horizontal scroll to put edit cursor at 10%" },				"SWS_HSCROLL10",		CursorTo10,			NULL, },
	{ { DEFACCEL, "SWS: Horizontal scroll to put edit cursor at 50%" },				"SWS_HSCROLL50",		CursorTo50,			NULL, },
	{ { DEFACCEL, "SWS: Horizontal scroll to put play cursor at 50%" },				"SWS_HSCROLLPLAY50",	PCursorTo50,		NULL, },
	{ { DEFACCEL, "SWS: Vertical zoom to selected track(s)" },	 					"SWS_VZOOMFIT",			FitSelTracks,		NULL, },
	{ { DEFACCEL, "SWS: Vertical zoom to selected track(s), minimize others" }, 	"SWS_VZOOMFITMIN",		FitSelTracksMin,	NULL, },
	{ { DEFACCEL, "SWS: Vertical zoom to selected items(s)" },	 					"SWS_VZOOMIITEMS",		VZoomToSelItems,	NULL, },
	{ { DEFACCEL, "SWS: Vertical zoom to selected items(s), minimize others" },		"SWS_VZOOMITEMSMIN",	VZoomToSelItemsMin,	NULL, },
	{ { DEFACCEL, "SWS: Horizontal zoom to selected items(s)" },	 				"SWS_HZOOMITEMS",		HZoomToSelItems,	NULL, },
	{ { DEFACCEL, "SWS: Zoom to selected item(s)" },				 				"SWS_ITEMZOOM",			ZoomToSelItems,		NULL, },
	{ { DEFACCEL, "SWS: Zoom to selected item(s), minimize others" },				"SWS_ITEMZOOMMIN",		ZoomToSelItemsMin,	NULL, },
	{ { DEFACCEL, "SWS: Zoom to sel item(s) or time sel" },				 			"SWS_ZOOMSIT",			ZoomToSIT,			NULL, },
	{ { DEFACCEL, "SWS: Zoom to sel item(s) or time sel, minimize others" },		"SWS_ZOOMSITMIN",		ZoomToSITMin,		NULL, },
	{ { DEFACCEL, "SWS: Toggle zoom to sel track(s) + time sel" },					"SWS_TOGZOOMTT",		TogZoomTT,			NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle zoom to sel track(s) + time sel, minimize others" },	"SWS_TOGZOOMTTMIN",		TogZoomTTMin,		NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle zoom to sel track(s) + time sel, hide others" },		"SWS_TOGZOOMTTHIDE",	TogZoomTTHide,		NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle zoom to sel items(s) or time sel" },					"SWS_TOGZOOMI",			TogZoomItems,		NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle zoom to sel items(s) or time sel, minimize other tracks" },"SWS_TOGZOOMIMIN",TogZoomItemsMin,	NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle zoom to sel items(s) or time sel, hide other tracks" },	"SWS_TOGZOOMIHIDE",	TogZoomItemsHide,	NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle zoom to sel items(s)" },								"SWS_TOGZOOMIONLY",		TogZoomItemsOnly,	NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle zoom to sel items(s), minimize other tracks" },		"SWS_TOGZOOMIONLYMIN",	TogZoomItemsOnlyMin,NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle zoom to sel items(s), hide other tracks" },			"SWS_TOGZOOMIONLYHIDE",	TogZoomItemsOnlyHide,NULL, 0, IsTogZoomed },
	{ { DEFACCEL, "SWS: Toggle horizontal zoom to sel items(s) or time sel" },		"SWS_TOGZOOMHORIZ",		TogZoomHoriz,		NULL, 0, IsTogZoomed },

	{ { DEFACCEL, "SWS: Scroll left 10%" },											"SWS_SCROLL_L10",		HorizScroll,		NULL, -10 },
	{ { DEFACCEL, "SWS: Scroll right 10%" },										"SWS_SCROLL_R10",		HorizScroll,		NULL, 10 },
	{ { DEFACCEL, "SWS: Scroll left 1%" },											"SWS_SCROLL_L1",		HorizScroll,		NULL, -1 },
	{ { DEFACCEL, "SWS: Scroll right 1%" },											"SWS_SCROLL_R1",		HorizScroll,		NULL, 1 },

	{ { DEFACCEL, "SWS: Save current arrange view" },				 				"SWS_SAVEVIEW",			SaveArngView,		NULL, },
	{ { DEFACCEL, "SWS: Restore arrange view" },				 					"SWS_RESTOREVIEW",		RestoreArngView,	NULL, },

	{ { DEFACCEL, "SWS: Toggle experimental smooth scroll" },						"SWS_SMOOTHSCROLL",		TogSmoothScroll,	NULL, 0, IsSmoothScroll },

	{ { DEFACCEL, "SWS: Undo zoom" },												"SWS_UNDOZOOM",			UndoZoom,			NULL, },
	{ { DEFACCEL, "SWS: Redo zoom" },												"SWS_REDOZOOM",			RedoZoom,			NULL, },
	{ { DEFACCEL, "SWS: Zoom tool (marquee)" },										"SWS_ZOOM",				ZoomTool,			NULL, 0, IsZoomMode },
	{ { DEFACCEL, "SWS: Zoom preferences" },										"SWS_ZOOMPREFS",		ZoomPrefs,			"SWS Zoom preferences...", },

	{ {}, LAST_COMMAND, }, // Denote end of table
};

void ZoomSlice()
{
	if (g_bSmoothScroll && GetPlayState() & 1)
		PCursorTo50();
	SaveZoomSlice(false);
}

static int translateAccel(MSG *msg, accelerator_register_t *ctx)
{
	// Catch the ESC key when zooming to cancel
	if (g_bZooming && msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE)
	{
		SendMessage(GetTrackWnd(), WM_CANCEL_ZOOM, 0, 0);
		return 1;
	}
	return 0;
}

static accelerator_register_t g_ar = { translateAccel, TRUE, NULL };

static void menuhook(const char* menustr, HMENU hMenu, int flag)
{
	if (strcmp(menustr, "Main extensions") == 0 && flag == 0)
	{
		int i = -1;
		while (g_commandTable[++i].id != LAST_COMMAND)
			if (g_commandTable[i].menuText)
				AddToMenu(hMenu, g_commandTable[i].menuText, g_commandTable[i].accel.accel.cmd);
	}
}

#define ZOOMPREFS_KEY "ZoomPrefs"
int ZoomInit()
{
	if (!plugin_register("accelerator",&g_ar))
		return 0;
	SWSRegisterCommands(g_commandTable);
	if (!plugin_register("projectconfig",&g_projectconfig))
		return 0;
	if (!plugin_register("hookcustommenu", (void*)menuhook))
		return 0;

	// Init the zoom tool
	HWND hTrackView = GetTrackWnd();
	if (hTrackView)
		g_ReaperTrackWndProc = (WNDPROC)SetWindowLongPtr(hTrackView, GWLP_WNDPROC, (LONG_PTR)ZoomWndProc);

#ifdef _WIN32
	g_hZoomCur = LoadCursor(g_hInst, MAKEINTRESOURCE(IDC_ZOOM));
#else
	g_hZoomCur = LoadCursor(g_hInst, IDC_ARROW);
#endif

	// Restore prefs
	int iPrefs = GetPrivateProfileInt(SWS_INI, ZOOMPREFS_KEY, 0, get_ini_file());
	g_bMidMouseButton = !!(iPrefs & 1);
	g_bItemZoom = !!(iPrefs & 2);
	g_bUndoZoom = !!(iPrefs & 4);
	g_bUnzoomMode = !!(iPrefs & 8);
	g_bUndoSWSOnly = !!(iPrefs & 16);
	g_bLastUndoProj = !!(iPrefs & 32);
	g_iMidMouseModifier = iPrefs >> 8;

	return 1;
}

void ZoomExit()
{
	// Write the zoom prefs
	char str[32];
	sprintf(str, "%d", (g_bMidMouseButton ? 1 : 0) + (g_bItemZoom ? 2 : 0) + (g_bUndoZoom ? 4 : 0) +
		(g_bUnzoomMode ? 8 : 0) + (g_bUndoSWSOnly ? 16 : 0) + (g_bLastUndoProj ? 32 : 0) + (g_iMidMouseModifier << 8));
	WritePrivateProfileString(SWS_INI, ZOOMPREFS_KEY, str, get_ini_file());
}