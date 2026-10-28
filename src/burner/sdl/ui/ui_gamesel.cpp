// Game selector: the parent/clone list with the filters of the Win32 selector,
// previews and titles, driver icons, the rom scan (in a thread) and the
// Neo Geo MVS cartridge slots
#include "burner.h"
#include "gamelist_model.h"
#include "ui.h"
#include "imgui.h"
#include "imgui_internal.h"			// CheckboxFlags() for 64 bits
#include <vector>
#include <algorithm>

char szSelSearch[256] = "";
bool bEnableIcons = false;
int nIconsSize = 0;
bool bIconsOnlyParents = true;
bool bIconsByHardwares = false;
char szSelMvsCarts[6][64];

static int nSelected = -1;
static bool bScrollToSelected = false;
static bool bFocusList = true;				// gives the keyboard / gamepad navigation to the selected game
static bool bNeedBuild = true;				// filters, search or availability changed
static bool bOverGame = false;				// opened over the running game
static int nSortKey = GAMELIST_SORT_NAME;
static bool bSortDescending = false;
static signed char* pExpand = NULL;			// per parent: -1 the default of the Win32 selector, 0 closed, 1 open

struct Row {
	int nDrv;
	int nNode;
	bool bClone;
};
static std::vector<Row> Rows;

// requests for the popups, opened at the level of the selector window
static int nConfirmDrv = -1;				// "not working, load anyway?"
static bool bMvsRequest = false;
static int nMvsDrv = -1;					// the MVS system driver

// ----------------------------------------------------------------------------
// Rom availability cache, the file of the old selector

static void CreateRomDatName(char* szRomDat)
{
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	char* pszConfigPath = SDL_GetPrefPath("fbneo", "config");
	snprintf(szRomDat, MAX_PATH, "%sroms.found", pszConfigPath);
	SDL_free(pszConfigPath);
#else
	snprintf(szRomDat, MAX_PATH, "fbneo.dat");
#endif
}

static int WriteGameAvb()
{
	char szRomDat[MAX_PATH];
	CreateRomDatName(szRomDat);

	FILE* h = fopen(szRomDat, "wt");
	if (h == NULL) {
		return 1;
	}

	fprintf(h, APP_TITLE " v%.20s ROMs", szAppBurnVer);
	fprintf(h, " 0x%04X ", nBurnDrvCount);
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		fputc((pGameListAv[i] & 2) ? '*' : ((pGameListAv[i] & 1) ? '+' : '-'), h);
	}
	fprintf(h, " END");
	fclose(h);

	return 0;
}

static int ReadGameAvb()
{
	char szRomDat[MAX_PATH];
	CreateRomDatName(szRomDat);

	FILE* h = fopen(szRomDat, "rt");
	if (h == NULL) {
		return 1;
	}

	int nSize = nBurnDrvCount + 256;
	char* pszBuffer = (char*)calloc(nSize, 1);
	if (pszBuffer == NULL) {
		fclose(h);
		return 1;
	}
	fgets(pszBuffer, nSize, h);
	fclose(h);

	int nRet = 1;
	char szLabel[256];
	snprintf(szLabel, sizeof(szLabel), APP_TITLE " v%.20s ROMs", szAppBurnVer);
	char* p = LabelCheck(pszBuffer, szLabel);
	if (p) {
		char* pEnd = NULL;
		UINT32 nCount = strtol(p, &pEnd, 0);
		if (nCount == nBurnDrvCount && pEnd && *pEnd == ' ') {
			p = pEnd + 1;
			nRet = 0;
			for (UINT32 i = 0; i < nBurnDrvCount && nRet == 0; i++, p++) {
				switch (*p) {
					case '*': pGameListAv[i] = GAMELIST_AV_OK; break;
					case '+': pGameListAv[i] = GAMELIST_AV_OPTIONAL; break;
					case '-': pGameListAv[i] = GAMELIST_AV_MISSING; break;
					default:  nRet = 1; break;
				}
			}
			if (nRet == 0 && strncmp(p, " END", 4) != 0) {
				nRet = 1;
			}
		}
	}
	free(pszBuffer);

	return nRet;
}

// ----------------------------------------------------------------------------
// Rom scan. BzipOpen() works on nBurnDrvActive, so while the thread runs
// nothing else may use it: the selector only reads the GameList cache and
// the game launch stops the scan first.

static SDL_Thread* pScanThread = NULL;
static SDL_atomic_t nScanNext;
static SDL_atomic_t nScanStop;
static SDL_atomic_t nScanFinished;
static UINT32 nScanOldDrv = 0;
static Uint32 nScanLastBuild = 0;

static void ScanEnd();

static int ScanProc(void*)
{
	int i;
	while ((i = SDL_AtomicGet(&nScanNext)) < (int)nBurnDrvCount && !SDL_AtomicGet(&nScanStop)) {
		nBurnDrvActive = i;
		switch (BzipOpen(true)) {
			case 0:  pGameListAv[i] = GAMELIST_AV_OK; break;
			case 2:  pGameListAv[i] = GAMELIST_AV_OPTIONAL; break;
			default: pGameListAv[i] = GAMELIST_AV_MISSING; break;
		}
		BzipClose();
		SDL_AtomicSet(&nScanNext, i + 1);
	}
	SDL_AtomicSet(&nScanFinished, 1);

	return 0;
}

static bool Scanning()
{
	return pScanThread != NULL;
}

static void ScanStart(int nFrom)
{
	if (Scanning() || UiGameRunning()) {
		return;
	}

	SDL_AtomicSet(&nScanNext, nFrom);
	SDL_AtomicSet(&nScanStop, 0);
	SDL_AtomicSet(&nScanFinished, 0);
	nScanOldDrv = nBurnDrvActive;

	pScanThread = SDL_CreateThread(ScanProc, "rom scan", NULL);
	if (pScanThread == NULL) {
		ScanProc(NULL);						// no thread, do it now
		ScanEnd();
	}
}

static void ScanEnd()
{
	if (pScanThread) {
		SDL_WaitThread(pScanThread, NULL);
		pScanThread = NULL;
	}
	nBurnDrvActive = nScanOldDrv;

	if (SDL_AtomicGet(&nScanNext) >= (int)nBurnDrvCount) {
		WriteGameAvb();
	}
	bNeedBuild = true;
}

void UiGameSelStopScan()
{
	if (Scanning()) {
		SDL_AtomicSet(&nScanStop, 1);
		ScanEnd();
	}
}

// the rest of the scan, when the game that stopped it is over
static bool bScanResume = false;
static bool bRescanPending = false;			// the rom paths changed while a game was running

