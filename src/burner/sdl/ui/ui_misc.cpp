// Misc and Help menus: the options, the dat files, the snapshots, the shot
// factory, the cheat search, the palette viewer, the About box,
// the system information and the license on the first run
#include "burner.h"
#include "version.h"
#include "ui.h"
#include "imgui.h"
#include "license_text.h"		// LicenseText[], made from src/license.txt

extern INT32 EnableHiscores;				// burn/hiscore.cpp
extern INT32 BurnShiftEnabled;				// burn/burn_shift.cpp
extern bool bBurnGunDrawReticles;			// burn/burn_gun.cpp
extern int vsync;							// main.cpp
extern char videofiltering[3];
extern SDL_Renderer* sdlRenderer;			// vid_sdl2.cpp
extern int nIniVersion;						// config.cpp
void CmdLineOptionsApply();

bool bUiShowModeIndicator = true;
bool bUiScanAtStart = true;
bool bUiAutoSaveInputs = true;
int nUiTheme = UI_THEME_DARK;

static bool bShortcutsRequest = false;
static bool bAboutRequest = false;
static bool bSysInfoRequest = false;
static bool bFirstRunRequest = false;

static bool bShotFactory = false;
static bool bCheatSearch = false;
static bool bPaletteViewer = false;

bool UiMiscCapture()
{
	return bShotFactory;
}

static void OpenUrl(const char* pszUrl)
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
	SDL_OpenURL(pszUrl);
#endif
}

// ----------------------------------------------------------------------------
// Snapshots: 0 screenshots/, 1 the title of the selector, 2 its preview

void UiSnapshot(int nType)
{
	if (!bDrvOkay) {
		return;
	}

	// the folder may not be there yet
	if (nType == 1) {
		MakeDirs(szAppTitlesPath);
	} else if (nType == 2) {
		MakeDirs(szAppPreviewsPath);
	}
#if defined(SDL_WINDOWS)
	else {
		MakeDirs("screenshots/");
	}
#endif

	char szText[64];
	int nRet = MakeScreenShot(nType);
	if (nRet) {
		snprintf(szText, sizeof(szText), "Error %d saving the snapshot", nRet);
	} else {
		static const char* pszSaved[3] = { "Screenshot saved", "Title snapshot saved", "Preview snapshot saved" };
		snprintf(szText, sizeof(szText), "%s", pszSaved[nType]);
		if (nType) {
			UiGameSelReloadImages();
		}
	}
	UpdateMessage(szText);
}

// ----------------------------------------------------------------------------
// Dat files, in szAppDatListsPath

static void GenerateDat(int nSystem)		// -1: all of them
{
	UiGameSelPauseScan();					// create_datfile() goes through nBurnDrvActive
	MakeDirs(szAppDatListsPath);

	int nWritten = 0;
	for (int i = 0; i < nDatSystems; i++) {
		if (nSystem >= 0 && i != nSystem) {
			continue;
		}
		char szFile[MAX_PATH * 2];
		snprintf(szFile, sizeof(szFile), "%s%s", szAppDatListsPath, DatSystems[i].pszFile);
		if (create_datfile(szFile, DatSystems[i].nType) == 0) {
			nWritten++;
		} else {
			FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Couldn't write %s.\n"), szFile);
		}
	}

	if (nWritten) {
		if (nSystem >= 0) {
			FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Written to %s%s"), szAppDatListsPath, DatSystems[nSystem].pszFile);
		} else {
			FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("%d dat files written to %s"), nWritten, szAppDatListsPath);
		}
	}
	FBAPopupDisplay(nWritten ? PUF_TYPE_INFO : PUF_TYPE_ERROR);
}

// ----------------------------------------------------------------------------
// Settings

static void ReloadSettings()
{
	bool bOldFullscreen = bAppFullscreen;
	int nOldVsync = vsync;
	char cOldFilter = videofiltering[0];

	ConfigAppLoad();
	CmdLineOptionsApply();					// the options of the command line are still in use

	ComputeGammaLUT();
	AudSoundSetVolume();
	UiApplyTheme();
	UiGameSelReloadImages();
	if (cOldFilter != videofiltering[0]) {
		VidSDL2SetFiltering();
	}
#if SDL_VERSION_ATLEAST(2, 0, 18)
	if (nOldVsync != vsync) {
		SDL_RenderSetVSync(sdlRenderer, vsync);
	}
#endif
	if (UiGameRunning()) {
		UiVideoReInit();					// software effect, image depth, gamma
	}
	if (bOldFullscreen != bAppFullscreen) {
		bool bFullscreen = bAppFullscreen;
		bAppFullscreen = bOldFullscreen;
		UiVideoSetFullscreen(bFullscreen);
	}
	UiRefitWindow();

	UpdateMessage((char*)"Settings reloaded");
}

