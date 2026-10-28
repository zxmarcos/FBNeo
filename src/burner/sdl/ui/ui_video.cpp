// Video and Audio menus, and the fullscreen of the window
#include "burner.h"
#include "ui.h"
#include "imgui.h"
#include "vid_softfx.h"

extern int vsync;
extern char videofiltering[3];
extern SDL_Renderer* sdlRenderer;			// vid_sdl2.cpp
extern int RunAudioReInit();				// run.cpp
extern void AudSDLPickDriver();				// aud_sdl.cpp

static bool bAutoFullscreen = false;		// Auto Switch to Fullscreen put the window in fullscreen

// The software effects that have a C version, the others need BUILD_X86_ASM
static const int SoftFXList[] = {
	FILTER_ADVMAME_SCALE_2X, FILTER_ADVMAME_SCALE_3X,
	FILTER_HQ2XS_VBA, FILTER_HQ2XS_SNES9X, FILTER_HQ3XS_SNES9X, FILTER_HQ2XBOLD, FILTER_HQ3XBOLD,
	FILTER_EPXB, FILTER_EPXC,
	FILTER_2XBR_A, FILTER_2XBR_B, FILTER_2XBR_C, FILTER_3XBR_A, FILTER_3XBR_B, FILTER_3XBR_C, FILTER_4XBR_A, FILTER_4XBR_B, FILTER_4XBR_C,
	FILTER_DDT3X, FILTER_CRTx22, FILTER_CRTx33, FILTER_CRTx44,
#if defined BUILD_X86_ASM
	FILTER_2XPM_LQ, FILTER_2XPM_HQ, FILTER_EAGLE, FILTER_SUPEREAGLE, FILTER_2XSAI, FILTER_SUPER_2XSAI,
	FILTER_SUPEREAGLE_VBA, FILTER_2XSAI_VBA, FILTER_SUPER_2XSAI_VBA, FILTER_SUPERSCALE, FILTER_SUPERSCALE_75SCAN,
	FILTER_HQ2X, FILTER_HQ3X, FILTER_HQ4X, FILTER_HQ3XS_VBA,
#endif
};

// Makes the image and the texture again, after a change of their depth or size
void UiVideoReInit()
{
	if (!UiGameRunning()) {
		return;
	}
	VidInit();
	if (bRunPause) {
		VidRedraw();						// a paused game shows its picture again
		VidPaint(0);
	}
}

// The colours of the game go through the gamma table
static void GammaChanged()
{
	ComputeGammaLUT();
	if (UiGameRunning()) {
		SetBurnHighCol(nVidImageDepth);
		if (bRunPause) {
			VidRedraw();
			VidPaint(0);
		}
	}
}

// The display and the mode of the fullscreen: the ones of the vertical games while one runs
void UiVideoApplyFullscreen()
{
	if (!bAppFullscreen) {
		SDL_SetWindowFullscreen(sdlWindow, 0);
		return;
	}

	int v = (UiGameRunning() && VidSDL2GameVertical()) ? 1 : 0;
	int nDisplay = nVidFullDisplay[v];
	if (nDisplay >= SDL_GetNumVideoDisplays()) {
		nDisplay = -1;						// unplugged
	}
	if (nDisplay >= 0 && SDL_GetWindowDisplayIndex(sdlWindow) != nDisplay) {
		// a window goes to another display out of fullscreen
		SDL_SetWindowFullscreen(sdlWindow, 0);
		SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED_DISPLAY(nDisplay), SDL_WINDOWPOS_CENTERED_DISPLAY(nDisplay));
	}
	if (nDisplay < 0) {
		nDisplay = SDL_GetWindowDisplayIndex(sdlWindow);
	}

	if (bVidExclusiveFullscreen && UiGameRunning() && nVidFullWidth[v] > 0 && nVidFullHeight[v] > 0) {
		// the refresh rate nearest to the game's
		SDL_DisplayMode want = { 0, nVidFullWidth[v], nVidFullHeight[v], (nBurnFPS + 50) / 100, NULL };
		SDL_DisplayMode mode;
		if (SDL_GetClosestDisplayMode(nDisplay, &want, &mode) && SDL_SetWindowDisplayMode(sdlWindow, &mode) == 0) {
			if (SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN) == 0) {
				return;
			}
		}
		fprintf(stderr, "Couldn't change the resolution to %dx%d: %s\n", nVidFullWidth[v], nVidFullHeight[v], SDL_GetError());
	}
	SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void UiVideoSetFullscreen(bool bFullscreen)
{
	bAppFullscreen = bFullscreen;
	bAutoFullscreen = false;
	UiVideoApplyFullscreen();
}