void UiGameSelPauseScan()
{
	bool bWasScanning = Scanning();
	UiGameSelStopScan();
	if (bWasScanning && SDL_AtomicGet(&nScanNext) < (int)nBurnDrvCount) {
		bScanResume = true;
	}
}

int UiGameSelCheckRoms(int nDrv, char* pszText, int nSize)
{
	extern StringSet BzipText, BzipDetail;	// bzip.cpp

	UiGameSelPauseScan();
	UINT32 nOldDrv = nBurnDrvActive;
	nBurnDrvActive = nDrv;

	int nAv;
	switch (BzipOpen(false)) {
		case 0:  nAv = GAMELIST_AV_OK; break;
		case 2:  nAv = GAMELIST_AV_OPTIONAL; break;
		default: nAv = GAMELIST_AV_MISSING; break;
	}
	snprintf(pszText, nSize, "%s%s", BzipText.szText ? BzipText.szText : "", BzipDetail.szText ? BzipDetail.szText : "");
	BzipClose();
	nBurnDrvActive = nOldDrv;

	if (pGameListAv) {
		pGameListAv[nDrv] = nAv;
		WriteGameAvb();
		bNeedBuild = true;
	}

	return nAv;
}

void UiGameSelRefresh()
{
	bNeedBuild = true;
}

static void ScanPoll()
{
	if (Scanning()) {
		if (SDL_AtomicGet(&nScanFinished)) {
			ScanEnd();
		} else if (SDL_GetTicks() - nScanLastBuild > 500) {
			nScanLastBuild = SDL_GetTicks();
			bNeedBuild = true;				// the list fills up while the scan goes
		}
	} else if (bRescanPending && !UiGameRunning()) {
		bRescanPending = false;
		UiGameSelRescan();
	} else if (bScanResume && !UiGameRunning()) {
		bScanResume = false;
		ScanStart(SDL_AtomicGet(&nScanNext));
	}
}

void UiGameSelRescan()
{
	if (pGameListAv == NULL) {
		return;
	}
	if (UiGameRunning()) {
		bRescanPending = true;
		return;
	}
	// the old results stay until each set is checked again, so the list doesn't empty
	UiGameSelStopScan();
	bScanResume = false;
	ScanStart(0);
	bNeedBuild = true;
}

// ----------------------------------------------------------------------------
// Previews, titles and icons

struct Image {
	SDL_Texture* pTexture;
	int nWidth, nHeight;
};

static Image Title = { NULL, 0, 0 };
static Image Preview = { NULL, 0, 0 };
static int nImagesDrv = -1;
static int nPreviewIndex = 1;
static Uint32 nPreviewTime = 0;
static bool bPreviewCycle = false;

static void FreeImage(Image* pImage)
{
	if (pImage->pTexture) {
		SDL_DestroyTexture(pImage->pTexture);
	}
	pImage->pTexture = NULL;
	pImage->nWidth = pImage->nHeight = 0;
}

// <path><name>.png, then <path><name> [NN].png, then the same for the parent
// (OpenPreview() of the Win32 selector), and <path><zip>.zip at last
static bool LoadSnap(Image* pImage, const char* pszPath, const char* pszZip, const char* pszZipFolder, int nDrv, int nIndex)
{
	const GameListEntry* e = GameListGet(nDrv);
	char szFile[MAX_PATH * 2];
	const char* pszNames[2] = { e->pszName, e->nParent >= 0 ? e->pszParent : NULL };

	FreeImage(pImage);

	for (int i = 0; i < 2 && pszNames[i]; i++) {
		if (nIndex <= 1) {
			snprintf(szFile, sizeof(szFile), "%s%s.png", pszPath, pszNames[i]);
			if ((pImage->pTexture = UiImageLoad(szFile, &pImage->nWidth, &pImage->nHeight)) != NULL) {
				return true;
			}
		}
		snprintf(szFile, sizeof(szFile), "%s%s [%02d].png", pszPath, pszNames[i], nIndex);
		if ((pImage->pTexture = UiImageLoad(szFile, &pImage->nWidth, &pImage->nHeight)) != NULL) {
			return true;
		}
	}

	if (nIndex <= 1) {
		char szEntry[MAX_PATH];
		snprintf(szFile, sizeof(szFile), "%s%s", pszPath, pszZip);
		snprintf(szEntry, sizeof(szEntry), "%s/%s.png", pszZipFolder, e->pszName);
		pImage->pTexture = UiImageLoadFromZipFile(szFile, szEntry, &pImage->nWidth, &pImage->nHeight);
	}

	return pImage->pTexture != NULL;
}

static void LoadImages(int nDrv)
{
	nImagesDrv = nDrv;
	nPreviewIndex = 1;
	nPreviewTime = SDL_GetTicks();
	FreeImage(&Title);
	FreeImage(&Preview);
	if (nDrv < 0) {
		bPreviewCycle = false;
		return;
	}

	LoadSnap(&Title, szAppTitlesPath, "titles.zip", "titles", nDrv, 0);
	bPreviewCycle = LoadSnap(&Preview, szAppPreviewsPath, "previews.zip", "previews", nDrv, 1);
}

// the previews of a game change every 2.5 seconds, as in the Win32 selector
static void CyclePreview()
{
	if (!bPreviewCycle || nImagesDrv < 0 || SDL_GetTicks() - nPreviewTime < 2500) {
		return;
	}
	nPreviewTime = SDL_GetTicks();

	Image Next = { NULL, 0, 0 };
	if (LoadSnap(&Next, szAppPreviewsPath, "", "", nImagesDrv, nPreviewIndex + 1)) {
		FreeImage(&Preview);
		Preview = Next;
		nPreviewIndex++;
	} else if (nPreviewIndex == 1) {
		bPreviewCycle = false;				// only one image
	} else {
		nPreviewIndex = 1;
		if (LoadSnap(&Next, szAppPreviewsPath, "previews.zip", "previews", nImagesDrv, 1)) {
			FreeImage(&Preview);
			Preview = Next;
		}
	}
}

static SDL_Texture** pIcons = NULL;			// per driver
static char* pIconTried = NULL;
struct HardwareIcon {
	const char* pszName;
	SDL_Texture* pTexture;
	bool bTried;
};
static std::vector<HardwareIcon> HardwareIcons;
static UiZip* pIconZip = NULL;
static bool bIconZipTried = false;
static int nIconLoads = 0;					// loads left for this frame, so scrolling stays smooth

