// Main loop of the ImGui interface: one window and one event loop for the
// game selector and the game, the emulation and the interface share each frame
#include "burner.h"
#include "sdl2_gui_common.h"
#include "ui.h"
#include "imgui.h"

extern int vsync;
extern bool bSaveconfig;					// main.cpp
extern bool do_reset_game;					// inp_sdl2.cpp
extern SDL_Renderer* sdlRenderer;			// vid_sdl2.cpp
extern int RunInit();
extern int RunExit();
extern int RunIdle();
extern INT32 display_set_controls();

bool bUiMenuPinned = true;
bool bUiPauseOnMenu = true;
bool bUiPauseOnFocusLoss = true;

static bool bGameRunning = false;
static bool bMenuMode = false;
static bool bOverlay = false;				// quick menu over the game
static bool bUserPause = false;
static bool bFocusLost = false;
static bool bPaused = false;				// pause currently applied
static bool bUiCapture = false;				// the interface has the mouse and the keyboard
static bool bScreenshotHeld = false;
static int nSelectorW = 0, nSelectorH = 0;	// window size of the selector, put back when the game ends

// requests, carried out between two frames
static int nLaunchDrv = -1;
static bool bQuitRequest = false;
static bool bEndRequest = false;
static bool bFullscreenRequest = false;
static bool bFitWindowRequest = false;

void UiLaunchGame(int nDrv)		{ nLaunchDrv = nDrv; }
void UiEndGame()				{ bEndRequest = true; }
void UiQuit()					{ bQuitRequest = true; }
void UiToggleFullscreen()		{ bFullscreenRequest = true; }
bool UiGameRunning()			{ return bGameRunning; }
bool UiGamePaused()				{ return bUserPause; }
bool UiMenuMode()				{ return bMenuMode; }
bool UiOverlayShown()			{ return bOverlay; }
void UiTogglePause()			{ bUserPause = !bUserPause; }
void UiSetPause(bool bPause)	{ bUserPause = bPause; }

void UiShowOverlay(bool bShow)
{
	bOverlay = bShow && bGameRunning;
}

void UiSetWindowScale(int nScale)
{
	nWindowScale = nScale;
	bFitWindowRequest = true;
}

void UiRefitWindow()
{
	bFitWindowRequest = true;
}

static void SetTitle()
{
	char szTitle[512];
	if (bGameRunning) {
		// a RomData dat names its own game
		snprintf(szTitle, sizeof(szTitle), APP_TITLE " - %s", szRomDataTitle[0] ? szRomDataTitle : UiText(BurnDrvGetTextA(DRV_FULLNAME)));
	} else {
		snprintf(szTitle, sizeof(szTitle), APP_TITLE " v%s", szAppBurnVer);
	}
	SDL_SetWindowTitle(sdlWindow, szTitle);
}

// Window size for the game (scaled by nWindowScale, 0 = maximized) plus the menu bar
static void FitWindow()
{
	if (!bGameRunning || bAppFullscreen) {
		return;
	}

	int w, h;
	VidSDL2GetScreenSize(&w, &h);
	if (w <= 0 || h <= 0) {
		return;
	}
	if (nWindowScale <= 0) {
		SDL_MaximizeWindow(sdlWindow);
		return;
	}

	ImGuiStyle& style = ImGui::GetStyle();
	int nMenu = bUiMenuPinned ? (int)(ImGui::GetIO().Fonts->Fonts[0]->FontSize + style.FramePadding.y * 2.0f) : 0;

	// a smaller scale if the window wouldn't fit in the display
	int nDisplay = SDL_GetWindowDisplayIndex(sdlWindow);
	if (nDisplay < 0) {
		nDisplay = 0;
	}
	int nScale = nWindowScale;
	SDL_Rect usable;
	if (SDL_GetDisplayUsableBounds(nDisplay, &usable) == 0) {
		int nTop = 0, nLeft = 0, nBottom = 0, nRight = 0;
		SDL_GetWindowBordersSize(sdlWindow, &nTop, &nLeft, &nBottom, &nRight);
		while (nScale > 1 && (w * nScale + nLeft + nRight > usable.w || h * nScale + nMenu + nTop + nBottom > usable.h)) {
			nScale--;
		}
	}

	SDL_RestoreWindow(sdlWindow);		// a maximized window ignores the new size
	SDL_SetWindowSize(sdlWindow, w * nScale, h * nScale + nMenu);
	SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED_DISPLAY(nDisplay), SDL_WINDOWPOS_CENTERED_DISPLAY(nDisplay));
}