// StartGame(): Auto Switch to Fullscreen, and the fullscreen of the game
void UiVideoGameStarted()
{
	if (bVidAutoSwitchFull && !bAppFullscreen) {
		bAppFullscreen = true;
		bAutoFullscreen = true;
	}
	if (bAppFullscreen) {
		UiVideoApplyFullscreen();
	}
}

// StopGame(): back to the window if the game put the window in fullscreen
void UiVideoGameStopped()
{
	if (bAutoFullscreen) {
		bAutoFullscreen = false;
		bAppFullscreen = false;
	}
	UiVideoApplyFullscreen();
}

static void DisplayMenu(int v)
{
	if (ImGui::MenuItem("The Window's Display", NULL, nVidFullDisplay[v] < 0)) {
		nVidFullDisplay[v] = -1;
		UiVideoApplyFullscreen();
	}
	ImGui::Separator();
	for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
		SDL_Rect r;
		SDL_GetDisplayBounds(i, &r);
		const char* pszName = SDL_GetDisplayName(i);
		char szLabel[128];
		snprintf(szLabel, sizeof(szLabel), "%d: %s (%d x %d)", i + 1, pszName ? pszName : "Display", r.w, r.h);
		if (ImGui::MenuItem(szLabel, NULL, nVidFullDisplay[v] == i)) {
			nVidFullDisplay[v] = i;
			UiVideoApplyFullscreen();
		}
	}
}

static void ResolutionMenu(int v)
{
	int nDisplay = nVidFullDisplay[v];
	if (nDisplay < 0 || nDisplay >= SDL_GetNumVideoDisplays()) {
		nDisplay = SDL_GetWindowDisplayIndex(sdlWindow);
	}

	if (ImGui::MenuItem("Desktop Resolution", NULL, nVidFullWidth[v] <= 0)) {
		nVidFullWidth[v] = nVidFullHeight[v] = 0;
		UiVideoApplyFullscreen();
	}
	ImGui::Separator();

	// the modes come from the largest, one item per size (the refresh rate follows the game)
	int nLastW = 0, nLastH = 0;
	for (int i = 0; i < SDL_GetNumDisplayModes(nDisplay); i++) {
		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(nDisplay, i, &mode) || (mode.w == nLastW && mode.h == nLastH)) {
			continue;
		}
		nLastW = mode.w;
		nLastH = mode.h;

		char szLabel[32];
		snprintf(szLabel, sizeof(szLabel), "%d x %d", mode.w, mode.h);
		if (ImGui::MenuItem(szLabel, NULL, nVidFullWidth[v] == mode.w && nVidFullHeight[v] == mode.h)) {
			nVidFullWidth[v] = mode.w;
			nVidFullHeight[v] = mode.h;
			UiVideoApplyFullscreen();
		}
	}
}

static void FullscreenMenu()
{
	if (ImGui::MenuItem("Fullscreen", UiHotkeyName(UI_HK_FULLSCREEN), bAppFullscreen)) {
		UiToggleFullscreen();
	}
	if (ImGui::MenuItem("Auto Switch to Fullscreen after Loading", NULL, bVidAutoSwitchFull != 0)) {
		bVidAutoSwitchFull = !bVidAutoSwitchFull;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Change the Resolution (Exclusive Mode)", NULL, bVidExclusiveFullscreen != 0)) {
		bVidExclusiveFullscreen = !bVidExclusiveFullscreen;
		UiVideoApplyFullscreen();
	}
	if (ImGui::BeginMenu("Resolution (Horizontal Games)", bVidExclusiveFullscreen != 0)) {
		ResolutionMenu(0);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Resolution (Vertical Games)", bVidExclusiveFullscreen != 0)) {
		ResolutionMenu(1);
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Display (Horizontal Games)")) {
		DisplayMenu(0);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Display (Vertical Games)")) {
		DisplayMenu(1);
		ImGui::EndMenu();
	}
}