static SDL_Texture* LoadIcon(const char* pszName)
{
	if (!bIconZipTried) {
		char szZip[MAX_PATH * 2];
		snprintf(szZip, sizeof(szZip), "%sicons.zip", szAppIconsPath);
		pIconZip = UiZipOpen(szZip);
		bIconZipTried = true;
	}

	char szFile[MAX_PATH * 2];
	if (pIconZip) {
		snprintf(szFile, sizeof(szFile), "icons/%s.ico", pszName);
		return UiImageLoadZip(pIconZip, szFile, NULL, NULL);
	}
	snprintf(szFile, sizeof(szFile), "%s%s.ico", szAppIconsPath, pszName);
	return UiImageLoad(szFile, NULL, NULL);
}

static void FreeIcons()
{
	if (pIcons) {
		for (UINT32 i = 0; i < nBurnDrvCount; i++) {
			if (pIcons[i]) {
				SDL_DestroyTexture(pIcons[i]);
				pIcons[i] = NULL;
			}
		}
		memset(pIconTried, 0, nBurnDrvCount);
	}
	for (size_t i = 0; i < HardwareIcons.size(); i++) {
		if (HardwareIcons[i].pTexture) {
			SDL_DestroyTexture(HardwareIcons[i].pTexture);
		}
	}
	HardwareIcons.clear();
	UiZipClose(pIconZip);
	pIconZip = NULL;
	bIconZipTried = false;
}

static SDL_Texture* DriverIcon(int nDrv, bool bWait)
{
	if (bIconsByHardwares) {
		const char* pszName = GameListHardwareIcon(nDrv);
		for (size_t i = 0; i < HardwareIcons.size(); i++) {
			if (HardwareIcons[i].pszName == pszName) {
				return HardwareIcons[i].pTexture;
			}
		}
		HardwareIcon icon = { pszName, LoadIcon(pszName), true };
		HardwareIcons.push_back(icon);
		return icon.pTexture;
	}

	const GameListEntry* e = GameListGet(nDrv);
	if (e->nParent >= 0 && bIconsOnlyParents) {
		return NULL;
	}
	if (!pIconTried[nDrv]) {
		if (nIconLoads <= 0 && !bWait) {
			return NULL;
		}
		nIconLoads--;
		pIconTried[nDrv] = 1;
		pIcons[nDrv] = LoadIcon(e->pszName);
	}
	if (pIcons[nDrv] == NULL && e->nParent >= 0) {
		return DriverIcon(e->nParent, bWait);	// a clone without its own icon takes the parent's
	}
	return pIcons[nDrv];
}

void UiGameSelReloadImages()
{
	FreeIcons();
	nImagesDrv = -2;						// reloaded when drawn
}

// The size of an image of the game in a box of nMaxW x nMaxH, using the
// aspect of the game (the snaps are in the game resolution)
static ImVec2 SnapSize(const Image* pImage, int nDrv, float fMaxW, float fMaxH)
{
	const GameListEntry* e = GameListGet(nDrv);
	int ax = e->nAspectX, ay = e->nAspectY;

	// pixel aspect drivers (and wrally2), and the snaps of one screen of a
	// multi-monitor game, are shown 4:3
	if (ax > 100 || strncmp(e->pszName, "wrally2", 7) == 0 || ax <= 0 || ay <= 0) {
		ax = 4;
		ay = 3;
	}
	if (ax > 4 && pImage->nWidth <= e->nWidth / 2) {
		ax = 4;
		ay = 3;
	}

	float w = fMaxW, h = fMaxW * ay / ax;
	if (h > fMaxH) {
		h = fMaxH;
		w = fMaxH * ax / ay;
	}
	return ImVec2(w, h);
}

// ----------------------------------------------------------------------------
// The list

static bool IsExpanded(const GameListNode* pNode)
{
	if (pNode->nCloneCount == 0) {
		return false;
	}
	signed char nState = pExpand[pNode->nDrv];
	return nState < 0 ? GameListExpandedByDefault(pNode) : nState != 0;
}

static void BuildRows()
{
	Rows.clear();
	for (int i = 0; i < nGameListNodes; i++) {
		const GameListNode* n = &pGameListNodes[i];
		Row row = { n->nDrv, i, false };
		Rows.push_back(row);
		if (IsExpanded(n)) {
			for (int j = 0; j < n->nCloneCount; j++) {
				Row clone = { pnGameListClones[n->nFirstClone + j], i, true };
				Rows.push_back(clone);
			}
		}
	}
}

static void Build()
{
	GameListBuild(szSelSearch, nSortKey, bSortDescending, false);
	BuildRows();
	bNeedBuild = false;

	// a selection that isn't in the list goes to the first game (with a search, Enter plays the first match)
	bool bFound = false;
	for (size_t i = 0; i < Rows.size() && !bFound; i++) {
		bFound = Rows[i].nDrv == nSelected;
	}
	if (!bFound && !Rows.empty() && !Scanning()) {
		nSelected = Rows[0].nDrv;
		bScrollToSelected = true;
	}
}

static void SetExpanded(int nNode, bool bOpen)
{
	pExpand[pGameListNodes[nNode].nDrv] = bOpen ? 1 : 0;
	BuildRows();
}

static void Select(int nDrv)
{
	if (nDrv != nSelected) {
		nSelected = nDrv;
	}
}

static void Launch(int nDrv)
{
	bool bWasScanning = Scanning();
	UiGameSelStopScan();
	bScanResume = bWasScanning && SDL_AtomicGet(&nScanNext) < (int)nBurnDrvCount;
	bOverGame = false;
	nGameSelect = nDrv;
	UiLaunchGame(nDrv);
}

// Play: the MVS system asks for its cartridges, the broken games for a confirmation
static void TryLaunch(int nDrv)
{
	const GameListEntry* e = GameListGet(nDrv);
	if (e == NULL) {
		return;
	}
	if ((e->nHardware & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_MVS) {
		nMvsDrv = nDrv;
		bMvsRequest = true;
		return;
	}
	if (!e->bWorking) {
		nConfirmDrv = nDrv;
		return;
	}
	Launch(nDrv);
}

static void OpenArcadeDatabase(int nDrv)
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
	char szUrl[256];
	snprintf(szUrl, sizeof(szUrl), "http://adb.arcadeitalia.net/dettaglio_mame.php?game_name=%s", GameListMameName(GameListGet(nDrv)->pszName));
	SDL_OpenURL(szUrl);
#endif
}