// ----------------------------------------------------------------------------
// Menus

static void OptionItem(const char* pszLabel, bool* pbValue, bool bEnabled = true)
{
	if (ImGui::MenuItem(pszLabel, NULL, *pbValue, bEnabled)) {
		*pbValue = !*pbValue;
	}
}

static void OptionsMenu(bool bRunning)
{
	OptionItem("Pause when Inactive", &bUiPauseOnFocusLoss);
	OptionItem("Pause while a Menu is Open", &bUiPauseOnMenu);
	OptionItem("Display Mode Indicator", &bUiShowModeIndicator);
	OptionItem("Show Menu Bar in a Window", &bUiMenuPinned);
	ImGui::Separator();
	OptionItem("Always Start in the Game Selector", &bAlwaysMenu);
	OptionItem("Scan ROMs at Start-up (if needed)", &bUiScanAtStart);
	ImGui::Separator();
	if (ImGui::MenuItem("Save High Scores when Supported", NULL, EnableHiscores != 0)) {
		EnableHiscores = !EnableHiscores;
	}
	// the blend tables are read when the game starts
	OptionItem("Use Alpha Blend Effects when Supported", &bBurnUseBlend, !bRunning);
	if (ImGui::MenuItem("Display Gear-Shift Indicator when Supported", NULL, BurnShiftEnabled != 0)) {
		BurnShiftEnabled = !BurnShiftEnabled;
	}
	OptionItem("Display Lightgun Reticles when Supported", &bBurnGunDrawReticles);
}