static void StretchMenu()
{
	if (ImGui::MenuItem("Normal (Whole Multiples of the Pixels)", NULL, !bVidFullStretch && !bVidCorrectAspect)) {
		bVidFullStretch = 0; bVidCorrectAspect = 0; bIntegerScale = false;
		UiRefitWindow();
	}
	if (ImGui::MenuItem("Full Stretch", NULL, bVidFullStretch != 0)) {
		bVidFullStretch = 1; bVidCorrectAspect = 0; bIntegerScale = false;
	}
	if (ImGui::MenuItem("Correct Aspect Ratio", NULL, !bVidFullStretch && bVidCorrectAspect && !bIntegerScale)) {
		bVidFullStretch = 0; bVidCorrectAspect = 1; bIntegerScale = false;
		UiRefitWindow();
	}
	if (ImGui::MenuItem("Integer Scaling (Correct Aspect Ratio)", NULL, !bVidFullStretch && bVidCorrectAspect && bIntegerScale)) {
		bVidFullStretch = 0; bVidCorrectAspect = 1; bIntegerScale = true;
		UiRefitWindow();
	}
}

static void AspectItem(const char* pszLabel, int x, int y, INT32* pnX, INT32* pnY)
{
	if (ImGui::MenuItem(pszLabel, NULL, !bMonitorAutoCheck && *pnX == x && *pnY == y)) {
		bMonitorAutoCheck = false;
		*pnX = x;
		*pnY = y;
		UiRefitWindow();
	}
}