static void RowContextMenu(int nDrv)
{
	if (ImGui::BeginPopupContextItem("##rowmenu")) {
		Select(nDrv);
		const GameListEntry* e = GameListGet(nDrv);
		if (ImGui::MenuItem("Play")) {
			TryLaunch(nDrv);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Favorite", NULL, e->bFavorite)) {
			GameListFavoriteSet(nDrv, !e->bFavorite);
			if (nLoadMenuFavoritesFilter) {
				bNeedBuild = true;
			}
		}
		if (ImGui::MenuItem("Game Info...")) {
			UiGameInfoOpen(nDrv);
		}
#if SDL_VERSION_ATLEAST(2, 0, 14)
		if (ImGui::MenuItem("View in Arcade Database")) {
			OpenArcadeDatabase(nDrv);
		}
#endif
		ImGui::EndPopup();
	}
}

// red: essential roms missing, yellow: optional roms missing or bad, grey: not working
static void DrawStatus(int nDrv, float fSize)
{
	const GameListEntry* e = GameListGet(nDrv);
	ImU32 nColor = 0;
	if (!e->bWorking) {
		nColor = IM_COL32(150, 150, 150, 255);
	} else if (pGameListAv[nDrv] == GAMELIST_AV_MISSING) {
		nColor = IM_COL32(220, 70, 60, 255);
	} else if (pGameListAv[nDrv] == GAMELIST_AV_OPTIONAL) {
		nColor = IM_COL32(230, 190, 50, 255);
	}

	ImVec2 pos = ImGui::GetCursorScreenPos();
	if (nColor) {
		float r = fSize * 0.22f;
		ImVec2 c(pos.x + fSize * 0.5f, pos.y + ImGui::GetTextLineHeight() * 0.5f);
		if (e->bWorking) {
			ImGui::GetWindowDrawList()->AddCircleFilled(c, r, nColor);
		} else {
			ImGui::GetWindowDrawList()->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), nColor, 2.0f * fUiScale);
			ImGui::GetWindowDrawList()->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), nColor, 2.0f * fUiScale);
		}
		if (ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + fSize, pos.y + ImGui::GetTextLineHeight()))) {
			ImGui::SetTooltip("%s", !e->bWorking ? "Not working" : (pGameListAv[nDrv] == GAMELIST_AV_MISSING ? "ROMs missing" : "Optional ROMs missing or bad"));
		}
	}
	ImGui::Dummy(ImVec2(fSize, ImGui::GetTextLineHeight()));
}

static void DrawList(float fHeight)
{
	if (bNeedBuild) {
		Build();						// the toolbar and the filters change it in this frame
	}

	ImGuiTableFlags nFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
		| ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable;

	if (!ImGui::BeginTable("##games", 5, nFlags, ImVec2(0.0f, fHeight))) {
		return;
	}

	ImGui::TableSetupScrollFreeze(0, 1);
	// all proportional, so the title keeps its room whatever the width of the panel
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoHide, 5.0f, GAMELIST_SORT_NAME);
	ImGui::TableSetupColumn("Zip", ImGuiTableColumnFlags_WidthStretch, 1.0f, GAMELIST_SORT_ZIP);
	ImGui::TableSetupColumn("Year", ImGuiTableColumnFlags_WidthStretch, 0.6f, GAMELIST_SORT_YEAR);
	ImGui::TableSetupColumn("Manufacturer", ImGuiTableColumnFlags_WidthStretch, 1.6f, GAMELIST_SORT_MANUFACTURER);
	ImGui::TableSetupColumn("System", ImGuiTableColumnFlags_WidthStretch, 1.6f, GAMELIST_SORT_SYSTEM);
	ImGui::TableHeadersRow();

	ImGuiTableSortSpecs* pSort = ImGui::TableGetSortSpecs();
	if (pSort && pSort->SpecsDirty) {
		if (pSort->SpecsCount > 0) {
			nSortKey = pSort->Specs[0].ColumnUserID;
			bSortDescending = pSort->Specs[0].SortDirection == ImGuiSortDirection_Descending;
		}
		pSort->SpecsDirty = false;
		Build();
		bScrollToSelected = true;
	}

	bool bShort = (nLoadMenuShowY & GAMELIST_SHOWSHORT) != 0;
	bool bAsciiOnly = (nLoadMenuShowY & GAMELIST_ASCIIONLY) != 0;
	float fIcon = (16.0f + 8.0f * nIconsSize) * fUiScale;
	float fRowHeight = (bEnableIcons && fIcon > ImGui::GetTextLineHeight()) ? fIcon : ImGui::GetTextLineHeight();
	float fArrow = ImGui::GetTextLineHeight();
	float fStatus = ImGui::GetTextLineHeight();
	nIconLoads = 16;

	int nScrollRow = -1;
	if (bScrollToSelected) {
		for (int i = 0; i < (int)Rows.size(); i++) {
			if (Rows[i].nDrv == nSelected) {
				nScrollRow = i;
				break;
			}
		}
	}

	int nToggleNode = -1;
	bool bToggleOpen = false;

	ImGuiListClipper clipper;
	clipper.Begin(Rows.size());
	if (nScrollRow >= 0) {
		clipper.IncludeItemByIndex(nScrollRow);
	}
	while (clipper.Step()) {
		for (int nRow = clipper.DisplayStart; nRow < clipper.DisplayEnd; nRow++) {
			const Row& row = Rows[nRow];
			const GameListEntry* e = GameListGet(row.nDrv);
			const GameListNode* n = &pGameListNodes[row.nNode];
			bool bOpen = !row.bClone && IsExpanded(n);

			ImGui::TableNextRow(ImGuiTableRowFlags_None, fRowHeight);
			if (e->bFavorite) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(70, 110, 60, 90));
			}

			ImU32 nColor = ImGui::GetColorU32(ImGuiCol_Text);
			if ((row.bClone || !n->bMatch) && row.nDrv != nSelected) {
				nColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
			}
			if (pGameListAv[row.nDrv] == GAMELIST_AV_MISSING) {
				nColor = IM_COL32(200, 110, 105, 255);
			}

			ImGui::TableNextColumn();
			ImGui::PushID(row.nDrv);
			ImGui::PushStyleColor(ImGuiCol_Text, nColor);

			ImVec2 rowStart = ImGui::GetCursorPos();
			if (bFocusList && row.nDrv == nSelected) {
				ImGui::SetKeyboardFocusHere();
				bFocusList = false;
			}
			if (ImGui::Selectable("##row", row.nDrv == nSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, fRowHeight))) {
				Select(row.nDrv);
				// a double click, or Enter / the gamepad button: a single click only selects
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					TryLaunch(row.nDrv);
				}
			}
			if (ImGui::IsItemFocused()) {
				Select(row.nDrv);				// follows the keyboard / gamepad
				if (!row.bClone && n->nCloneCount && (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow))) {
					nToggleNode = row.nNode;
					bToggleOpen = ImGui::IsKeyPressed(ImGuiKey_RightArrow);
				}
			}
			RowContextMenu(row.nDrv);
			if (nRow == nScrollRow) {
				ImGui::SetScrollHereY(0.5f);
				bScrollToSelected = false;
			}

			// the tree arrow, the status, the icon and the title over the selectable
			float fTextY = rowStart.y + (fRowHeight - ImGui::GetTextLineHeight()) * 0.5f;
			ImGui::SetCursorPos(ImVec2(rowStart.x + (row.bClone ? fArrow : 0.0f), fTextY));
			if (!row.bClone && n->nCloneCount) {
				ImVec2 arrowPos = ImGui::GetCursorScreenPos();
				if (ImGui::InvisibleButton("##arrow", ImVec2(fArrow, ImGui::GetTextLineHeight()))) {
					nToggleNode = row.nNode;
					bToggleOpen = !bOpen;
				}
				ImGui::GetWindowDrawList()->AddText(arrowPos, ImGui::GetColorU32(ImGuiCol_Text), bOpen ? "-" : "+");
				ImGui::SameLine(0.0f, 0.0f);
			} else if (!row.bClone) {
				ImGui::Dummy(ImVec2(fArrow, ImGui::GetTextLineHeight()));
				ImGui::SameLine(0.0f, 0.0f);
			}
			DrawStatus(row.nDrv, fStatus);
			ImGui::SameLine(0.0f, 2.0f * fUiScale);

			if (bEnableIcons) {
				SDL_Texture* pIcon = DriverIcon(row.nDrv, false);
				ImGui::SetCursorPosY(rowStart.y + (fRowHeight - fIcon) * 0.5f);
				if (pIcon) {
					ImGui::Image((ImTextureID)(intptr_t)pIcon, ImVec2(fIcon, fIcon));
				} else {
					ImGui::Dummy(ImVec2(fIcon, fIcon));
				}
				ImGui::SameLine(0.0f, 4.0f * fUiScale);
				ImGui::SetCursorPosY(fTextY);
			}

			char szTitle[512];
			if (bShort) {
				ImGui::Text("%-12s", e->pszName);
				ImGui::SameLine();
			}
			ImGui::TextUnformatted(GameListTitle(row.nDrv, bAsciiOnly, szTitle, sizeof(szTitle)));

			ImGui::TableNextColumn();
			ImGui::SetCursorPosY(fTextY);
			ImGui::TextUnformatted(e->pszName);
			ImGui::TableNextColumn();
			ImGui::SetCursorPosY(fTextY);
			ImGui::TextUnformatted(e->pszDate ? e->pszDate : "");
			ImGui::TableNextColumn();
			ImGui::SetCursorPosY(fTextY);
			ImGui::TextUnformatted(UiText(e->pszManufacturer));
			ImGui::TableNextColumn();
			ImGui::SetCursorPosY(fTextY);
			ImGui::TextUnformatted(UiText(e->pszSystem));

			ImGui::PopStyleColor();
			ImGui::PopID();
		}
	}
	bScrollToSelected = bScrollToSelected && nScrollRow >= 0;

	if (Rows.empty()) {
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextDisabled(Scanning() ? "Scanning the ROMs..." : "No game to show. Check the filters, or the ROM paths (Misc > ROM Paths).");
	}

	ImGui::EndTable();

	if (nToggleNode >= 0) {
		SetExpanded(nToggleNode, bToggleOpen);
	}
}