void UiMiscMenu()
{
	bool bRunning = UiGameRunning();

	if (ImGui::BeginMenu("Options")) {
		OptionsMenu(bRunning);
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Generate Dat Files")) {
		for (int i = 0; i < nDatSystems; i++) {
			char szLabel[64];
			snprintf(szLabel, sizeof(szLabel), "%s...", DatSystems[i].pszName);
			if (ImGui::MenuItem(szLabel)) {
				GenerateDat(i);
			}
		}
		ImGui::Separator();
		if (ImGui::MenuItem("All of Them...")) {
			GenerateDat(-1);
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("ROM Paths...")) {
		UiRomPathsOpen();
	}
	if (ImGui::MenuItem("Support Paths...")) {
		UiSupportPathsOpen();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Interface Theme")) {
		for (int i = 0; i < UiThemeCount(); i++) {
			if (i == UI_THEME_CLASSIC + 1) {
				ImGui::Separator();
			}
			if (ImGui::MenuItem(UiThemeName(i), NULL, nUiTheme == i)) {
				nUiTheme = i;
				UiApplyTheme();
				UiRefitWindow();				// the height of the menu bar
			}
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Selector Icons")) {
		OptionItem("Show Icons", &bEnableIcons);
		ImGui::Separator();
		static const char* pszSizes[] = { "16 x 16", "24 x 24", "32 x 32" };
		for (int i = 0; i < 3; i++) {
			if (ImGui::MenuItem(pszSizes[i], NULL, nIconsSize == i, bEnableIcons)) {
				nIconsSize = i;
			}
		}
		ImGui::Separator();
		OptionItem("Only for the Parents", &bIconsOnlyParents, bEnableIcons && !bIconsByHardwares);
		OptionItem("By Hardware", &bIconsByHardwares, bEnableIcons);
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Save Snapshot", UiHotkeyName(UI_HK_SCREENSHOT), false, bRunning)) {
		UiSnapshot(0);
	}
	if (ImGui::MenuItem("Save Snapshot as Title", UiHotkeyName(UI_HK_SNAP_TITLE), false, bRunning)) {
		UiSnapshot(1);
	}
	if (ImGui::MenuItem("Save Snapshot as Preview", UiHotkeyName(UI_HK_SNAP_PREVIEW), false, bRunning)) {
		UiSnapshot(2);
	}
	if (ImGui::MenuItem("Shot Factory...", UiHotkeyName(UI_HK_SHOT_FACTORY), bShotFactory, bRunning)) {
		UiShotFactoryOpen();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Cheat Search...", NULL, bCheatSearch, bRunning)) {
		bCheatSearch = !bCheatSearch;
	}
	if (ImGui::MenuItem("Palette Viewer...", UiHotkeyName(UI_HK_PALETTE), bPaletteViewer, bRunning)) {
		bPaletteViewer = !bPaletteViewer;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Save Input Mapping Now", NULL, false, bRunning)) {
		ConfigGameSave(true);
		UpdateMessage((char*)"Input mapping saved");
	}
	OptionItem("Auto-Save Input Mapping", &bUiAutoSaveInputs);
	ImGui::Separator();
	if (ImGui::MenuItem("Save Settings Now")) {
		ConfigAppSave();
		UpdateMessage((char*)"Settings saved");
	}
	if (ImGui::MenuItem("Reload Settings")) {
		ReloadSettings();
	}
}

void UiHelpMenu()
{
	if (ImGui::MenuItem("Keyboard Shortcuts...")) {
		bShortcutsRequest = true;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("What's New")) {
		// the whatsnew.html of the Windows package, the releases otherwise
		FILE* fp = fopen("whatsnew.html", "rb");
		if (fp) {
			fclose(fp);
			char* pszBase = SDL_GetBasePath();
			char szUrl[MAX_PATH * 2];
			snprintf(szUrl, sizeof(szUrl), "file:///%swhatsnew.html", pszBase ? pszBase : "");
			SDL_free(pszBase);
			OpenUrl(szUrl);
		} else {
			OpenUrl("https://github.com/finalburnneo/FBNeo/releases");
		}
	}
	ImGui::Separator();
	if (ImGui::MenuItem("FB Neo Homepage")) {
		OpenUrl("https://neo-source.com/");
	}
	if (ImGui::MenuItem("FB Neo GitHub")) {
		OpenUrl("https://github.com/finalburnneo/FBNeo");
	}
	ImGui::Separator();
	if (ImGui::MenuItem("System Information...")) {
		bSysInfoRequest = true;
	}
	if (ImGui::MenuItem("About...")) {
		bAboutRequest = true;
	}
}

// ----------------------------------------------------------------------------
// Shot factory: the layers and the sprites of the paused game, for snapshots

static bool bShotKeep = false;				// the layers stay as they are after closing
static bool bShotOldPause = false;

void UiShotFactoryOpen()
{
	if (!UiGameRunning() || bShotFactory) {
		return;
	}
	bShotFactory = true;
	bShotKeep = false;
	bShotOldPause = UiGamePaused();
	UiSetPause(true);
}

static void ShotFactoryClose()
{
	if (!bShotFactory) {
		return;
	}
	bShotFactory = false;
	if (!bShotKeep) {
		nBurnLayer = 0xFF;
		nSpriteEnable = 0xFF;
		if (UiGameRunning()) {
			VidRedraw();
			VidPaint(0);
		}
	}
	if (UiGameRunning()) {
		UiSetPause(bShotOldPause);
	}
}

static void DrawShotFactory()
{
	if (!bShotFactory) {
		return;
	}
	if (!UiGameRunning()) {
		ShotFactoryClose();
		return;
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 20.0f * fUiScale, vp->WorkPos.y + 20.0f * fUiScale), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));
	bool bOpen = true;
	if (ImGui::Begin("Shot Factory", &bOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
		bool bChanged = false;

		ImGui::SeparatorText("Layers");
		for (int i = 0; i < 4; i++) {
			char szLabel[16];
			snprintf(szLabel, sizeof(szLabel), "%d##layer", i + 1);
			bool bOn = (nBurnLayer & (1 << i)) != 0;
			if (i) ImGui::SameLine();
			if (ImGui::Checkbox(szLabel, &bOn)) {
				nBurnLayer ^= 1 << i;
				bChanged = true;
			}
		}
		ImGui::SeparatorText("Sprites");
		for (int i = 0; i < 8; i++) {
			char szLabel[16];
			snprintf(szLabel, sizeof(szLabel), "%d##sprite", i + 1);
			bool bOn = (nSpriteEnable & (1 << i)) != 0;
			if (i & 3) ImGui::SameLine();
			if (ImGui::Checkbox(szLabel, &bOn)) {
				nSpriteEnable ^= 1 << i;
				bChanged = true;
			}
		}
		if (bChanged) {
			VidRedraw();					// the drivers without a redraw run a frame
			VidPaint(0);
		}

		ImGui::Separator();
		ImGui::Checkbox("Keep these settings", &bShotKeep);
		ImGui::Spacing();
		float fButton = 120.0f * fUiScale;
		if (ImGui::Button("Capture", ImVec2(fButton, 0.0f))) {
			UiSnapshot(0);
		}
		ImGui::SameLine();
		if (ImGui::Button("Advance Frame", ImVec2(fButton, 0.0f))) {
			bAppDoStep = 1;
		}
		if (ImGui::Button("Close", ImVec2(fButton * 2.0f + ImGui::GetStyle().ItemSpacing.x, 0.0f))) {
			bOpen = false;
		}
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && UiEscapePressed()) {
			bOpen = false;
		}
	}
	ImGui::End();

	if (!bOpen) {
		ShotFactoryClose();
	}
}

