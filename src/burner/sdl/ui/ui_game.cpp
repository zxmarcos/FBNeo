// The Game menu: recent games, quick load (archive, CD image), speed and
// slow-mo, save states (slots, undo, files) and the memory cards
#include "burner.h"
#include "gamelist_model.h"
#include "cdlist.h"
#include "ui.h"
#include "imgui.h"
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

char szPrevGames[10][32];
int bUiFastToggle = 0;

// requests from the menus, the dialogs are opened at the root of the window
enum {
	DIALOG_NONE, DIALOG_STATE_LOAD, DIALOG_STATE_SAVE, DIALOG_ARCHIVE, DIALOG_CDIMAGE,
	DIALOG_MEMCARD_CREATE, DIALOG_MEMCARD_SELECT, DIALOG_PGM2_SELECT, DIALOG_CPU_SPEED
};
static int nDialogRequest = DIALOG_NONE;
static int nDialog = DIALOG_NONE;
static int nPgm2Slot = 0;
static char szBrowse[MAX_PATH] = "";

// a launch from here, carried out by UiGamePrepareLaunch()
static char szPendingQuickPath[MAX_PATH] = "";
static char szPendingCDImage[MAX_PATH] = "";
static char szPendingState[MAX_PATH] = "";		// loaded once the game runs

static char szBrowseTitle[64] = "###gamebrowse";		// the id is only the part after ###

// ----------------------------------------------------------------------------

static void Error(const char* pszText)
{
	AppError((TCHAR*)pszText, 0);
}

void UiRecentAdd(const char* pszName)
{
	int i;
	for (i = 0; i < 9; i++) {
		if (strcmp(szPrevGames[i], pszName) == 0) {
			break;
		}
	}
	memmove(szPrevGames[1], szPrevGames[0], i * sizeof(szPrevGames[0]));
	snprintf(szPrevGames[0], sizeof(szPrevGames[0]), "%s", pszName);
}

// Called by StartGame() once the rom scan is stopped: the folder of a quick
// load archive is searched first while that game runs
void UiGamePrepareLaunch()
{
	_tcscpy(szAppQuickPath, szPendingQuickPath);
	szPendingQuickPath[0] = '\0';
	if (szPendingCDImage[0]) {
		_tcscpy(CDEmuImage, szPendingCDImage);
		szPendingCDImage[0] = '\0';
	}
}

void UiGameStarted()
{
	UiRecentAdd(BurnDrvGetTextA(DRV_NAME));

	if (szPendingState[0]) {
		if (StatedLoadFile(szPendingState)) {
			Error("The state couldn't be loaded.\n");
		}
		szPendingState[0] = '\0';
	}
}

static void LoadStateFile(const char* pszFile)
{
	char szName[33];
	if (StatedFileGame(pszFile, szName)) {
		Error("This file isn't a save state.\n");
		return;
	}

	if (UiGameRunning() && strcmp(szName, BurnDrvGetTextA(DRV_NAME)) == 0) {
		int nRet = StatedLoadFile(pszFile);
		if (nRet == 4) Error("This state is too old to be loaded by this version.\n");
		else if (nRet == 5) Error("This state was saved by a newer version.\n");
		else if (nRet) Error("The state couldn't be loaded.\n");
		return;
	}

	// a state of another game: that game is started first
	int nDrv = GameListFind(szName);
	if (nDrv < 0) {
		char szText[128];
		snprintf(szText, sizeof(szText), "This state is for %s, which this version doesn't have.\n", szName);
		Error(szText);
		return;
	}
	snprintf(szPendingState, sizeof(szPendingState), "%s", pszFile);
	UiGameSelPlay(nDrv);
}