static void DrawInfo()
{
	static const char* pszLabels[6] = { "Game:", "ROM:", "Info:", "Released:", "Genre:", "Notes:" };

	if (!ImGui::BeginTable("##info", 2, ImGuiTableFlags_SizingFixedFit)) {
		return;
	}
	ImGui::TableSetupColumn("##label");
	ImGui::TableSetupColumn("##text", ImGuiTableColumnFlags_WidthStretch);

	char szText[1024];
	for (int i = 0; i < 6; i++) {
		szText[0] = '\0';
		if (nSelected >= 0) {
			switch (i) {
				case 0: GameListInfoTitles(nSelected, szText, sizeof(szText)); break;
				case 1: GameListInfoRom(nSelected, szText, sizeof(szText)); break;
				case 2: GameListInfoBoard(nSelected, szText, sizeof(szText)); break;
				case 3: GameListInfoRelease(nSelected, szText, sizeof(szText)); break;
				case 4: GameListInfoGenre(nSelected, szText, sizeof(szText)); break;
				case 5: GameListInfoNotes(nSelected, szText, sizeof(szText)); break;
			}
		}
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", pszLabels[i]);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(UiText(szText));
		if (ImGui::IsItemHovered() && ImGui::CalcTextSize(UiText(szText)).x > ImGui::GetContentRegionAvail().x) {
			ImGui::SetTooltip("%s", UiText(szText));
		}
	}
	ImGui::EndTable();
}

static void DrawImage(const char* pszName, const Image* pImage, float fWidth, float fHeight)
{
	ImGui::TextDisabled("%s", pszName);
	ImVec2 box(fWidth, fHeight);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + box.x, pos.y + box.y), IM_COL32(0, 0, 0, 120));

	if (pImage->pTexture && nImagesDrv >= 0) {
		ImVec2 size = SnapSize(pImage, nImagesDrv, box.x, box.y);
		ImVec2 at(pos.x + (box.x - size.x) * 0.5f, pos.y + (box.y - size.y) * 0.5f);
		ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)pImage->pTexture, at, ImVec2(at.x + size.x, at.y + size.y));
	} else {
		ImVec2 size = ImGui::CalcTextSize("No image");
		ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + (box.x - size.x) * 0.5f, pos.y + (box.y - size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "No image");
	}
	ImGui::Dummy(box);
}

static void DrawImages(float fHeight)
{
	if (nImagesDrv != nSelected) {
		LoadImages(nSelected);
	}
	CyclePreview();

	float fWidth = ImGui::GetContentRegionAvail().x;
	float fBox = (fHeight - ImGui::GetTextLineHeightWithSpacing() * 2.0f - ImGui::GetStyle().ItemSpacing.y * 2.0f) * 0.5f;
	if (fBox > fWidth * 0.75f) {
		fBox = fWidth * 0.75f;
	}
	DrawImage("Title", &Title, fWidth, fBox);
	DrawImage("Preview", &Preview, fWidth, fBox);
}

// ----------------------------------------------------------------------------
// Filters