// ----------------------------------------------------------------------------
// Cheat search (cheat.cpp), on the memory of the first CPU. The game runs
// between two steps of the search.

static int nSearchDrv = -1;					// the game of the search, -1 none
static int nSearchMatches = -1;				// after the last step, -1 none yet

static void CheatSearchStep(UINT32 (*pStep)())
{
	nSearchMatches = pStep();
}

static void DrawCheatSearch()
{
	if (!bCheatSearch) {
		return;
	}
	if (!UiGameRunning()) {
		bCheatSearch = false;				// BurnDrvExit() has ended the search
		nSearchDrv = -1;
		return;
	}
	if (nSearchDrv >= 0 && (UINT32)nSearchDrv != nBurnDrvActive) {
		nSearchDrv = -1;					// another game
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 20.0f * fUiScale, vp->WorkPos.y + 20.0f * fUiScale), ImGuiCond_Appearing);
	bool bOpen = true;
	if (ImGui::Begin("Cheat Search", &bOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
		float fButton = 130.0f * fUiScale;
		bool bSearching = nSearchDrv >= 0;

		ImGui::TextDisabled("Play the game between two steps of the search.");
		if (ImGui::Button(bSearching ? "New Search" : "Start Search", ImVec2(fButton * 2.0f + ImGui::GetStyle().ItemSpacing.x, 0.0f))) {
			if (bSearching) {
				CheatSearchExit();
			}
			if (CheatSearchStart() == 0) {
				nSearchDrv = nBurnDrvActive;
				nSearchMatches = -1;
				UpdateMessage((char*)"Cheat search started");
			} else {
				nSearchDrv = -1;
				UpdateMessage((char*)"The memory of this game can't be searched");
			}
			bSearching = nSearchDrv >= 0;
		}

		ImGui::SeparatorText("The value has");
		ImGui::BeginDisabled(!bSearching);
		if (ImGui::Button("Not Changed", ImVec2(fButton, 0.0f))) CheatSearchStep(CheatSearchValueNoChange);
		ImGui::SameLine();
		if (ImGui::Button("Changed", ImVec2(fButton, 0.0f))) CheatSearchStep(CheatSearchValueChange);
		if (ImGui::Button("Decreased", ImVec2(fButton, 0.0f))) CheatSearchStep(CheatSearchValueDecreased);
		ImGui::SameLine();
		if (ImGui::Button("Increased", ImVec2(fButton, 0.0f))) CheatSearchStep(CheatSearchValueIncreased);
		ImGui::EndDisabled();

		ImGui::Separator();
		if (!bSearching) {
			ImGui::TextDisabled("No search");
		} else if (nSearchMatches < 0) {
			ImGui::TextUnformatted("All the addresses");
		} else {
			ImGui::Text("%d matching address%s", nSearchMatches, nSearchMatches == 1 ? "" : "es");
			// the core keeps the first CHEATSEARCH_SHOWRESULTS of them
			if (nSearchMatches > 0 && nSearchMatches <= CHEATSEARCH_SHOWRESULTS && ImGui::BeginTable("##results", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
				ImGui::TableSetupColumn("Address");
				ImGui::TableSetupColumn("Value");
				ImGui::TableHeadersRow();
				for (int i = 0; i < nSearchMatches; i++) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%08X", CheatSearchShowResultAddresses[i]);
					ImGui::TableNextColumn();
					ImGui::Text("%02X (%d)", CheatSearchShowResultValues[i], CheatSearchShowResultValues[i]);
				}
				ImGui::EndTable();
			}
		}

		ImGui::Spacing();
		ImGui::BeginDisabled(!bSearching);
		if (ImGui::Button("Dump to File", ImVec2(fButton, 0.0f))) {
			CheatSearchDumptoFile();
			UpdateMessage((char*)"Search written to cheatsearchdump.txt");
		}
		ImGui::SameLine();
		if (ImGui::Button("End Search", ImVec2(fButton, 0.0f))) {
			CheatSearchExit();
			nSearchDrv = -1;
			UpdateMessage((char*)"Cheat search ended");
		}
		ImGui::EndDisabled();
	}
	ImGui::End();

	if (!bOpen) {
		bCheatSearch = false;				// the search goes on until it's ended or the game is over
	}
}