// <folder>/<zip>.zip: the game whose zip name it is
static void QuickLoadArchive(const char* pszFile)
{
	const char* pszName = pszFile + strlen(pszFile);
	while (pszName > pszFile && pszName[-1] != '/' && pszName[-1] != '\\') pszName--;
	const char* pszExt = strrchr(pszName, '.');

	char szName[64];
	int nLen = pszExt ? (int)(pszExt - pszName) : (int)strlen(pszName);
	if (nLen >= (int)sizeof(szName)) nLen = sizeof(szName) - 1;
	for (int i = 0; i < nLen; i++) szName[i] = tolower((unsigned char)pszName[i]);
	szName[nLen] = '\0';

	int nDrv = GameListFind(szName);
	if (nDrv < 0 || (GameListGet(nDrv)->nFlags & BDF_BOARDROM)) {
		char szText[MAX_PATH + 64];
		snprintf(szText, sizeof(szText), "No game uses the archive %s.\n", pszName);
		Error(szText);
		return;
	}

	snprintf(szPendingQuickPath, sizeof(szPendingQuickPath), "%.*s", (int)(pszName - pszFile), pszFile);
	UiGameSelPlay(nDrv);
}

// a .cue/.ccd/.chd: Neo Geo CD or PC Engine CD, told by the contents
static void QuickLoadCD(const char* pszFile)
{
	CDListResult result;
	const char* pszDriver = NULL;

	if (CDListIdentify((TCHAR*)pszFile, &result)) {
		if (result.nPlatform == CDLIST_PLATFORM_NEOCD) pszDriver = "neocdz";
		if (result.nPlatform == CDLIST_PLATFORM_PCECD) pszDriver = "pce_scdsys";
	}
	int nDrv = pszDriver ? GameListFind(pszDriver) : -1;
	if (nDrv < 0) {
		char szText[MAX_PATH + 64];
		snprintf(szText, sizeof(szText), "The CD image %s is neither a Neo Geo CD nor a PC Engine CD.\n", pszFile);
		Error(szText);
		return;
	}

	char szFull[MAX_PATH];
#ifdef _WIN32
	if (_fullpath(szFull, pszFile, sizeof(szFull)) == NULL)
#else
	if (realpath(pszFile, szFull) == NULL)
#endif
	{
		snprintf(szFull, sizeof(szFull), "%s", pszFile);
	}
	snprintf(szPendingCDImage, sizeof(szPendingCDImage), "%s", szFull);
	UiGameSelPlay(nDrv);
}

// ----------------------------------------------------------------------------
// Dialogs