static bool GroupNode(const char* pszLabel, int nGroup, UINT64 nMask, int nExpandBit)
{
	ImGui::SetNextItemOpen((nLoadMenuExpand & nExpandBit) != 0, ImGuiCond_Once);
	bool bOpen = ImGui::TreeNodeEx((const void*)pszLabel, ImGuiTreeNodeFlags_AllowOverlap, "%s", "");
	if (bOpen) {
		nLoadMenuExpand |= nExpandBit;
	} else {
		nLoadMenuExpand &= ~nExpandBit;
	}

	ImGui::SameLine();
	// shown = not hidden: the checkbox is mixed when only some are shown
	ImU64 nShown = ~GameListFilterValue(nGroup) & nMask;
	if (ImGui::CheckboxFlags(pszLabel, &nShown, nMask)) {
		GameListFilterSet(nGroup, nMask, (nShown & nMask) == nMask);
		bNeedBuild = true;
	}

	return bOpen;
}

static void FilterCheckbox(const GameListFilter* f)
{
	ImU64 nShown = ~GameListFilterValue(f->nGroup) & f->nMask;
	if (ImGui::CheckboxFlags(f->pszName, &nShown, f->nMask)) {
		GameListFilterSet(f->nGroup, f->nMask, (nShown & f->nMask) == f->nMask);
		bNeedBuild = true;
	}
}

static void DrawFilterGroup(const char* pszLabel, int nGroup, int nExpandBit)
{
	if (!GroupNode(pszLabel, nGroup, GameListFilterAll(nGroup), nExpandBit)) {
		return;
	}

	for (int i = 0; i < nGameListFilters; i++) {
		const GameListFilter* f = &GameListFilters[i];
		if (f->nGroup != nGroup || f->nDepth != 0) {
			continue;
		}
		bool bSubGroup = i + 1 < nGameListFilters && GameListFilters[i + 1].nDepth == 1;
		if (!bSubGroup) {
			FilterCheckbox(f);
			continue;
		}

		// Capcom and Sega
		int nBit = strcmp(f->pszName, "Capcom") == 0 ? (1 << 5) : (1 << 6);
		if (GroupNode(f->pszName, nGroup, f->nMask, nBit)) {
			for (int j = i + 1; j < nGameListFilters && GameListFilters[j].nDepth == 1; j++) {
				FilterCheckbox(&GameListFilters[j]);
			}
			ImGui::TreePop();
		}
	}
	ImGui::TreePop();
}

static void DrawFilters()
{
	bool bFavorites = nLoadMenuFavoritesFilter != 0;
	if (ImGui::Checkbox("Favorites only", &bFavorites)) {
		nLoadMenuFavoritesFilter = bFavorites;
		bNeedBuild = true;
	}
	ImGui::Separator();
	DrawFilterGroup("Board Type", GAMELIST_FILTER_BOARD, 1 << 1);
	DrawFilterGroup("Family", GAMELIST_FILTER_FAMILY, 1 << 2);
	DrawFilterGroup("Genre", GAMELIST_FILTER_GENRE, 1 << 3);
	DrawFilterGroup("Hardware", GAMELIST_FILTER_HARDWARE, 1 << 4);
}

// ----------------------------------------------------------------------------
// Popups: not working confirmation, MVS cartridges

static UINT32 nMvsSlots[6];
static int nMvsPickSlot = -1;
static char szMvsSearch[128] = "";
static std::vector<int> MvsCarts;
static Image MvsThumbs[6];
static int nMvsThumbDrv[6];
static bool bMvsError = false;

static void MvsBuildCarts()
{
	MvsCarts.clear();
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		const GameListEntry* e = GameListGet(i);
		if ((e->nHardware & HARDWARE_PREFIX_CARTRIDGE) != HARDWARE_PREFIX_CARTRIDGE || (e->nHardware & HARDWARE_PUBLIC_MASK) != HARDWARE_SNK_NEOGEO) {
			continue;
		}
		if ((e->nFlags & BDF_BOARDROM) || pGameListAv[i] == GAMELIST_AV_MISSING) {
			continue;
		}
		if (szMvsSearch[0] && strstr(e->pszName, szMvsSearch) == NULL) {
			char szLower[256];
			snprintf(szLower, sizeof(szLower), "%s", e->pszFullName);
			for (char* p = szLower; *p; p++) *p = tolower((unsigned char)*p);
			char szFind[128];
			snprintf(szFind, sizeof(szFind), "%s", szMvsSearch);
			for (char* p = szFind; *p; p++) *p = tolower((unsigned char)*p);
			if (strstr(szLower, szFind) == NULL) {
				continue;
			}
		}
		MvsCarts.push_back(i);
	}
	std::sort(MvsCarts.begin(), MvsCarts.end(), [](int a, int b) { return _stricmp(GameListGet(a)->pszFullName, GameListGet(b)->pszFullName) < 0; });
}

