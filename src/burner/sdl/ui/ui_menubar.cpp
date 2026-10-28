// Main menu bar
#include "burner.h"
#include "ui.h"
#include "imgui.h"

extern int vsync;
extern char videofiltering[3];
extern bool do_reset_game;					// inp_sdl2.cpp
extern INT32 AutoFireRateList[6];			// sdl2_gui_ingame.cpp
extern char AutoFireRateLabels[6][10];
extern INT32 EnableHiscores;
extern SDL_Renderer* sdlRenderer;
void VidSDL2SetFiltering();

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

static void VideoMenu()
{
	if (ImGui::MenuItem("Fullscreen", "Alt+Enter", bAppFullscreen)) {
		UiToggleFullscreen();
	}
	if (ImGui::BeginMenu("Window Size", !bAppFullscreen)) {
		for (int i = 1; i <= 5; i++) {
			char szLabel[8];
			snprintf(szLabel, sizeof(szLabel), "%dx", i);
			if (ImGui::MenuItem(szLabel, NULL, nWindowScale == i)) {
				UiSetWindowScale(i);
			}
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Stretch to Fill", NULL, bVidFullStretch != 0)) {
		bVidFullStretch = !bVidFullStretch;
	}
	if (ImGui::MenuItem("Integer Scaling", NULL, bIntegerScale, !bVidFullStretch)) {
		bIntegerScale = !bIntegerScale;
	}
	if (ImGui::BeginMenu("Filtering")) {
		static const char* pszFilters[] = { "Nearest", "Linear", "Best" };
		for (int i = 0; i < 3; i++) {
			if (ImGui::MenuItem(pszFilters[i], NULL, videofiltering[0] == '0' + i)) {
				snprintf(videofiltering, 3, "%d", i);
				VidSDL2SetFiltering();
			}
		}
		ImGui::EndMenu();
	}
#if SDL_VERSION_ATLEAST(2, 0, 18)
	if (ImGui::MenuItem("VSync", NULL, vsync != 0)) {
		vsync = !vsync;
		SDL_RenderSetVSync(sdlRenderer, vsync);
	}
#endif
	ImGui::Separator();
	if (ImGui::MenuItem("Auto Frameskip", NULL, !bAlwaysDrawFrames)) {
		bAlwaysDrawFrames = !bAlwaysDrawFrames;
	}
	if (ImGui::MenuItem("Run-Ahead (one frame less of lag)", NULL, bRunAhead != 0)) {
		bRunAhead = !bRunAhead;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Show FPS", "F11", bAppShowFPS)) {
		bAppShowFPS = !bAppShowFPS;
	}
}

static void AudioMenu()
{
	int nVolume = nAudVolume / 100;
	ImGui::SetNextItemWidth(160.0f * fUiScale);
	if (ImGui::SliderInt("Volume", &nVolume, 0, 100, "%d%%")) {
		nAudVolume = nVolume * 100;
		AudSoundSetVolume();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Sample Rate")) {
		static const int nRates[] = { 11025, 22050, 44100, 48000 };
		for (int i = 0; i < 4; i++) {
			char szLabel[16];
			snprintf(szLabel, sizeof(szLabel), "%d Hz", nRates[i]);
			if (ImGui::MenuItem(szLabel, NULL, nAudSampleRate[0] == nRates[i])) {
				nAudSampleRate[0] = nRates[i];
			}
		}
		if (UiGameRunning()) {
			ImGui::TextDisabled("Used from the next game on");
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Low-Pass Filter", NULL, nAudDSPModule[0] != 0)) {
		nAudDSPModule[0] = nAudDSPModule[0] ? 0 : 1;
	}
}

static void InputMenu()
{
	bool bRunning = UiGameRunning();

	if (ImGui::BeginMenu("Autofire Rate")) {
		for (int i = 0; i < 6; i++) {
			if (ImGui::MenuItem(AutoFireRateLabels[i], NULL, nAutoFireRate == AutoFireRateList[i])) {
				nAutoFireRate = AutoFireRateList[i];
			}
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Map Game Inputs...", NULL, false, bRunning)) {
		UiOpenLegacyMenu();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Rewind")) {
		// the buffer is made when the game starts, StateRewindReInit() makes it again
		if (ImGui::MenuItem("Enable Rewind (hold Page Up)", NULL, bRewindEnabled != 0)) {
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
		static const char* pszKeys[][2] = {
			{ "Tab",			"Game menu" },
			{ "Pause",			"Pause" },
			{ "F1 (hold)",		"Fast forward, frame advance when paused" },
			{ "Shift+F1",		"Fast forward on / off" },
			{ "Page Up (hold)",	"Rewind (Input > Rewind)" },
			{ "F6",				"Save screenshot" },
			{ "F9 / F10",		"Load / save the state of the active slot" },
			{ "Ctrl+F9 / F10",	"Load / save a state file" },
			{ "F8 / F7",		"Previous / next state slot" },
			{ "Shift+F8",		"Undo the last save of the slot" },
			{ "F11",			"Show FPS" },
			{ "F12",			"End the game" },
			{ "Alt+Enter",		"Fullscreen" },
			{ "Keypad - / +",	"Volume" },
		};
		if (ImGui::BeginTable("##keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
			for (unsigned int i = 0; i < sizeof(pszKeys) / sizeof(pszKeys[0]); i++) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pszKeys[i][0]);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pszKeys[i][1]);
			}
			ImGui::EndTable();
		}
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
			VideoMenu();
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		if (ImGui::BeginMenu("Audio")) {
			AudioMenu();
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