static const char* FirstRomPath()
{
	for (int i = 0; i < DIRS_MAX; i++) {
		struct stat st;
		if (szAppRomPaths[i][0] && stat(szAppRomPaths[i], &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR) {
			return szAppRomPaths[i];
		}
	}
	return "";
}

static void OpenBrowser(int nDialogType)
{
	static const char* pszTitles[] = { "", "Load State", "Save State", "Quick Load a ROM Archive", "Quick Load a CD Image", "Create a Memory Card", "Select a Memory Card", "Select an IC Card" };
	snprintf(szBrowseTitle, sizeof(szBrowseTitle), "%s###gamebrowse", pszTitles[nDialogType]);

	nDialog = nDialogType;
	switch (nDialogType) {
		case DIALOG_STATE_LOAD:
			snprintf(szBrowse, sizeof(szBrowse), "%s", StatedFolder());
			UiFileBrowserOpen(szBrowseTitle, szBrowse, UI_BROWSE_OPEN, ".fs");
			break;
		case DIALOG_STATE_SAVE:
			snprintf(szBrowse, sizeof(szBrowse), "%s%s.fs", StatedFolder(), BurnDrvGetTextA(DRV_NAME));
			UiFileBrowserOpen(szBrowseTitle, szBrowse, UI_BROWSE_SAVE, ".fs");
			break;
		case DIALOG_ARCHIVE:
			UiFileBrowserOpen(szBrowseTitle, FirstRomPath(), UI_BROWSE_OPEN, ".zip;.7z");
			break;
		case DIALOG_CDIMAGE:
			UiFileBrowserOpen(szBrowseTitle, CDEmuImage, UI_BROWSE_OPEN, ".cue;.ccd;.chd");
			break;
		case DIALOG_MEMCARD_CREATE:
			snprintf(szBrowse, sizeof(szBrowse), "%s%s.fc", MemCardFolder(), BurnDrvGetTextA(DRV_NAME));
			UiFileBrowserOpen(szBrowseTitle, szBrowse, UI_BROWSE_SAVE, ".fc");
			break;
		case DIALOG_MEMCARD_SELECT:
			UiFileBrowserOpen(szBrowseTitle, szMemoryCardFile[0] ? szMemoryCardFile : MemCardFolder(), UI_BROWSE_OPEN, NULL);
			break;
		case DIALOG_PGM2_SELECT:
			UiFileBrowserOpen(szBrowseTitle, MemCardFolder(), UI_BROWSE_OPEN, ".bin;.fc");
			break;
	}
}

static void BrowserDone(const char* pszFile)
{
	switch (nDialog) {
		case DIALOG_STATE_LOAD:
			LoadStateFile(pszFile);
			break;
		case DIALOG_STATE_SAVE:
			if (StatedSaveFile(pszFile)) {
				Error("The state couldn't be saved.\n");
			}
			break;
		case DIALOG_ARCHIVE:
			QuickLoadArchive(pszFile);
			break;
		case DIALOG_CDIMAGE:
			QuickLoadCD(pszFile);
			break;
		case DIALOG_MEMCARD_CREATE:
			MemCardEject();
			if (MemCardCreate(pszFile)) {
				Error("The memory card couldn't be created.\n");
			} else if (MemCardInsert() == 0) {
				UpdateMessage("Memory card inserted");
			}
			break;
		case DIALOG_MEMCARD_SELECT:
			MemCardEject();
			if (MemCardSelect(pszFile)) {
				Error("This file can't be used as a memory card.\n");
			} else if (MemCardInsert()) {
				Error("This memory card can't be used by this game.\n");
			} else {
				UpdateMessage("Memory card inserted");
			}
			break;
		case DIALOG_PGM2_SELECT:
			MemCardEjectPGM2Slot(nPgm2Slot);
			if (MemCardSelectPGM2Slot(nPgm2Slot, pszFile)) {
				Error("This file can't be used as an IC card.\n");
			} else {
				MemCardInsertPGM2Slot(nPgm2Slot);
			}
			break;
	}
}

static void CpuSpeedDialog()
{
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("CPU Speed###cpuspeed", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	int nPercent = (nBurnCPUSpeedAdjust * 100 + 128) / 256;
	ImGui::TextUnformatted("Speed of the emulated processors");
	ImGui::SetNextItemWidth(300.0f * fUiScale);
	if (ImGui::SliderInt("##slider", &nPercent, 50, 200, "%d%%")) {
		nBurnCPUSpeedAdjust = nPercent * 256 / 100;
	}
	ImGui::SetNextItemWidth(300.0f * fUiScale);
	if (ImGui::InputInt("##value", &nPercent, 1, 10)) {
		if (nPercent < 1) nPercent = 1;
		if (nPercent > 400) nPercent = 400;
		nBurnCPUSpeedAdjust = nPercent * 256 / 100;
	}
	ImGui::TextDisabled("1%% to 400%%, some games only follow it after a reset");

	ImGui::Spacing();
	float fButton = 110.0f * fUiScale;
	if (ImGui::Button("100%", ImVec2(fButton, 0.0f))) {
		nBurnCPUSpeedAdjust = 0x0100;
	}
	ImGui::SameLine();
	if (ImGui::Button("Close", ImVec2(fButton, 0.0f)) || UiEscapePressed()) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void UiGameDialogsDraw()
{
	if (nDialogRequest != DIALOG_NONE) {
		int nRequest = nDialogRequest;
		nDialogRequest = DIALOG_NONE;
		if (nRequest == DIALOG_CPU_SPEED) {
			ImGui::OpenPopup("CPU Speed###cpuspeed");
		} else {
			OpenBrowser(nRequest);
		}
	}

	char szFile[MAX_PATH];
	if (UiFileBrowserDraw(szBrowseTitle, szFile, sizeof(szFile))) {
		BrowserDone(szFile);
	}
	CpuSpeedDialog();
}

// ----------------------------------------------------------------------------
// The menu

static void RecentMenu()
{
	bool bAny = false;
	for (int i = 0; i < 10; i++) {
		int nDrv = GameListFind(szPrevGames[i]);
		if (nDrv < 0) {
			continue;
		}
		char szTitle[512];
		ImGui::PushID(i);
		if (ImGui::MenuItem(GameListTitle(nDrv, true, szTitle, sizeof(szTitle)))) {
			UiGameSelPlay(nDrv);
		}
		ImGui::PopID();
		bAny = true;
	}
	if (!bAny) {
		ImGui::TextDisabled("No game played yet");
	}
}

static void QuickLoadMenu()
{
	if (ImGui::MenuItem("ROM Archive...")) {
		nDialogRequest = DIALOG_ARCHIVE;
	}
	if (ImGui::MenuItem("CD Image (Neo Geo CD, PC Engine CD)...")) {
		nDialogRequest = DIALOG_CDIMAGE;
	}
	ImGui::Separator();
	int nMvs = GameListFind("neogeo");
	if (ImGui::MenuItem("Start Neo Geo MVS (Cartridge Slots)...", NULL, false, nMvs >= 0)) {
		UiGameSelPlay(nMvs);
	}
	int nCdz = GameListFind("neocdz");
	if (ImGui::MenuItem("Start Neo Geo CDZ", NULL, false, nCdz >= 0)) {
		if (CDEmuImage[0]) {
			UiGameSelPlay(nCdz);
		} else {
			nDialogRequest = DIALOG_CDIMAGE;
		}
	}
	if (CDEmuImage[0]) {
		ImGui::TextDisabled("CD: %s", UiText(CDEmuImage));
	}
}

static void SlotsMenu(bool bRunning)
{
	char szLabel[64];
	for (int i = 1; i <= 8; i++) {
		FILE* fp = bRunning ? fopen(StatedSlotFile(i), "rb") : NULL;
		snprintf(szLabel, sizeof(szLabel), "Slot %d%s", i, fp ? "" : "  (empty)");
		if (fp) fclose(fp);
		if (ImGui::MenuItem(szLabel, NULL, nSavestateSlot == i)) {
			nSavestateSlot = i;
		}
	}
}

static void StatesMenu(bool bRunning)
{
	char szLabel[64];

	snprintf(szLabel, sizeof(szLabel), "Load from Slot %d", nSavestateSlot);
	if (ImGui::MenuItem(szLabel, "F9", false, bRunning)) {
		StatedLoad(nSavestateSlot);
	}
	snprintf(szLabel, sizeof(szLabel), "Save to Slot %d", nSavestateSlot);
	if (ImGui::MenuItem(szLabel, "F10", false, bRunning)) {
		StatedSave(nSavestateSlot);
	}
	if (ImGui::MenuItem("Undo the Last Save of the Slot", "Shift+F8", false, bRunning)) {
		StatedUNDO(nSavestateSlot);
	}
	if (ImGui::BeginMenu("Active Slot")) {
		SlotsMenu(bRunning);
		ImGui::Separator();
		if (ImGui::MenuItem("Previous Slot", "F8")) {
			nSavestateSlot = ((nSavestateSlot + 6) % 8) + 1;
		}
		if (ImGui::MenuItem("Next Slot", "F7")) {
			nSavestateSlot = (nSavestateSlot % 8) + 1;
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Load State...", "Ctrl+F9")) {
		nDialogRequest = DIALOG_STATE_LOAD;
	}
	if (ImGui::MenuItem("Save State...", "Ctrl+F10", false, bRunning)) {
		nDialogRequest = DIALOG_STATE_SAVE;
	}
	ImGui::Separator();
	bool bFull = bDrvSaveAll != 0;
	if (ImGui::MenuItem("Restore the Full State on Load", NULL, &bFull)) {
		bDrvSaveAll = bFull;
	}
}

static void MemCardMenu()
{
	int nPgm2 = MemCardPgm2Slots();
	if (nPgm2 > 0) {
		// IC cards, one per player
		for (int i = 0; i < nPgm2 && i < 4; i++) {
			char szLabel[32];
			snprintf(szLabel, sizeof(szLabel), "Player %d", i + 1);
			ImGui::PushID(i);
			if (ImGui::BeginMenu(szLabel)) {
				if (ImGui::MenuItem("Create New Card")) {
					char szFile[MAX_PATH];
					snprintf(szFile, sizeof(szFile), "%s%s_card_p%d.bin", MemCardFolder(), BurnDrvGetTextA(DRV_NAME), i + 1);
					MemCardEjectPGM2Slot(i);
					if (MemCardCreatePGM2Slot(i, szFile) == 0) {
						MemCardInsertPGM2Slot(i);
					}
				}
				if (ImGui::MenuItem("Select Card...")) {
					nPgm2Slot = i;
					nDialogRequest = DIALOG_PGM2_SELECT;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Insert", NULL, false, (nPgm2CardStatus[i] & 1) && !(nPgm2CardStatus[i] & 2))) {
					MemCardInsertPGM2Slot(i);
				}
				if (ImGui::MenuItem("Eject", NULL, false, (nPgm2CardStatus[i] & 2) != 0)) {
					MemCardEjectPGM2Slot(i);
				}
				ImGui::EndMenu();
			}
			ImGui::PopID();
		}
		return;
	}

	if (ImGui::MenuItem("Create New Memory Card...")) {
		nDialogRequest = DIALOG_MEMCARD_CREATE;
	}
	if (ImGui::MenuItem("Select Memory Card...")) {
		nDialogRequest = DIALOG_MEMCARD_SELECT;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Insert", NULL, false, (nMemoryCardStatus & 1) && !(nMemoryCardStatus & 2))) {
		MemCardInsert();
	}
	if (ImGui::MenuItem("Eject", NULL, false, (nMemoryCardStatus & 2) != 0)) {
		MemCardEject();
	}
	if (nMemoryCardStatus & 1) {
		ImGui::TextDisabled("%s%s", UiText(szMemoryCardFile), (nMemoryCardStatus & 2) ? " (inserted)" : "");
	}
}

void UiGameMenu()
{
	bool bRunning = UiGameRunning();

	if (ImGui::MenuItem(bRunning ? "Load Another Game..." : "Load Game...", NULL, false, bRunning)) {
		UiGameSelOpen();
	}
	if (ImGui::BeginMenu("Recent Games")) {
		RecentMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Quick Load")) {
		QuickLoadMenu();
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Rescan ROMs", NULL, false, !bRunning)) {
		UiGameSelRescan();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Pause", "Pause", UiGamePaused(), bRunning)) {
		UiTogglePause();
	}
	if (ImGui::MenuItem("Frame Advance", "F1 (paused)", false, bRunning && UiGamePaused())) {
		bAppDoStep = 1;
	}
	if (ImGui::MenuItem("Fast Forward", "Shift+F1", bUiFastToggle != 0, bRunning)) {
		bUiFastToggle = !bUiFastToggle;
		bAppDoFast = bUiFastToggle;
	}
	if (ImGui::BeginMenu("Slow Motion")) {
		static const char* pszSpeeds[] = { "Normal", "75%", "50%", "25%", "17%" };
		for (int i = 0; i < 5; i++) {
			if (ImGui::MenuItem(pszSpeeds[i], NULL, nSlowMo == i)) {
				nSlowMo = i;
			}
		}
		ImGui::EndMenu();
	}
	char szSpeed[32];
	snprintf(szSpeed, sizeof(szSpeed), "%d%%", (nBurnCPUSpeedAdjust * 100 + 128) / 256);
	if (ImGui::MenuItem("CPU Speed...", szSpeed, false, bRunning)) {
		nDialogRequest = DIALOG_CPU_SPEED;
	}
	if (ImGui::MenuItem("Reset", NULL, false, bRunning)) {
		extern bool do_reset_game;
		do_reset_game = true;
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Save States")) {
		StatesMenu(bRunning);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Memory Card", MemCardSupported())) {
		MemCardMenu();
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Save Screenshot", "F6", false, bRunning)) {
		MakeScreenShot(0);
	}
	ImGui::Separator();
	if (ImGui::MenuItem("DIPs, Cheats, Input Mapping...", NULL, false, bRunning)) {
		UiOpenLegacyMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("End Game", "F12", false, bRunning)) {
		UiEndGame();
	}
	if (ImGui::MenuItem("Exit")) {
		UiQuit();
	}
}

// the hotkeys that open a dialog
void UiGameOpenStateDialog(bool bSave)
{
	nDialogRequest = bSave ? DIALOG_STATE_SAVE : DIALOG_STATE_LOAD;
}
