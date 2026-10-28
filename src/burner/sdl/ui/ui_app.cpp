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
extern UINT16 maxLinesMenu;					// sdl2_gui_ingame.cpp
extern int RunInit();
extern int RunExit();
extern int RunIdle();
extern INT32 display_set_controls();
extern void ingame_gui_start(SDL_Renderer* renderer);
void VidSDL2GetScreenSize(int* pnWidth, int* pnHeight);
void VidSDL2DrawGame(const SDL_Rect* pArea);

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
static bool bLegacyMenuRequest = false;
static bool bFullscreenRequest = false;
static bool bFitWindowRequest = false;

void UiLaunchGame(int nDrv)		{ nLaunchDrv = nDrv; }
void UiEndGame()				{ bEndRequest = true; }
void UiQuit()					{ bQuitRequest = true; }
void UiToggleFullscreen()		{ bFullscreenRequest = true; }
void UiOpenLegacyMenu()			{ bLegacyMenuRequest = true; }
bool UiGameRunning()			{ return bGameRunning; }
bool UiGamePaused()				{ return bUserPause; }
bool UiMenuMode()				{ return bMenuMode; }
bool UiOverlayShown()			{ return bOverlay; }
void UiTogglePause()			{ bUserPause = !bUserPause; }

void UiShowOverlay(bool bShow)
{
	bOverlay = bShow && bGameRunning;
}

void UiSetWindowScale(int nScale)
{
	nWindowScale = nScale;
	bFitWindowRequest = true;
}

static void SetTitle()
{
	char szTitle[512];
	if (bGameRunning) {
		snprintf(szTitle, sizeof(szTitle), APP_TITLE " - %s", UiText(BurnDrvGetTextA(DRV_FULLNAME)));
	} else {
		snprintf(szTitle, sizeof(szTitle), APP_TITLE " v%s", szAppBurnVer);
	}
	SDL_SetWindowTitle(sdlWindow, szTitle);
}

// Window size for the game (scaled by nWindowScale) plus the menu bar
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

	ImGuiStyle& style = ImGui::GetStyle();
	int nMenu = bUiMenuPinned ? (int)(ImGui::GetIO().Fonts->Fonts[0]->FontSize + style.FramePadding.y * 2.0f) : 0;

	SDL_RestoreWindow(sdlWindow);		// a maximized window ignores the new size
	SDL_SetWindowSize(sdlWindow, w * nWindowScale, h * nWindowScale + nMenu);
	SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
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

	RunExit();
	bPaused = false;
	bRunPause = 0;
	DrvExit();
	MediaExit();

	bGameRunning = false;
	bOverlay = false;
	bUserPause = false;
	bAppDoFast = 0;
	bUiCapture = false;

	if (bSaveconfig) {
		ConfigAppSave();
	}

	SDL_SetRelativeMouseMode(SDL_FALSE);
	SetTitle();

	if (nSelectorW && !bAppFullscreen) {
		SDL_SetWindowSize(sdlWindow, nSelectorW, nSelectorH);
		SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	}
	nSelectorW = nSelectorH = 0;
}