static void SetPaused(bool bPause)
{
	if (bPause == bPaused) {
		return;
	}
	bPaused = bPause;
	bRunPause = bPause;

	// while paused the run loop is timed without sound, see RunIdle()
	if (bPause) {
		AudSoundStop();
	} else {
		AudSoundPlay();
	}
	RunReset();
}

static void StopGame()
{
	if (!bGameRunning) {
		return;
	}

	UiRecordGameStopping();				// the recording files are closed with the game
	RunExit();
	bPaused = false;
	bRunPause = 0;
	DrvExit();
	MediaExit();

	bGameRunning = false;
	bOverlay = false;
	bUserPause = false;
	bAppDoFast = 0;
	bUiFastToggle = 0;
	bAppDoRewind = 0;
	bUiCapture = false;

	UiVideoGameStopped();				// out of the fullscreen of Auto Switch to Fullscreen, or of the game's mode

	if (bSaveconfig) {
		ConfigAppSave();
	}

	SDL_SetRelativeMouseMode(SDL_FALSE);
	SetTitle();

	if (nSelectorW && !bAppFullscreen) {
		int nDisplay = SDL_GetWindowDisplayIndex(sdlWindow);
		if (nDisplay < 0) nDisplay = 0;
		SDL_RestoreWindow(sdlWindow);
		SDL_SetWindowSize(sdlWindow, nSelectorW, nSelectorH);
		SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED_DISPLAY(nDisplay), SDL_WINDOWPOS_CENTERED_DISPLAY(nDisplay));
	}
	nSelectorW = nSelectorH = 0;
}

static void StartGame(int nDrv)
{
	UiGameSelStopScan();				// the rom scan thread uses nBurnDrvActive
	StopGame();
	UiGameSelClose();
	UiGamePrepareLaunch();

	if (nDrv < 0 || (UINT32)nDrv >= nBurnDrvCount) {
		return;
	}

	UiRomDataPrepareLaunch();			// szRomdataName, for DrvInit()
	nBurnDrvActive = nDrv;
	UiIpsBeforeLoad();					// Apply IPS patches of the selector, -ips
	char szTitle[300];
	snprintf(szTitle, sizeof(szTitle), "Loading %s...", UiText(BurnDrvGetTextA(DRV_FULLNAME)));
	UiProgressStart(szTitle);

	int nRet = DrvInit(nDrv, 0);
	if (nRet == 0) {
		MediaInit();
	}

	UiProgressEnd();

	if (nRet) {
		IpsPatchExit();					// DrvInit() can fail before the roms are loaded
		DrvExit();
		MediaExit();
		bMenuMode = true;				// stay on the selector to show the error
		return;
	}

	AudSoundSetVolume();
	display_set_controls();
	RunInit();

	bGameRunning = true;
	bUserPause = false;
	bPaused = false;
	bOverlay = false;
	bUiCapture = false;
	nGameSelect = nDrv;

	if (!bAppFullscreen && !(SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_MAXIMIZED)) {
		SDL_GetWindowSize(sdlWindow, &nSelectorW, &nSelectorH);
	}
	SetTitle();
	UiVideoGameStarted();				// the fullscreen of the game (display, resolution)
	FitWindow();
	GameInpCheckMouse();
	UiGameStarted();
	UiRecordGameStarted();				// Game > Replay from power-on
}

static void DoRequests()
{
	if (bFullscreenRequest) {
		bFullscreenRequest = false;
		UiVideoSetFullscreen(!bAppFullscreen);
		if (!bAppFullscreen) {
			bFitWindowRequest = true;
		}
	}
	if (bFitWindowRequest) {
		bFitWindowRequest = false;
		FitWindow();
	}
	if (bEndRequest) {
		bEndRequest = false;
		StopGame();
		bMenuMode = true;				// back to the selector
	}
	if (nLaunchDrv >= 0) {
		int nDrv = nLaunchDrv;
		nLaunchDrv = -1;
		StartGame(nDrv);
	}
}

static bool bEndHeld = false;				// the game ends when the key is released
static int nMuteVolume = -1;

bool UiMuted()
{
	return nMuteVolume >= 0;
}

void UiToggleMute()
{
	if (nMuteVolume >= 0) {
		nAudVolume = nMuteVolume;
		nMuteVolume = -1;
		UpdateMessage((char*)"Sound on");
	} else {
		nMuteVolume = nAudVolume;
		nAudVolume = 0;
		UpdateMessage((char*)"Sound muted");
	}
	AudSoundSetVolume();
}

