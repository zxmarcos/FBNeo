// Main menu bar
#include "burner.h"
#include "ui.h"
#include "imgui.h"

extern bool do_reset_game;					// inp_sdl2.cpp
extern INT32 AutoFireRateList[6];			// sdl2_gui_ingame.cpp
extern char AutoFireRateLabels[6][10];
extern INT32 EnableHiscores;
extern INT32 nSocd[6];						// burn/devices/joyprocess.cpp

static float fReserved = 0.0f;				// taken from the game area
static bool bMenusOpen = false;
static bool bOpenAbout = false;
static bool bOpenShortcuts = false;

float UiMenuBarHeight()
{
	return fReserved;
}

static void GameMenu()
{
	UiGameMenu();					// ui_game.cpp
}

static void InputMenu()
{
	bool bRunning = UiGameRunning();

	if (ImGui::MenuItem("Map Game Inputs...", UiHotkeyName(UI_HK_MAP_INPUTS), false, bRunning)) {
		UiInputMappingOpen();
	}
	if (ImGui::MenuItem("DIP Switches...", UiHotkeyName(UI_HK_DIPS), false, bRunning)) {
		UiDipsOpen();
	}
	if (ImGui::MenuItem("Cheats...", UiHotkeyName(UI_HK_CHEATS), false, bRunning)) {
		UiCheatsOpen();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Autofire Rate")) {
		for (int i = 0; i < 6; i++) {
			if (ImGui::MenuItem(AutoFireRateLabels[i], NULL, nAutoFireRate == AutoFireRateList[i])) {
				nAutoFireRate = AutoFireRateList[i];
			}
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("SOCD (Opposite Directions)")) {
		// joyprocess.cpp: what the games that use it do with left+right / up+down
		static const char* pszModes[] = { "Disabled", "SOCD-N (neutral)", "SOCD-L (last, 4-way)", "SOCD-L (last, 8-way)", "SOCD-F (first)", "SOCD-U (up wins)", "SOCD-D+L" };
		for (int p = 0; p < 6; p++) {
			char szPlayer[16];
			snprintf(szPlayer, sizeof(szPlayer), "Player %d", p + 1);
			if (ImGui::BeginMenu(szPlayer)) {
				for (int m = 0; m < 7; m++) {
					if (ImGui::MenuItem(pszModes[m], NULL, nSocd[p] == m)) {
						nSocd[p] = m;
					}
				}
				ImGui::EndMenu();
			}
		}
		ImGui::Separator();
		if (ImGui::MenuItem("All Players: SOCD-L (8-way)")) {
			for (int p = 0; p < 6; p++) nSocd[p] = 3;
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Rewind")) {
		// the buffer is made when the game starts, StateRewindReInit() makes it again
		if (ImGui::MenuItem("Enable Rewind", UiHotkeyName(UI_HK_REWIND), bRewindEnabled != 0)) {
			bRewindEnabled = !bRewindEnabled;
			if (bRunning) StateRewindReInit();
		}
		ImGui::Separator();
		static const int nSizes[] = { 128, 256, 512, 768, 1024 };
		for (int i = 0; i < 5; i++) {
			char szLabel[32];
			snprintf(szLabel, sizeof(szLabel), nSizes[i] < 1024 ? "%d MB" : "1 GB", nSizes[i]);
			if (ImGui::MenuItem(szLabel, NULL, nRewindMemory == nSizes[i], bRewindEnabled != 0)) {
				nRewindMemory = nSizes[i];
				if (bRunning) StateRewindReInit();
			}
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Hotkeys...")) {
		UiHotkeysOpen();
	}
	if (ImGui::MenuItem("Detect Gamepads", NULL, false, bRunning)) {
		UiInputRedetect();
	}
}

static void MiscMenu()
{
	if (ImGui::MenuItem("Show Menu Bar in a Window", NULL, bUiMenuPinned)) {
		bUiMenuPinned = !bUiMenuPinned;
	}
	if (ImGui::MenuItem("Pause while a Menu is Open", NULL, bUiPauseOnMenu)) {
		bUiPauseOnMenu = !bUiPauseOnMenu;
	}
	if (ImGui::MenuItem("Pause when Inactive", NULL, bUiPauseOnFocusLoss)) {
		bUiPauseOnFocusLoss = !bUiPauseOnFocusLoss;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Always Start in the Game Selector", NULL, bAlwaysMenu)) {
		bAlwaysMenu = !bAlwaysMenu;
	}
	if (ImGui::MenuItem("Save High Scores", NULL, EnableHiscores != 0)) {
		EnableHiscores = !EnableHiscores;
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Selector Icons")) {
		if (ImGui::MenuItem("Show Icons", NULL, bEnableIcons)) {
			bEnableIcons = !bEnableIcons;
		}
		ImGui::Separator();
		static const char* pszSizes[] = { "16 x 16", "24 x 24", "32 x 32" };
		for (int i = 0; i < 3; i++) {
			if (ImGui::MenuItem(pszSizes[i], NULL, nIconsSize == i, bEnableIcons)) {
				nIconsSize = i;
			}
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Only for the Parents", NULL, bIconsOnlyParents, bEnableIcons && !bIconsByHardwares)) {
			bIconsOnlyParents = !bIconsOnlyParents;
		}
		if (ImGui::MenuItem("By Hardware", NULL, bIconsByHardwares, bEnableIcons)) {
			bIconsByHardwares = !bIconsByHardwares;
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("ROM Paths...")) {
		UiRomPathsOpen();
	}
	if (ImGui::MenuItem("Support Paths...")) {
		UiSupportPathsOpen();
	}
}

static void HelpMenu()
{
	if (ImGui::MenuItem("Keyboard Shortcuts...")) {
		bOpenShortcuts = true;
	}
	if (ImGui::MenuItem("About...")) {
		bOpenAbout = true;
	}
}

static void ShortcutsPopup()
{
	if (bOpenShortcuts) {
		ImGui::OpenPopup("Keyboard Shortcuts");
		bOpenShortcuts = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	bool bOpen = true;
	if (ImGui::BeginPopupModal("Keyboard Shortcuts", &bOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		UiHotkeysTable();						// the current keys (Input > Hotkeys)
		ImGui::TextDisabled("Change them in Input > Hotkeys.");
		if (ImGui::Button("Close") || UiEscapePressed()) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

static void AboutPopup()
{
	if (bOpenAbout) {
		ImGui::OpenPopup("About");
		bOpenAbout = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	bool bOpen = true;
	if (ImGui::BeginPopupModal("About", &bOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		SDL_version linked;
		SDL_GetVersion(&linked);

		ImGui::Text(APP_TITLE " v%s", szAppBurnVer);
		ImGui::Spacing();
		ImGui::Text("SDL2 frontend, SDL %d.%d.%d, Dear ImGui %s", linked.major, linked.minor, linked.patch, IMGUI_VERSION);
		ImGui::TextUnformatted("https://github.com/finalburnneo/FBNeo");
		ImGui::Spacing();
		ImGui::TextUnformatted("Usage is restricted by the license in src/license.txt");
		ImGui::Spacing();
		if (ImGui::Button("Close") || UiEscapePressed()) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

void UiMenuBarDraw()
{
	ImGuiIO& io = ImGui::GetIO();
	float fBar = ImGui::GetFrameHeight();
	bool bRunning = UiGameRunning();

	// with a game in fullscreen (or not pinned) the bar floats over the game,
	// and shows up when the mouse goes to the top or the game menu is open
	bool bPinned = !bRunning || (bUiMenuPinned && !bAppFullscreen);
	bool bShow = bPinned || bMenusOpen || UiOverlayShown() || (ImGui::IsMousePosValid() && io.MousePos.y < fBar);

	fReserved = (bRunning && bPinned) ? fBar : 0.0f;

	bMenusOpen = false;
	if (bShow && ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Game")) {
			GameMenu();
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		if (ImGui::BeginMenu("Video")) {
			UiVideoMenu();				// ui_video.cpp
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		if (ImGui::BeginMenu("Audio")) {
			UiAudioMenu();
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		if (ImGui::BeginMenu("Input")) {
			InputMenu();
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		if (ImGui::BeginMenu("Misc")) {
			MiscMenu();
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		if (ImGui::BeginMenu("Help")) {
			HelpMenu();
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		ImGui::EndMainMenuBar();
	}

	// opened out of the menus, so they don't live in the menu's ID scope
	ShortcutsPopup();
	AboutPopup();
}