static void MonitorMenu()
{
	if (ImGui::MenuItem("Square Pixels (Detect the Aspect Ratio)", NULL, bMonitorAutoCheck)) {
		bMonitorAutoCheck = true;
		UiRefitWindow();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Aspect Ratio (Horizontal)")) {
		AspectItem("Normal CRT (4:3)", 4, 3, &nVidScrnAspectX, &nVidScrnAspectY);
		AspectItem("Normal LCD (5:4)", 5, 4, &nVidScrnAspectX, &nVidScrnAspectY);
		AspectItem("Widescreen (16:9)", 16, 9, &nVidScrnAspectX, &nVidScrnAspectY);
		AspectItem("Widescreen LCD (16:10)", 16, 10, &nVidScrnAspectX, &nVidScrnAspectY);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Aspect Ratio (Vertical, in Fullscreen)")) {
		AspectItem("Normal CRT, Landscape (4:3)", 4, 3, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
		AspectItem("Normal LCD, Landscape (5:4)", 5, 4, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
		AspectItem("Widescreen, Landscape (16:9)", 16, 9, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
		AspectItem("Widescreen LCD, Landscape (16:10)", 16, 10, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
		ImGui::Separator();
		AspectItem("Normal CRT, Portrait (3:4)", 3, 4, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
		AspectItem("Normal LCD, Portrait (4:5)", 4, 5, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
		AspectItem("Widescreen, Portrait (9:16)", 9, 16, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
		AspectItem("Widescreen LCD, Portrait (10:16)", 10, 16, &nVidVerScrnAspectX, &nVidVerScrnAspectY);
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Mirrored Vertical Alignment", NULL, (nVidRotationAdjust & 2) != 0)) {
		nVidRotationAdjust ^= 2;		// the vertical games are turned the other way
	}
}

static void GammaMenu()
{
	if (ImGui::MenuItem("Enable Gamma Correction", NULL, bDoGamma != 0)) {
		bDoGamma = !bDoGamma;
		GammaChanged();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Darken Midtones (1.25)", NULL, bDoGamma && nGamma == 1.25)) {
		nGamma = 1.25;
		bDoGamma = 1;
		GammaChanged();
	}
	if (ImGui::MenuItem("Lighten Midtones (0.80)", NULL, bDoGamma && nGamma == 0.80)) {
		nGamma = 0.80;
		bDoGamma = 1;
		GammaChanged();
	}
	float fGamma = (float)nGamma;
	ImGui::SetNextItemWidth(160.0f * fUiScale);
	if (ImGui::SliderFloat("Gamma", &fGamma, 0.25f, 3.0f, "%.2f")) {
		nGamma = (int)(fGamma * 100.0f + 0.5f) / 100.0;
		bDoGamma = 1;
		GammaChanged();
	}
}

static void ScanlinesMenu()
{
	if (ImGui::MenuItem("Enable Scanlines", NULL, bVidScanlines != 0)) {
		bVidScanlines = !bVidScanlines;
	}
	if (ImGui::MenuItem("Turn Them with the Vertical Games", NULL, bVidScanRotate != 0)) {
		bVidScanRotate = !bVidScanRotate;
	}
	// nVidScanIntensity is the brightness of the dark lines, as in the Win32 build
	int nDarkness = 100 - (nVidScanIntensity & 0xFF) * 100 / 255;
	ImGui::SetNextItemWidth(160.0f * fUiScale);
	if (ImGui::SliderInt("Darkness", &nDarkness, 0, 100, "%d%%")) {
		int n = (100 - nDarkness) * 255 / 100;
		nVidScanIntensity = (n << 16) | (n << 8) | n;
		bVidScanlines = 1;
	}
}

static void SoftFXMenu()
{
	if (ImGui::MenuItem("None", NULL, nVidSoftFX < 0)) {
		nVidSoftFX = -1;
		UiVideoReInit();
	}
	ImGui::Separator();
	for (unsigned int i = 0; i < sizeof(SoftFXList) / sizeof(SoftFXList[0]); i++) {
		int nEffect = SoftFXList[i];
		char szLabel[64];
		snprintf(szLabel, sizeof(szLabel), "%s (%dx)", VidSoftFXGetEffect(nEffect), VidSoftFXGetZoom(nEffect));
		if (ImGui::MenuItem(szLabel, NULL, nVidSoftFX == nEffect)) {
			nVidSoftFX = nEffect;
			UiVideoReInit();
		}
	}
}

void UiVideoMenu()
{
	if (ImGui::BeginMenu("Fullscreen")) {
		FullscreenMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Window Size", !bAppFullscreen)) {
		for (int i = 1; i <= 4; i++) {
			char szLabel[8];
			snprintf(szLabel, sizeof(szLabel), "%dx", i);
			if (ImGui::MenuItem(szLabel, NULL, nWindowScale == i)) {
				UiSetWindowScale(i);
			}
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Maximum Size", NULL, nWindowScale <= 0)) {
			UiSetWindowScale(0);
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Stretch")) {
		StretchMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Monitor")) {
		MonitorMenu();
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Rotate Vertical Games", NULL, !(nVidRotationAdjust & 1))) {
		nVidRotationAdjust ^= 1;
		UiRefitWindow();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Gamma Correction")) {
		GammaMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Scanlines")) {
		ScanlinesMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Software Effect")) {
		SoftFXMenu();
		ImGui::EndMenu();
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
	ImGui::Separator();
#if SDL_VERSION_ATLEAST(2, 0, 18)
	if (ImGui::MenuItem("VSync", NULL, vsync != 0)) {
		vsync = !vsync;
		SDL_RenderSetVSync(sdlRenderer, vsync);
	}
#endif
	if (ImGui::MenuItem("Auto Frameskip", NULL, !bAlwaysDrawFrames)) {
		bAlwaysDrawFrames = !bAlwaysDrawFrames;
	}
	if (ImGui::MenuItem("Run-Ahead (one frame less of lag)", NULL, bRunAhead != 0)) {
		bRunAhead = !bRunAhead;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Show FPS", UiHotkeyName(UI_HK_FPS), bAppShowFPS)) {
		bAppShowFPS = !bAppShowFPS;
	}
}

// The driver, the device and the buffer are taken at once, the sample rate by the next game
static void AudioOutputChanged()
{
	if (!UiGameRunning()) {
		AudSDLPickDriver();					// the device list is the driver's
	} else if (bAudOkay) {
		RunAudioReInit();					// a game without sound gets it with the next one
	}
}

static void DriverMenu()
{
	// the default one is the one in use while it's picked
	const char* pszCurrent = SDL_GetCurrentAudioDriver();
	char szDefault[64] = "Default";
	if (szAudSDLDriver[0] == '\0' && pszCurrent) {
		snprintf(szDefault, sizeof(szDefault), "Default (%s)", pszCurrent);
	}
	if (ImGui::MenuItem(szDefault, NULL, szAudSDLDriver[0] == '\0')) {
		szAudSDLDriver[0] = '\0';
		szAudSDLDevice[0] = '\0';			// the devices are the driver's
		AudioOutputChanged();
	}
	ImGui::Separator();
	for (int i = 0; i < SDL_GetNumAudioDrivers(); i++) {
		const char* pszDriver = SDL_GetAudioDriver(i);
		if (pszDriver == NULL || !strcmp(pszDriver, "dummy") || !strcmp(pszDriver, "disk")) {
			continue;
		}
		if (ImGui::MenuItem(pszDriver, NULL, !strcmp(szAudSDLDriver, pszDriver))) {
			snprintf(szAudSDLDriver, sizeof(szAudSDLDriver), "%s", pszDriver);
			szAudSDLDevice[0] = '\0';
			AudioOutputChanged();
		}
	}
}

static void DeviceMenu()
{
	if (ImGui::MenuItem("Default", NULL, szAudSDLDevice[0] == '\0')) {
		szAudSDLDevice[0] = '\0';
		AudioOutputChanged();
	}
	ImGui::Separator();
	// the devices of the driver in use
	for (int i = 0; i < SDL_GetNumAudioDevices(0); i++) {
		const char* pszDevice = SDL_GetAudioDeviceName(i, 0);
		if (pszDevice == NULL) {
			continue;
		}
		ImGui::PushID(i);
		if (ImGui::MenuItem(pszDevice, NULL, !strcmp(szAudSDLDevice, pszDevice))) {
			snprintf(szAudSDLDevice, sizeof(szAudSDLDevice), "%s", pszDevice);
			AudioOutputChanged();
		}
		ImGui::PopID();
	}
}

void UiAudioMenu()
{
	bool bRunning = UiGameRunning();

	int nVolume = nAudVolume / 100;
	ImGui::SetNextItemWidth(160.0f * fUiScale);
	if (ImGui::SliderInt("Volume", &nVolume, 0, 200, "%d%%")) {
		nAudVolume = nVolume * 100;
		AudSoundSetVolume();
	}
	ImGui::TextDisabled("Above 100%% it amplifies");
	if (ImGui::MenuItem("Mute", UiHotkeyName(UI_HK_MUTE), UiMuted())) {
		UiToggleMute();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Output Driver")) {
		DriverMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Output Device")) {
		DeviceMenu();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Sample Rate")) {
		static const int nRates[] = { 11025, 22050, 44100, 48000 };
		for (int i = 0; i < 4; i++) {
			char szLabel[16];
			snprintf(szLabel, sizeof(szLabel), "%d Hz", nRates[i]);
			if (ImGui::MenuItem(szLabel, NULL, nAudSampleRate[0] == nRates[i])) {
				nAudSampleRate[0] = nRates[i];
			}
		}
		if (bRunning) {
			ImGui::TextDisabled("Used from the next game on");
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Buffer Size")) {
		// nAudSegCount frames of sound, the fewer the less latency
		for (int n = 2; n <= 12; n++) {
			char szLabel[32];
			snprintf(szLabel, sizeof(szLabel), "%d Frames (%d ms)", n, n * 100000 / (nBurnFPS > 0 && bRunning ? nBurnFPS : 6000));
			if (ImGui::MenuItem(szLabel, NULL, nAudSegCount == n)) {
				nAudSegCount = n;
				AudioOutputChanged();
			}
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Low-Pass Filter", NULL, nAudDSPModule[0] != 0)) {
		nAudDSPModule[0] = nAudDSPModule[0] ? 0 : 1;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("2-Point 1st Order Sample Interpolation", NULL, nInterpolation == 1)) {
		nInterpolation = 1;
	}
	if (ImGui::MenuItem("4-Point 3rd Order Sample Interpolation", NULL, nInterpolation == 3)) {
		nInterpolation = 3;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("No FM Interpolation", NULL, nFMInterpolation == 0)) {
		nFMInterpolation = 0;
	}
	if (ImGui::MenuItem("4-Point 3rd Order FM Interpolation", NULL, nFMInterpolation == 3)) {
		nFMInterpolation = 3;
	}
	ImGui::Separator();
	UiWaveMenuItems();					// ui_record.cpp
}