static void ChangeVolume(int nDelta)
{
	nMuteVolume = -1;
	nAudVolume += nDelta;
	if (nAudVolume < 0) nAudVolume = 0;
	if (nAudVolume > 20000) nAudVolume = 20000;		// 200%, it amplifies above 100%
	AudSoundSetVolume();

	char szText[32];
	snprintf(szText, sizeof(szText), "Volume %d%%", nAudVolume / 100);
	UpdateMessage(szText);
}

static void GameHotkey(int nAction, const SDL_KeyboardEvent& key)
{
	char szText[32];

	// only the hold actions repeat
	if (key.repeat && nAction != UI_HK_FFWD && nAction != UI_HK_REWIND && nAction != UI_HK_VOLUME_UP && nAction != UI_HK_VOLUME_DOWN) {
		return;
	}

	switch (nAction) {
		case UI_HK_PAUSE:			UiTogglePause(); break;
		case UI_HK_FFWD:
			if (bUserPause) {
				if (!key.repeat) bAppDoStep = 1;		// frame advance
			} else {
				bAppDoFast = 1;
			}
			break;
		case UI_HK_FFWD_TOGGLE:
			bUiFastToggle = !bUiFastToggle;
			bAppDoFast = bUiFastToggle;
			break;
		case UI_HK_REWIND:			bAppDoRewind = 1; break;
		case UI_HK_LOAD_GAME:		UiGameSelOpen(); break;
		case UI_HK_END_GAME:		bEndHeld = true; break;
		case UI_HK_STATE_LOAD:		StatedLoad(nSavestateSlot); break;
		case UI_HK_STATE_SAVE:		StatedSave(nSavestateSlot); break;
		case UI_HK_STATE_UNDO:		StatedUNDO(nSavestateSlot); break;
		case UI_HK_STATE_LOAD_DIALOG:	UiGameOpenStateDialog(false); break;
		case UI_HK_STATE_SAVE_DIALOG:	UiGameOpenStateDialog(true); break;
		case UI_HK_STATE_PREV:
		case UI_HK_STATE_NEXT:
			nSavestateSlot = (nAction == UI_HK_STATE_NEXT) ? (nSavestateSlot % 8) + 1 : ((nSavestateSlot + 6) % 8) + 1;
			snprintf(szText, sizeof(szText), "State slot %d", nSavestateSlot);
			UpdateMessage(szText);
			break;
		case UI_HK_MEMCARD_TOGGLE:
			if (MemCardSupported() && (nMemoryCardStatus & 1)) {
				MemCardToggle();
				UpdateMessage((nMemoryCardStatus & 2) ? (char*)"Memory card inserted" : (char*)"Memory card ejected");
			}
			break;
		case UI_HK_MAP_INPUTS:		UiInputMappingOpen(); break;
		case UI_HK_DIPS:			UiDipsOpen(); break;
		case UI_HK_CHEATS:			UiCheatsOpen(); break;
		case UI_HK_TOGGLE_MENU:
			bUiMenuPinned = !bUiMenuPinned;
			bFitWindowRequest = true;
			break;
		case UI_HK_FPS:				bAppShowFPS = !bAppShowFPS; break;
		case UI_HK_SCREENSHOT:
			if (!bScreenshotHeld) {
				UiSnapshot(0);
				bScreenshotHeld = true;
			}
			break;
		case UI_HK_SNAP_TITLE:		UiSnapshot(1); break;
		case UI_HK_SNAP_PREVIEW:	UiSnapshot(2); break;
		case UI_HK_SHOT_FACTORY:	UiShotFactoryOpen(); break;
		case UI_HK_PALETTE:			UiPaletteViewerToggle(); break;
		case UI_HK_MUTE:			UiToggleMute(); break;
		case UI_HK_VOLUME_UP:		ChangeVolume(500); break;
		case UI_HK_VOLUME_DOWN:		ChangeVolume(-500); break;
	}
}

// the releases, whatever the modifiers are then
static void GameKeyUp(const SDL_KeyboardEvent& key)
{
	SDL_Keycode nKey = key.keysym.sym;

	if (UiHotkeyUsesKey(UI_HK_FFWD, nKey)) {
		bAppDoFast = bUiFastToggle;
	}
	if (UiHotkeyUsesKey(UI_HK_REWIND, nKey)) {
		bAppDoRewind = 0;
	}
	if (UiHotkeyUsesKey(UI_HK_SCREENSHOT, nKey)) {
		bScreenshotHeld = false;
	}
	if (bEndHeld && UiHotkeyUsesKey(UI_HK_END_GAME, nKey)) {
		bEndHeld = false;
		// started without the menu: the game is over, like the old interface
		if (bMenuMode) {
			UiEndGame();
		} else {
			UiQuit();
		}
	}
}