static void StartGame(int nDrv)
{
	StopGame();
	UiGameSelClose();

	if (nDrv < 0 || (UINT32)nDrv >= nBurnDrvCount) {
		return;
	}

	nBurnDrvActive = nDrv;
	char szTitle[300];
	snprintf(szTitle, sizeof(szTitle), "Loading %s...", UiText(BurnDrvGetTextA(DRV_FULLNAME)));
	UiProgressStart(szTitle);

	int nRet = DrvInit(nDrv, 0);
	if (nRet == 0) {
		MediaInit();
	}

	UiProgressEnd();

	if (nRet) {
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
	FitWindow();
	GameInpCheckMouse();
}

// The classic menus (dips, cheats, input mapping) until the ImGui versions
// exist: they run their own loop and draw at the game resolution
static void RunLegacyMenu()
{
	int w, h;
	VidSDL2GetScreenSize(&w, &h);
	maxLinesMenu = h / 10 - 6;

	inrenderer(sdlRenderer);
	prepare_inline_font();
	SDL_RenderSetScale(sdlRenderer, 1.0f, 1.0f);
	SDL_RenderSetLogicalSize(sdlRenderer, w, h);

	// the menu takes the current picture as its background
	SDL_Rect area = { 0, 0, w, h };
	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(sdlRenderer);
	VidSDL2DrawGame(&area);

	ingame_gui_start(sdlRenderer);

	SDL_RenderSetLogicalSize(sdlRenderer, 0, 0);
	SDL_RenderSetViewport(sdlRenderer, NULL);
	SDL_RenderSetScale(sdlRenderer, 1.0f, 1.0f);
	kill_inline_font();

	// the menu turns the controller events off and the sound back on when it closes
	SDL_GameControllerEventState(SDL_ENABLE);
	SDL_JoystickEventState(SDL_ENABLE);
	if (bPaused) {
		AudSoundStop();
	}
	RunReset();
	ImGui::GetIO().ClearInputKeys();
}

static void DoRequests()
{
	if (bFullscreenRequest) {
		bFullscreenRequest = false;
		SetFullscreen(!GetFullscreen());
		if (!bAppFullscreen) {
			bFitWindowRequest = true;
		}
	}
	if (bFitWindowRequest) {
		bFitWindowRequest = false;
		FitWindow();
	}
	if (bLegacyMenuRequest) {
		bLegacyMenuRequest = false;
		if (bGameRunning) {
			RunLegacyMenu();
		}
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

static void GameKeyDown(const SDL_KeyboardEvent& key)
{
	switch (key.keysym.sym) {
		case SDLK_F1:
			bAppDoFast = 1;
			break;
		case SDLK_F9:
			QuickState(0);
			break;
		case SDLK_F10:
			QuickState(1);
			break;
		case SDLK_F11:
			if (!key.repeat) bAppShowFPS = !bAppShowFPS;
			break;
		case SDLK_F6:
			if (!bScreenshotHeld) {
				MakeScreenShot(0);
				bScreenshotHeld = true;
			}
			break;
		case SDLK_PAUSE:
			if (!key.repeat) UiTogglePause();
			break;
		case SDLK_KP_MINUS:
			nAudVolume -= 500;
			if (nAudVolume < 0) nAudVolume = 0;
			AudSoundSetVolume();
			break;
		case SDLK_KP_PLUS:
			nAudVolume += 500;
			if (nAudVolume > 10000) nAudVolume = 10000;
			AudSoundSetVolume();
			break;
	}
}

static void GameKeyUp(const SDL_KeyboardEvent& key)
{
	switch (key.keysym.sym) {
		case SDLK_F1:
			bAppDoFast = 0;
			break;
		case SDLK_F6:
			bScreenshotHeld = false;
			break;
		case SDLK_F12:
			// started without the menu: F12 quits, like the old interface
			if (bMenuMode) {
				UiEndGame();
			} else {
				UiQuit();
			}
			break;
	}
}

static void HandleEvent(const SDL_Event& e)
{
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
			// these work whatever has the focus
			if (e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
				if (!e.key.repeat) UiToggleFullscreen();
				break;
			}
			if (!bGameRunning) {
				break;
			}
			if (e.key.keysym.sym == SDLK_TAB && !e.key.repeat && (bOverlay || !bUiCapture)) {
				UiShowOverlay(!bOverlay);
				break;
			}
			if (e.key.keysym.sym == SDLK_ESCAPE && bOverlay && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
				UiShowOverlay(false);
				break;
			}
			if (!bUiCapture) {
				GameKeyDown(e.key);
			}
			break;

		case SDL_KEYUP:
			if (bGameRunning && (!bUiCapture || e.key.keysym.sym == SDLK_F1)) {
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

	bool bDialogs = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) || UiPopupActive() || UiGameSelIsOpen();
	bool bCapture = bOverlay || bDialogs;

	SetPaused(bUserPause || (bUiPauseOnMenu && bCapture) || (bUiPauseOnFocusLoss && bFocusLost));

	if (bCapture != bUiCapture) {
		bUiCapture = bCapture;
		if (bCapture) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			bAppDoFast = 0;
		} else {
			GameInpCheckMouse();
		}
	}
	UiSetNavigation(bCapture);
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
		if (ImGui::Button("Save State (F10)", button)) { QuickState(1); bOpen = false; }
		if (ImGui::Button("Load State (F9)", button)) { QuickState(0); bOpen = false; }
		if (ImGui::Button("Save Screenshot (F6)", button)) { MakeScreenShot(0); bOpen = false; }
		if (ImGui::Button("Reset", button)) { do_reset_game = true; bOpen = false; }
		ImGui::Separator();
		if (ImGui::Button("DIPs, Cheats, Input Mapping...", button)) { UiOpenLegacyMenu(); bOpen = false; }
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

	const char* pszLines[3];
	int nLines = 0;
	if (bAppDoFast) pszLines[nLines++] = "FFWD";
	if (bPaused && !bOverlay) pszLines[nLines++] = "PAUSE";
	if (messageFrames > 1) {
		pszLines[nLines++] = lastMessage;
		messageFrames--;
	}

	for (int i = 0; i < nLines; i++) {
		dl->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 255), pszLines[i]);
		dl->AddText(pos, IM_COL32(255, 240, 0, 255), pszLines[i]);
		pos.y += fLine;
	}

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
	UiGameSelInit();

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
		UiPopupsDraw();

		UiPresent(bGameRunning);
	}

	StopGame();
	UiGameSelExit();
	UiExit();

	return 0;
}
