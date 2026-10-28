// Main menu bar
#include "burner.h"
#include "ui.h"
#include "imgui.h"

extern bool do_reset_game;					// inp_sdl2.cpp
extern INT32 AutoFireRateList[6];			// sdl2_gui_ingame.cpp
extern char AutoFireRateLabels[6][10];
extern INT32 nSocd[6];						// burn/devices/joyprocess.cpp

static float fReserved = 0.0f;				// taken from the game area
static bool bMenusOpen = false;

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
			UiMiscMenu();				// ui_misc.cpp
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		if (ImGui::BeginMenu("Help")) {
			UiHelpMenu();
			ImGui::EndMenu();
			bMenusOpen = true;
		}
		ImGui::EndMainMenuBar();
	}
}