static void HandleEvent(const SDL_Event& e)
{
	int nAction;

	switch (e.type) {
		case SDL_QUIT:
			UiQuit();
			break;

		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST || e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
				bFocusLost = true;
			}
			if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
				bFocusLost = false;
			}
			break;

		case SDL_KEYDOWN:
			if (UiHotkeysCapturing()) {				// the hotkey editor waits for a key
				UiHotkeysCaptureKey(e.key);
				break;
			}
			if (UiInputCapturing()) {				// a game input is being mapped
				break;
			}
			nAction = UiHotkeyFind(e.key);

			// fullscreen works whatever has the focus
			if (nAction == UI_HK_FULLSCREEN) {
				if (!e.key.repeat) UiToggleFullscreen();
				break;
			}
			if (!bGameRunning) {
				break;
			}
			if (nAction == UI_HK_GAME_MENU && !e.key.repeat && (bOverlay || !bUiCapture)) {
				UiShowOverlay(!bOverlay);
				break;
			}
			if (e.key.keysym.sym == SDLK_ESCAPE && bOverlay && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
				UiShowOverlay(false);
				break;
			}
			if (!bUiCapture && nAction >= 0) {
				GameHotkey(nAction, e.key);
			}
			break;

		case SDL_KEYUP:
			if (bGameRunning) {
				GameKeyUp(e.key);
			}
			break;
	}
}

// Pause and input ownership follow what the interface shows
static void UpdateState()
{
	if (!bGameRunning) {
		UiSetNavigation(true);
		return;
	}

	bool bDialogs = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) || UiPopupActive() || UiGameSelIsOpen() || UiMiscCapture();
	bool bCapture = bOverlay || bDialogs;

	SetPaused(bUserPause || (bUiPauseOnMenu && bCapture) || (bUiPauseOnFocusLoss && bFocusLost));

	if (bCapture != bUiCapture) {
		bUiCapture = bCapture;
		if (bCapture) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			bAppDoFast = 0;					// the fast forward toggle comes back with the game
			bAppDoRewind = 0;
			bEndHeld = false;
		} else {
			bAppDoFast = bUiFastToggle;
			GameInpCheckMouse();
		}
	}
	UiSetNavigation(bCapture && !UiInputCapturing());	// the buttons of a gamepad being mapped don't click
}