static void MvsPicker()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(560.0f, vp->WorkSize.y * 0.7f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("Select a Cartridge###mvspick", NULL, ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::IsWindowAppearing()) {
		ImGui::SetKeyboardFocusHere();
	}
	if (ImGui::InputTextWithHint("##search", "Search", szMvsSearch, sizeof(szMvsSearch))) {
		MvsBuildCarts();
	}

	int nPick = -1;
	float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	if (ImGui::BeginChild("##carts", ImVec2(0.0f, -fBottom), ImGuiChildFlags_Borders)) {
		ImGuiListClipper clipper;
		clipper.Begin(MvsCarts.size());
		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
				char szTitle[512];
				ImGui::PushID(MvsCarts[i]);
				if (ImGui::Selectable(GameListTitle(MvsCarts[i], true, szTitle, sizeof(szTitle)), false, ImGuiSelectableFlags_AllowDoubleClick)) {
					nPick = MvsCarts[i];
				}
				ImGui::PopID();
			}
		}
		if (MvsCarts.empty()) {
			ImGui::TextDisabled("No Neo Geo cartridge with its ROMs available.");
		}
	}
	ImGui::EndChild();

	if (ImGui::Button("Cancel", ImVec2(110.0f * fUiScale, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}
	if (nPick >= 0) {
		nMvsSlots[nMvsPickSlot] = nPick;
		bMvsError = false;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

static void DrawMvsDialog()
{
	if (bMvsRequest) {
		bMvsRequest = false;
		for (int i = 0; i < 6; i++) {
			nMvsSlots[i] = nBurnDrvSelect[i];
			nMvsThumbDrv[i] = -1;
		}
		bMvsError = false;
		ImGui::OpenPopup("Neo Geo MVS Cartridges###mvs");
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Neo Geo MVS Cartridges###mvs", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		for (int i = 0; i < 6; i++) {
			FreeImage(&MvsThumbs[i]);
		}
		return;
	}

	ImVec2 thumb(96.0f * fUiScale, 72.0f * fUiScale);
	int nPickSlot = -1;

	if (ImGui::BeginTable("##slots", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("##thumb");
		ImGui::TableSetupColumn("##text", ImGuiTableColumnFlags_WidthFixed, 380.0f * fUiScale);
		ImGui::TableSetupColumn("##buttons");
		for (int i = 0; i < 6; i++) {
			ImGui::PushID(i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			int nDrv = nMvsSlots[i] < nBurnDrvCount ? (int)nMvsSlots[i] : -1;
			if (nMvsThumbDrv[i] != nDrv) {
				nMvsThumbDrv[i] = nDrv;
				FreeImage(&MvsThumbs[i]);
				if (nDrv >= 0) {
					LoadSnap(&MvsThumbs[i], szAppTitlesPath, "titles.zip", "titles", nDrv, 0);
				}
			}
			ImVec2 pos = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + thumb.x, pos.y + thumb.y), IM_COL32(0, 0, 0, 120));
			if (MvsThumbs[i].pTexture) {
				ImGui::Image((ImTextureID)(intptr_t)MvsThumbs[i].pTexture, thumb);
			} else {
				ImGui::Dummy(thumb);
			}

			ImGui::TableNextColumn();
			ImGui::Text("Slot %d", i + 1);
			if (nDrv >= 0) {
				char szTitle[512], szText[512];
				ImGui::TextUnformatted(GameListTitle(nDrv, true, szTitle, sizeof(szTitle)));
				GameListInfoRelease(nDrv, szText, sizeof(szText));
				ImGui::TextDisabled("%s", UiText(szText));
			} else {
				ImGui::TextDisabled("Empty");
			}

			ImGui::TableNextColumn();
			if (ImGui::Button("Select...")) {
				nPickSlot = i;
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear")) {
				nMvsSlots[i] = ~0U;
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (nPickSlot >= 0) {
		nMvsPickSlot = nPickSlot;
		MvsBuildCarts();
		ImGui::OpenPopup("Select a Cartridge###mvspick");
	}
	MvsPicker();

	if (bMvsError) {
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "No driver has been selected for slot 1.");
	}

	float fButton = 110.0f * fUiScale;
	if (ImGui::Button("Play", ImVec2(fButton, 0.0f))) {
		if (nMvsSlots[0] >= nBurnDrvCount) {
			bMvsError = true;
		} else {
			for (int i = 0; i < 6; i++) {
				nBurnDrvSelect[i] = nMvsSlots[i];
				snprintf(szSelMvsCarts[i], sizeof(szSelMvsCarts[i]), "%s", nMvsSlots[i] < nBurnDrvCount ? GameListGet(nMvsSlots[i])->pszName : "");
			}
			ImGui::CloseCurrentPopup();
			Launch(nMvsDrv);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

static void DrawConfirm()
{
	if (nConfirmDrv >= 0 && !ImGui::IsPopupOpen("Warning###notworking")) {
		ImGui::OpenPopup("Warning###notworking");
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Warning###notworking", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	ImGui::TextUnformatted("This game isn't working. Load it anyway?");
	ImGui::Spacing();
	float fButton = 110.0f * fUiScale;
	if (ImGui::Button("Yes", ImVec2(fButton, 0.0f))) {
		int nDrv = nConfirmDrv;
		nConfirmDrv = -1;
		ImGui::CloseCurrentPopup();
		Launch(nDrv);
	}
	ImGui::SameLine();
	if (ImGui::Button("No", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		nConfirmDrv = -1;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SetItemDefaultFocus();

	ImGui::EndPopup();
}

// ----------------------------------------------------------------------------

// SameLine() when the next item fits, so the toolbar wraps in a small window
static void SameLineIfFits(float fWidth)
{
	ImGuiStyle& style = ImGui::GetStyle();
	float fRight = ImGui::GetItemRectMax().x + style.ItemSpacing.x + fWidth;
	if (fRight <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x) {
		ImGui::SameLine();
	}
}

static float CheckboxWidth(const char* pszLabel)
{
	ImGuiStyle& style = ImGui::GetStyle();
	return ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(pszLabel).x;
}

static void OptionCheckbox(const char* pszLabel, int nFlag)
{
	SameLineIfFits(CheckboxWidth(pszLabel));
	bool bValue = (nLoadMenuShowY & nFlag) != 0;
	if (ImGui::Checkbox(pszLabel, &bValue)) {
		nLoadMenuShowY = bValue ? (nLoadMenuShowY | nFlag) : (nLoadMenuShowY & ~nFlag);
		bNeedBuild = true;
		bScrollToSelected = true;
	}
}

static void DrawToolbar()
{
	ImGuiStyle& style = ImGui::GetStyle();

	if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) {
		ImGui::SetKeyboardFocusHere();
	}
	ImGui::SetNextItemWidth(260.0f * fUiScale);
	if (ImGui::InputTextWithHint("##search", "Search (Ctrl+F)", szSelSearch, sizeof(szSelSearch))) {
		bNeedBuild = true;
		bScrollToSelected = true;
	}
	// Enter plays the selected game (the first match), Down goes to the list
	if (ImGui::IsItemDeactivated() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) && nSelected >= 0) {
		TryLaunch(nSelected);
	}
	if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
		ImGui::ClearActiveID();
		bFocusList = true;
		bScrollToSelected = true;
	}
	OptionCheckbox("Available", GAMELIST_AVAILABLE);
	OptionCheckbox("Unavailable", GAMELIST_UNAVAILABLE);
	OptionCheckbox("Always show clones", GAMELIST_AUTOEXPAND);
	OptionCheckbox("Zip names", GAMELIST_SHOWSHORT);
	OptionCheckbox("Latin text only", GAMELIST_ASCIIONLY);

	// the buttons on the right
	float fButtons = ImGui::CalcTextSize("ROM Paths...").x + ImGui::CalcTextSize("Scan ROMs").x + style.FramePadding.x * 4.0f + style.ItemSpacing.x;
	SameLineIfFits(fButtons);
	if (ImGui::GetContentRegionAvail().x > fButtons) {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - fButtons);
	}
	if (ImGui::Button("ROM Paths...")) {
		UiRomPathsOpen();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(UiGameRunning() || Scanning());
	if (ImGui::Button("Scan ROMs")) {
		UiGameSelRescan();
	}
	ImGui::EndDisabled();

	if (Scanning()) {
		int nPos = SDL_AtomicGet(&nScanNext);
		char szScan[64];
		snprintf(szScan, sizeof(szScan), "Scanning ROMs... %d of %d", nPos, nBurnDrvCount);
		float fCancel = ImGui::CalcTextSize("Cancel").x + style.FramePadding.x * 2.0f;
		ImGui::ProgressBar((float)nPos / nBurnDrvCount, ImVec2(-(fCancel + style.ItemSpacing.x), 0.0f), szScan);
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			UiGameSelStopScan();
		}
	}
}

void UiGameSelDraw(bool bOverRunningGame)
{
	static bool bGameWasRunning = false;
	if (!bOverRunningGame && bGameWasRunning) {
		nSelected = nGameSelect;		// back from a game
		bScrollToSelected = true;
		bFocusList = true;
	}
	bGameWasRunning = bOverRunningGame;

	if (bOverRunningGame && !bOverGame) {
		return;
	}
	if (!bOverRunningGame) {
		bOverGame = false;
	}
	ScanPoll();
	if (bNeedBuild) {
		Build();
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGuiStyle& style = ImGui::GetStyle();
	ImGuiWindowFlags nFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	bool bOpen = true;

	if (bOverRunningGame) {
		ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.9f, vp->WorkSize.y * 0.9f), ImGuiCond_Appearing);
		nFlags |= ImGuiWindowFlags_NoCollapse;
	} else {
		ImGui::SetNextWindowPos(vp->WorkPos);
		ImGui::SetNextWindowSize(vp->WorkSize);
		nFlags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	// the same window in both cases, so both share the column and panel sizes
	if (ImGui::Begin("Load Game###gamesel", bOverRunningGame ? &bOpen : NULL, nFlags)) {
		DrawToolbar();

		float fStatus = ImGui::GetFrameHeightWithSpacing();
		float fInfo = ImGui::GetTextLineHeightWithSpacing() * 6.0f + style.CellPadding.y * 12.0f + style.ItemSpacing.y;
		float fMain = ImGui::GetContentRegionAvail().y - fStatus;

		ImGuiTableFlags nLayout = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoPadOuterX;
		if (ImGui::BeginTable("##layout", 3, nLayout, ImVec2(0.0f, fMain))) {
			// proportional, so a small window (the selector over a small game) keeps room for the list
			ImGui::TableSetupColumn("Filters", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn("Games", ImGuiTableColumnFlags_WidthStretch, 5.5f);
			ImGui::TableSetupColumn("Images", ImGuiTableColumnFlags_WidthStretch, 2.7f);
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##filters", ImVec2(0.0f, fMain - style.CellPadding.y * 2.0f))) {
				DrawFilters();
			}
			ImGui::EndChild();

			ImGui::TableNextColumn();
			DrawList(fMain - fInfo - style.CellPadding.y * 2.0f);
			DrawInfo();

			ImGui::TableNextColumn();
			DrawImages(fMain - style.CellPadding.y * 2.0f);

			ImGui::EndTable();
		}

		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("Showing %d of %d sets [%d Unavailable sets]", nGameListShown, nGameListTotal, nGameListMissing);

		float fButton = 120.0f * fUiScale;
		int nButtons = bOverRunningGame ? 3 : 2;
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - fButton * nButtons - style.ItemSpacing.x * (nButtons - 1));
		ImGui::BeginDisabled(nSelected < 0);
		if (ImGui::Button("Game Info", ImVec2(fButton, 0.0f))) {
			UiGameInfoOpen(nSelected);
		}
		ImGui::SameLine();
		if (ImGui::Button("Play", ImVec2(fButton, 0.0f))) {
			TryLaunch(nSelected);
		}
		ImGui::EndDisabled();
		if (bOverRunningGame) {
			ImGui::SameLine();
			if (ImGui::Button("Close", ImVec2(fButton, 0.0f))) {
				bOpen = false;
			}
		}

		DrawConfirm();
		DrawMvsDialog();

		if (bOverRunningGame && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && !UiPopupActive() && UiEscapePressed()) {
			bOpen = false;
		}
	}
	ImGui::End();

	if (!bOpen) {
		bOverGame = false;
	}
}

// Play from elsewhere (recent games, quick load): with a game running the
// selector opens over it, for the cartridge slots or the confirmation
void UiGameSelPlay(int nDrv)
{
	if (UiGameRunning()) {
		bOverGame = true;
		nSelected = nDrv;
		bScrollToSelected = true;
	}
	TryLaunch(nDrv);
}

bool UiGameSelIsOpen()
{
	return bOverGame;
}

void UiGameSelClose()
{
	bOverGame = false;
}

void UiGameSelOpen()
{
	bOverGame = true;
	if (UiGameRunning()) {
		nSelected = nBurnDrvActive;
	}
	bScrollToSelected = true;
	bFocusList = true;
}

void UiGameSelInit()
{
#if !defined(SDL_WINDOWS)
	char* pszConfigPath = SDL_GetPrefPath("fbneo", "config");
	snprintf(szGameListFavorites, sizeof(szGameListFavorites), "%sfavorites.dat", pszConfigPath);
	SDL_free(pszConfigPath);
#endif

	if (GameListInit()) {
		return;
	}

	pExpand = (signed char*)malloc(nBurnDrvCount);
	memset(pExpand, -1, nBurnDrvCount);
	pIcons = (SDL_Texture**)calloc(nBurnDrvCount, sizeof(SDL_Texture*));
	pIconTried = (char*)calloc(nBurnDrvCount, 1);

	for (int i = 0; i < 6; i++) {
		int nDrv = GameListFind(szSelMvsCarts[i]);
		nBurnDrvSelect[i] = nDrv >= 0 ? (UINT32)nDrv : ~0U;
	}

	nSelected = (nGameSelect >= 0 && (UINT32)nGameSelect < nBurnDrvCount) ? nGameSelect : -1;
	bScrollToSelected = true;
	bNeedBuild = true;

	if (ReadGameAvb() && bUiScanAtStart) {
		UiGameSelRescan();
	}
}

void UiGameSelExit()
{
	UiGameSelStopScan();

	if (nSelected >= 0) {
		nGameSelect = nSelected;
	}

	FreeImage(&Title);
	FreeImage(&Preview);
	for (int i = 0; i < 6; i++) {
		FreeImage(&MvsThumbs[i]);
	}
	FreeIcons();
	free(pIcons);
	pIcons = NULL;
	free(pIconTried);
	pIconTried = NULL;
	free(pExpand);
	pExpand = NULL;
	Rows.clear();

	GameListExit();
}