// ----------------------------------------------------------------------------
// Palette viewer: 256 colours of pBurnDrvPalette at a time

static int nPalettePos = 0;
static int nPaletteSelected = -1;

static UINT32 PaletteRgb(int nIndex)
{
	bool bCps = false;
	switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) {
		case HARDWARE_CAPCOM_CPS1:
		case HARDWARE_CAPCOM_CPS1_QSOUND:
		case HARDWARE_CAPCOM_CPS1_GENERIC:
		case HARDWARE_CAPCOM_CPSCHANGER:
		case HARDWARE_CAPCOM_CPS2:
			bCps = true;
			break;
	}
	UINT32 c = pBurnDrvPalette[bCps ? (nIndex ^ 0xF) : nIndex];
	int r, g, b;

	// the colours are in the format of the image (SetBurnHighCol())
	if (nVidImageDepth < 16 || (BurnDrvGetFlags() & BDF_16BIT_ONLY)) {
		r = (c & 0x7C00) >> 7;
		g = (c & 0x03E0) >> 2;
		b = (c & 0x001F) << 3;
	} else if (nVidImageDepth == 16) {
		r = (c & 0xF800) >> 8;
		g = (c & 0x07E0) >> 3;
		b = (c & 0x001F) << 3;
	} else {
		r = (c >> 16) & 0xFF;
		g = (c >> 8) & 0xFF;
		b = c & 0xFF;
	}

	return (r << 16) | (g << 8) | b;
}