static void DrawOverlay()
{
	static bool bWasShown = false;

	if (!bOverlay) {
		bWasShown = false;
		return;
	}

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (!bWasShown) {
		ImGui::SetNextWindowFocus();	// gives the keyboard/gamepad navigation to the menu
		bWasShown = true;
	}

	bool bOpen = true;
	if (ImGui::Begin(UiText(BurnDrvGetTextA(DRV_FULLNAME)), &bOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {
		ImVec2 button(300.0f * fUiScale, 0.0f);

		if (ImGui::Button("Resume", button)) bOpen = false;
		ImGui::SetItemDefaultFocus();
		ImGui::Separator();
		char szLabel[64];
		snprintf(szLabel, sizeof(szLabel), "Save State to Slot %d (F10)", nSavestateSlot);
		if (ImGui::Button(szLabel, button)) { StatedSave(nSavestateSlot); bOpen = false; }
		snprintf(szLabel, sizeof(szLabel), "Load State from Slot %d (F9)", nSavestateSlot);
		if (ImGui::Button(szLabel, button)) { StatedLoad(nSavestateSlot); bOpen = false; }
		if (ImGui::Button("Save Screenshot (F6)", button)) { UiSnapshot(0); bOpen = false; }
		if (ImGui::Button("Reset", button)) { do_reset_game = true; bOpen = false; }
		ImGui::Separator();
		if (ImGui::Button("Map Game Inputs...", button)) { UiInputMappingOpen(); bOpen = false; }
		if (ImGui::Button("DIP Switches...", button)) { UiDipsOpen(); bOpen = false; }
		if (ImGui::Button("Cheats...", button)) { UiCheatsOpen(); bOpen = false; }
		if (ImGui::Button("Load Game...", button)) { UiGameSelOpen(); bOpen = false; }
		ImGui::Separator();
		if (ImGui::Button("End Game", button)) { UiEndGame(); bOpen = false; }
		if (ImGui::Button("Quit FinalBurn Neo", button)) { UiQuit(); bOpen = false; }
	}
	ImGui::End();

	if (!bOpen) {
		bOverlay = false;
	}
}

// FFWD / PAUSE / FPS / messages over the game
static void DrawOsd()
{
	if (!bGameRunning) {
		return;
	}

	ImDrawList* dl = ImGui::GetBackgroundDrawList();		// over the game, under the windows
	ImGuiViewport* vp = ImGui::GetMainViewport();
	float fPad = 10.0f * fUiScale;
	float fLine = ImGui::GetTextLineHeightWithSpacing();
	ImVec2 pos(vp->Pos.x + fPad, vp->Pos.y + UiMenuBarHeight() + fPad);

	const char* pszLines[4];
	int nLines = 0;
	const char* pszRecord = UiRecordStatus();
	if (pszRecord) pszLines[nLines++] = pszRecord;
	if (bUiShowModeIndicator) {
		if (bAppDoFast) pszLines[nLines++] = "FFWD";
		if (bPaused && !bOverlay) pszLines[nLines++] = "PAUSE";
	}
	if (messageFrames > 1) {
		pszLines[nLines++] = lastMessage;
		messageFrames--;
	}

	for (int i = 0; i < nLines; i++) {
		dl->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 255), pszLines[i]);
		dl->AddText(pos, IM_COL32(255, 240, 0, 255), pszLines[i]);
		pos.y += fLine;
	}

	UiRecordDrawMovement(dl, ImVec2(vp->Pos.x + fPad, vp->Pos.y + vp->Size.y - fPad));

	if (bAppShowFPS) {
		char szFps[32];
		snprintf(szFps, sizeof(szFps), "%s fps", fpsstring);
		ImVec2 size = ImGui::CalcTextSize(szFps);
		ImVec2 p(vp->Pos.x + vp->Size.x - size.x - fPad, vp->Pos.y + UiMenuBarHeight() + fPad);
		dl->AddText(ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, 255), szFps);
		dl->AddText(p, IM_COL32(255, 240, 0, 255), szFps);
	}
}

int UiMain(int nDrvSelect, bool bMenu)
{
	if (UiInit()) {
		UiExit();
		return 1;
	}

	bMenuMode = bMenu || nDrvSelect < 0;
	bQuitRequest = false;
	SetTitle();
	if (bAppFullscreen) {
		UiVideoApplyFullscreen();		// on the display picked for the fullscreen
	}
	UiGameSelInit();
	UiFirstRunCheck();

	SDL_GameControllerEventState(SDL_ENABLE);
	SDL_JoystickEventState(SDL_ENABLE);

	if (nDrvSelect >= 0) {
		nLaunchDrv = nDrvSelect;
	}

	while (true) {
		DoRequests();
		if (!bGameRunning && !bMenuMode) {
			break;						// the game given on the command line is over (or failed)
		}
		if (bQuitRequest) {
			break;
		}

		bool bEvents = false;
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			bEvents = true;
			UiProcessEvent(&e);
			HandleEvent(e);
		}

		UpdateState();

		UINT32 nFrames = nFramesRendered;
		if (bGameRunning) {
			RunIdle();					// emulates as many frames as the sound needs
		}

		if (!vsync) {
			// nothing paces the loop then
			if (bGameRunning && !bPaused && !bEvents && nFrames == nFramesRendered) {
				SDL_Delay(1);
				continue;
			}
			if (!bGameRunning || bPaused) {
				SDL_Delay(15);
			}
		}

		UiPopupFlush();
		UiNewFrame();

		UiMenuBarDraw();
		if (!bGameRunning) {
			UiGameSelDraw(false);
		} else {
			UiGameSelDraw(true);
			DrawOverlay();
			DrawOsd();
		}
		UiGameDialogsDraw();
		UiInputDialogsDraw();
		UiHotkeysDraw();
		UiPathsDraw();
		UiGameInfoDraw();
		UiIpsDraw();
		UiRecordDraw();
		UiCdLibraryDraw();
		UiRomDataDraw();
		UiMiscDialogsDraw();
		UiPopupsDraw();

		UiPresent(bGameRunning);
	}

	StopGame();
	UiCdLibraryExit();
	UiGameSelExit();
	UiExit();

	return 0;
}