static void DrawPaletteViewer()
{
	if (!bPaletteViewer) {
		return;
	}
	if (!UiGameRunning()) {
		bPaletteViewer = false;
		nPalettePos = 0;
		nPaletteSelected = -1;
		return;
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	bool bOpen = true;
	if (ImGui::Begin("Palette Viewer", &bOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
		int nEntries = BurnDrvGetPaletteEntries();
		if (pBurnDrvPalette == NULL || nEntries <= 0) {
			ImGui::TextUnformatted("This game has no palette.");
		} else {
			if (nPalettePos >= nEntries) {
				nPalettePos = 0;
			}

			float fCell = 18.0f * fUiScale;
			float fGap = 2.0f * fUiScale;
			float fLabel = ImGui::CalcTextSize("00000").x + 6.0f * fUiScale;
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 origin = ImGui::GetCursorScreenPos();
			float fLine = ImGui::GetTextLineHeight();

			// the column numbers, then the rows with their first colour
			for (int x = 0; x < 16; x++) {
				char szHex[4];
				snprintf(szHex, sizeof(szHex), "%X", x);
				float w = ImGui::CalcTextSize(szHex).x;
				dl->AddText(ImVec2(origin.x + fLabel + x * (fCell + fGap) + (fCell - w) * 0.5f, origin.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), szHex);
			}
			ImVec2 grid(origin.x + fLabel, origin.y + fLine + fGap);
			for (int y = 0; y < 16; y++) {
				char szHex[8];
				snprintf(szHex, sizeof(szHex), "%05X", nPalettePos + y * 16);
				dl->AddText(ImVec2(origin.x, grid.y + y * (fCell + fGap) + (fCell - fLine) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), szHex);
				for (int x = 0; x < 16; x++) {
					int i = nPalettePos + y * 16 + x;
					ImVec2 p(grid.x + x * (fCell + fGap), grid.y + y * (fCell + fGap));
					if (i >= nEntries) {
						continue;
					}
					UINT32 rgb = PaletteRgb(i);
					dl->AddRectFilled(p, ImVec2(p.x + fCell, p.y + fCell), IM_COL32(rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF, 255));
					if (i == nPaletteSelected) {
						dl->AddRect(ImVec2(p.x - 1, p.y - 1), ImVec2(p.x + fCell + 1, p.y + fCell + 1), ImGui::GetColorU32(ImGuiCol_Text), 0.0f, 0, 2.0f);
					}
				}
			}
			ImVec2 size(fLabel + 16 * (fCell + fGap), fLine + fGap + 16 * (fCell + fGap));
			ImGui::InvisibleButton("##grid", size);
			if (ImGui::IsItemClicked()) {
				ImVec2 m = ImGui::GetIO().MousePos;
				int x = (int)((m.x - grid.x) / (fCell + fGap));
				int y = (int)((m.y - grid.y) / (fCell + fGap));
				if (m.x >= grid.x && m.y >= grid.y && x < 16 && y < 16 && nPalettePos + y * 16 + x < nEntries) {
					nPaletteSelected = nPalettePos + y * 16 + x;
				}
			}

			if (nPaletteSelected >= 0 && nPaletteSelected < nEntries) {
				ImGui::Text("Selected colour: #%X  RGB #%06X", nPaletteSelected, PaletteRgb(nPaletteSelected));
			} else {
				ImGui::TextDisabled("Click a colour to see its value.");
			}

			float fButton = 100.0f * fUiScale;
			if (ImGui::Button("< Previous", ImVec2(fButton, 0.0f))) {
				nPalettePos -= 0x100;
				if (nPalettePos < 0) nPalettePos = ((nEntries - 1) / 0x100) * 0x100;	// the last page
			}
			ImGui::SameLine();
			if (ImGui::Button("Next >", ImVec2(fButton, 0.0f))) {
				nPalettePos += 0x100;
				if (nPalettePos >= nEntries) nPalettePos = 0;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%X - %X of %X", nPalettePos, (nPalettePos + 0xFF < nEntries ? nPalettePos + 0xFF : nEntries - 1), nEntries);
		}
	}
	ImGui::End();

	if (!bOpen) {
		bPaletteViewer = false;
	}
}

void UiPaletteViewerToggle()
{
	if (UiGameRunning()) {
		bPaletteViewer = !bPaletteViewer;
	}
}

// ----------------------------------------------------------------------------
// Keyboard shortcuts, About, system information, first run

static bool CloseButton()
{
	float fButton = 120.0f * fUiScale;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - fButton);
	bool bClose = ImGui::Button("Close", ImVec2(fButton, 0.0f)) || UiEscapePressed();
	ImGui::SetItemDefaultFocus();
	return bClose;
}

static void DrawShortcuts()
{
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	bool bOpen = true;
	if (ImGui::BeginPopupModal("Keyboard Shortcuts", &bOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		UiHotkeysTable();						// the current keys (Input > Hotkeys)
		ImGui::TextDisabled("Change them in Input > Hotkeys.");
		if (CloseButton()) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

static void DrawLicense(float fWidth, float fHeight)
{
	ImGui::BeginChild("##license", ImVec2(fWidth, fHeight), ImGuiChildFlags_Borders);
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted((const char*)LicenseText);
	ImGui::PopTextWrapPos();
	ImGui::EndChild();
}

static const char* CompilerName()
{
	static char szName[64];
#if defined(__clang__)
	snprintf(szName, sizeof(szName), "Clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
	snprintf(szName, sizeof(szName), "MSVC %d", _MSC_FULL_VER);
#elif defined(__GNUC__)
	snprintf(szName, sizeof(szName), "GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
	snprintf(szName, sizeof(szName), "unknown compiler");
#endif
	return szName;
}

static void DrawAbout()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	bool bOpen = true;
	if (ImGui::BeginPopupModal("About###about", &bOpen, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
		static bool bShowLicense = false;
		if (ImGui::IsWindowAppearing()) {
			bShowLicense = false;
		}
		SDL_version linked;
		SDL_GetVersion(&linked);

		ImGui::Text(APP_TITLE " v%s", szAppBurnVer);
		ImGui::TextDisabled("Emulator for arcade games and select consoles, based on FinalBurn and old MAME versions");
		ImGui::Spacing();
		ImGui::Text("SDL2 frontend, SDL %d.%d.%d, Dear ImGui %s", linked.major, linked.minor, linked.patch, IMGUI_VERSION);
		ImGui::Text("Built with %s, %d-bit", CompilerName(), (int)(sizeof(void*) * 8));
		ImGui::Spacing();
		ImGui::TextLinkOpenURL("https://neo-source.com/");
		ImGui::TextLinkOpenURL("https://github.com/finalburnneo/FBNeo");
		ImGui::Spacing();
		if (bShowLicense) {
			DrawLicense(UiDialogSize(560.0f, 0.0f).x, vp->WorkSize.y * 0.5f);
		} else if (ImGui::Button("Show the License")) {
			bShowLicense = true;
		}
		ImGui::Spacing();
		if (CloseButton()) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

static int SysInfoAdd(char* pszText, int nPos, int nSize, const char* pszFormat, ...)
{
	if (nPos >= nSize - 1) {
		return nPos;
	}
	va_list args;
	va_start(args, pszFormat);
	int n = vsnprintf(pszText + nPos, nSize - nPos, pszFormat, args);
	va_end(args);
	if (n < 0) {
		return nPos;
	}
	return (nPos + n < nSize) ? nPos + n : nSize - 1;
}

static void MakeSysInfo(char* pszText, int nSize)
{
	int n = 0;
	SDL_version compiled, linked;
	SDL_VERSION(&compiled);
	SDL_GetVersion(&linked);

	n = SysInfoAdd(pszText, n, nSize, APP_TITLE " v%s (%s, %d-bit)\n", szAppBurnVer, CompilerName(), (int)(sizeof(void*) * 8));
	n = SysInfoAdd(pszText, n, nSize, "SDL %d.%d.%d (built with %d.%d.%d), Dear ImGui %s\n\n", linked.major, linked.minor, linked.patch, compiled.major, compiled.minor, compiled.patch, IMGUI_VERSION);

	n = SysInfoAdd(pszText, n, nSize, "Platform: %s\n", SDL_GetPlatform());
	n = SysInfoAdd(pszText, n, nSize, "CPU: %d logical cores, %s%s%s%s%s\n", SDL_GetCPUCount(),
		SDL_HasSSE2() ? "SSE2 " : "", SDL_HasSSE41() ? "SSE4.1 " : "", SDL_HasAVX() ? "AVX " : "", SDL_HasAVX2() ? "AVX2 " : "", SDL_HasNEON() ? "NEON " : "");
	n = SysInfoAdd(pszText, n, nSize, "Memory: %d MB\n\n", SDL_GetSystemRAM());

	n = SysInfoAdd(pszText, n, nSize, "Video driver: %s\n", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none");
	SDL_RendererInfo info;
	if (sdlRenderer && SDL_GetRendererInfo(sdlRenderer, &info) == 0) {
		n = SysInfoAdd(pszText, n, nSize, "Renderer: %s, textures up to %dx%d%s\n", info.name, info.max_texture_width, info.max_texture_height, vsync ? ", vsync" : "");
	}
	for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
		SDL_DisplayMode mode;
		float fDpi = 0.0f;
		if (SDL_GetCurrentDisplayMode(i, &mode) == 0) {
			SDL_GetDisplayDPI(i, &fDpi, NULL, NULL);
			n = SysInfoAdd(pszText, n, nSize, "Display %d: %s, %dx%d %d Hz, %.0f dpi\n", i + 1, SDL_GetDisplayName(i) ? SDL_GetDisplayName(i) : "", mode.w, mode.h, mode.refresh_rate, fDpi);
		}
	}
	n = SysInfoAdd(pszText, n, nSize, "\nAudio driver: %s, %s, %d Hz\n", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none",
		szAudSDLDevice[0] ? szAudSDLDevice : "default device", nAudSampleRate[0]);

	int nJoysticks = SDL_NumJoysticks();
	n = SysInfoAdd(pszText, n, nSize, "Game controllers: %d\n", nJoysticks);
	for (int i = 0; i < nJoysticks; i++) {
		const char* pszName = SDL_IsGameController(i) ? SDL_GameControllerNameForIndex(i) : SDL_JoystickNameForIndex(i);
		n = SysInfoAdd(pszText, n, nSize, "  %d: %s%s\n", i + 1, pszName ? pszName : "", SDL_IsGameController(i) ? "" : " (joystick)");
	}

	if (UiGameRunning()) {
		int w, h;
		BurnDrvGetVisibleSize(&w, &h);
		n = SysInfoAdd(pszText, n, nSize, "\nGame: %s (%s), %dx%d, %d.%02d Hz, %d-bit image\n", UiText(BurnDrvGetTextA(DRV_FULLNAME)), BurnDrvGetTextA(DRV_NAME),
			w, h, nBurnFPS / 100, nBurnFPS % 100, nVidImageDepth);
	}
}

static void DrawSysInfo()
{
	static char szInfo[4096];
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(UiDialogSize(620.0f, vp->WorkSize.y * 0.7f), ImGuiCond_Appearing);
	bool bOpen = true;
	if (ImGui::BeginPopupModal("System Information###sysinfo", &bOpen, ImGuiWindowFlags_NoSavedSettings)) {
		if (ImGui::IsWindowAppearing()) {
			MakeSysInfo(szInfo, sizeof(szInfo));
		}
		float fBottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
		ImGui::BeginChild("##info", ImVec2(0.0f, -fBottom), ImGuiChildFlags_Borders);
		ImGui::TextUnformatted(szInfo);
		ImGui::EndChild();
		if (ImGui::Button("Copy to Clipboard")) {
			SDL_SetClipboardText(szInfo);
		}
		ImGui::SameLine();
		if (CloseButton()) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

// The license has to be accepted once per version, as in the Win32 build
void UiFirstRunCheck()
{
	if (nIniVersion < nBurnVer) {
		bFirstRunRequest = true;
	}
}

static void DrawFirstRun()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Welcome###firstrun", NULL, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
		static bool bAccepted = false;
		float fWidth = UiDialogSize(620.0f, 0.0f).x;

		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + fWidth);
		ImGui::Text("This appears to be the first time you run %s v%s.", APP_TITLE, szAppBurnVer);
		ImGui::Spacing();
		ImGui::TextUnformatted("Please take some time to match the audio/video settings to your system capabilities and personal preferences. "
			"If you continue to use and/or distribute this software, you must agree to be bound by the terms of the license agreement, which is always accessible via Help > About.");
#if VER_ALPHA > 0 && (VER_ALPHA < 90 || VER_BETA < 99)
		ImGui::Spacing();
		ImGui::TextUnformatted("Note that this is alpha quality software. As such, it likely will exhibit problems, including audio/video options not behaving correctly, "
			"the emulation of some games misses some features, and may exhibit stability problems.");
#elif VER_BETA > 0 && VER_BETA < 99
		ImGui::Spacing();
		ImGui::TextUnformatted("Note that this is beta quality software. As such, it likely may be missing some features, or exhibit some problems with your system.");
#endif
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		DrawLicense(fWidth, vp->WorkSize.y * 0.4f);
		ImGui::Checkbox("I accept the terms of the license", &bAccepted);
		ImGui::Spacing();

		float fButton = 120.0f * fUiScale;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + fWidth - fButton * 2.0f - ImGui::GetStyle().ItemSpacing.x);
		ImGui::BeginDisabled(!bAccepted);
		if (ImGui::Button("OK", ImVec2(fButton, 0.0f))) {
			nIniVersion = nBurnVer;			// saved with the ini
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Quit", ImVec2(fButton, 0.0f))) {
			extern bool bSaveconfig;		// main.cpp, so the question comes back next time
			bSaveconfig = false;
			UiQuit();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void UiMiscDialogsDraw()
{
	if (bShortcutsRequest) {
		bShortcutsRequest = false;
		ImGui::OpenPopup("Keyboard Shortcuts");
	}
	if (bAboutRequest) {
		bAboutRequest = false;
		ImGui::OpenPopup("About###about");
	}
	if (bSysInfoRequest) {
		bSysInfoRequest = false;
		ImGui::OpenPopup("System Information###sysinfo");
	}
	if (bFirstRunRequest && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
		bFirstRunRequest = false;
		ImGui::OpenPopup("Welcome###firstrun");
	}

	DrawShortcuts();
	DrawAbout();
	DrawSysInfo();
	DrawFirstRun();

	DrawShotFactory();
	DrawCheatSearch();
	DrawPaletteViewer();
}
