// Burner Config file module
#include "burner.h"
#ifdef BUILD_SDL2_IMGUI
#include "ui.h"
#include "gamelist_model.h"
extern INT32 nSocd[6];			// burn/devices/joyprocess.cpp
extern int vsync;				// main.cpp
extern char videofiltering[3];
extern INT32 BurnShiftEnabled;	// burn/burn_shift.cpp
extern bool bBurnGunDrawReticles;	// burn/burn_gun.cpp
void CmdLineOptionsForSave(bool bSave);
#endif

int nIniVersion = 0;

#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
void FixAndCreateSupportPath(TCHAR *pSupportFolderPath, TCHAR *pBaseFolderName)
{
	TCHAR *szSupportFolderFixedPath = NULL;

	if (strstr(pSupportFolderPath, pBaseFolderName) == NULL) {
		if (pSupportFolderPath[strlen(pSupportFolderPath) - 1] == '/') pSupportFolderPath[strlen(pSupportFolderPath) - 1] = '\0';
		szSupportFolderFixedPath = SDL_GetPrefPath("fbneo", pSupportFolderPath);
		_stprintf(pSupportFolderPath, _T("%s"), szSupportFolderFixedPath);
		SDL_free(szSupportFolderFixedPath);
	}
}

void InitSupportPaths()
{
	TCHAR *szBaseFolderName = NULL;
	szBaseFolderName = SDL_GetPrefPath(NULL, "fbneo");
	FixAndCreateSupportPath(szAppPreviewsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppTitlesPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppCheatsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppHiscorePath, szBaseFolderName);
	FixAndCreateSupportPath(szAppSamplesPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppHDDPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppIpsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppIconsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppBlendPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppSelectPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppVersusPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppHowtoPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppScoresPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppBossesPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppGameoverPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppFlyersPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppMarqueesPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppControlsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppCabinetsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppPCBsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppHistoryPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppCommandPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppEEPROMPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppListsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppDatListsPath, szBaseFolderName);
	FixAndCreateSupportPath(szAppArchivesPath, szBaseFolderName);

	TCHAR szAppPresets[MAX_PATH] = _T("config/presets/");
	FixAndCreateSupportPath(szAppPresets, szBaseFolderName);

	SDL_free(szBaseFolderName);
}
#endif

static void CreateConfigName(char* szConfig)
{
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	TCHAR *szSDLconfigPath = NULL;
	szSDLconfigPath = SDL_GetPrefPath("fbneo", "config");
	_stprintf(szConfig, _T("%sfbneo.ini"), szSDLconfigPath);
	SDL_free(szSDLconfigPath);
#else
	_stprintf(szConfig, _T("fbneo.ini"));
#endif

	return;
}

// Read in the config file for the whole application
int ConfigAppLoad()
{
	char  szConfig[MAX_PATH];
	FILE* f;

	CreateConfigName(szConfig);

	if ((f = _tfopen(szConfig, _T("rt"))) == NULL)
	{
		return 1;
	}
	printf("Loading config from %s\n", szConfig);
#define VAR(x)    { char *szValue = LabelCheck(szLine, #x); \
                    if (szValue) { x = strtol(szValue, NULL, 0); } }
#define FLT(x)    { char *szValue = LabelCheck(szLine, #x); \
                    if (szValue) { x = atof(szValue); } }
#define STR(x)    { char *szValue = LabelCheck(szLine, #x " "); \
                    if (szValue) { strcpy(x, szValue); } }

	// Go through each line of the config file
	char szLine[256];
	while (fgets(szLine, sizeof(szLine), f))
	{
		// Get rid of the linefeed and carriage return at the end
		int nLen = strlen(szLine);
		if (nLen > 0 && szLine[nLen - 1] == 10)
		{
			szLine[nLen - 1] = 0;
			nLen--;
		}
		if (nLen > 0 && szLine[nLen - 1] == 13)
		{
			szLine[nLen - 1] = 0;
			nLen--;
		}

		VAR(nIniVersion);
#ifndef BUILD_PI
		VAR(nVidSelect);                           // video mode select
		VAR(bVidFullStretch);
		VAR(nAutoFireRate);
		VAR(bAlwaysMenu);
		VAR(nGameSelect);
		VAR(nFilterSelect);
		VAR(bShowAvailableOnly);
		VAR(bShowClones);
		VAR(gameSelectedFromFilter);
#endif
#ifdef BUILD_SDL2_IMGUI
		VAR(bUiMenuPinned);
		VAR(bUiPauseOnMenu);
		VAR(bUiPauseOnFocusLoss);
		VAR(nAudVolume);
		{
			char *szValue = LabelCheck(szLine, "nLoadMenuShowX");
			if (szValue) nLoadMenuShowX = strtoull(szValue, NULL, 0);
		}
		VAR(nLoadMenuShowY);
		VAR(nLoadMenuBoardTypeFilter);
		VAR(nLoadMenuGenreFilter);
		VAR(nLoadMenuFamilyFilter);
		VAR(nLoadMenuFavoritesFilter);
		VAR(nLoadMenuExpand);
		STR(szSelSearch);
		VAR(bEnableIcons);
		VAR(nIconsSize);
		VAR(bIconsOnlyParents);
		VAR(bIconsByHardwares);
		for (int i = 0; i < 6; i++) {
			char szLabel[32];
			snprintf(szLabel, sizeof(szLabel), "szSelMvsCarts[%d] ", i);
			char *szValue = LabelCheck(szLine, szLabel);
			if (szValue) snprintf(szSelMvsCarts[i], sizeof(szSelMvsCarts[i]), "%s", szValue);
		}
		for (int i = 0; i < 10; i++) {
			char szLabel[32];
			snprintf(szLabel, sizeof(szLabel), "szPrevGames[%d] ", i);
			char *szValue = LabelCheck(szLine, szLabel);
			if (szValue) snprintf(szPrevGames[i], sizeof(szPrevGames[i]), "%s", szValue);
		}
		VAR(nSavestateSlot);
		VAR(bDrvSaveAll);
		VAR(bRewindEnabled);
		{
			char *szValue = LabelCheck(szLine, "nRewindMemory");
			if (szValue) nRewindMemory = strtol(szValue, NULL, 0);
		}
		VAR(bRunAhead);
		VAR(bAlwaysDrawFrames);
		UiHotkeysLoadLine(szLine);
		VAR(nSocd[0]);
		VAR(nSocd[1]);
		VAR(nSocd[2]);
		VAR(nSocd[3]);
		VAR(nSocd[4]);
		VAR(nSocd[5]);
		for (int i = 0; i < 4; i++) {
			char szLabel[40];
			snprintf(szLabel, sizeof(szLabel), "nPlayerDefaultControls[%d]", i);
			char *szValue = LabelCheck(szLine, szLabel);
			if (szValue) nPlayerDefaultControls[i] = strtol(szValue, NULL, 0);
			snprintf(szLabel, sizeof(szLabel), "szPlayerDefaultIni[%d] ", i);
			szValue = LabelCheck(szLine, szLabel);
			if (szValue) snprintf(szPlayerDefaultIni[i], MAX_PATH, "%s", szValue);
		}
		// video
		VAR(vsync);
		VAR(bAppFullscreen);
		VAR(nWindowScale);
		VAR(bVidCorrectAspect);
		VAR(bIntegerScale);
		{
			char *szValue = LabelCheck(szLine, "videofiltering");
			if (szValue) {
				int n = strtol(szValue, NULL, 0);
				if (n >= 0 && n <= 2) snprintf(videofiltering, 3, "%d", n);
			}
		}
		VAR(nVidSoftFX);
		VAR(bVidScanRotate);
		VAR(nVidScanIntensity);
		VAR(bMonitorAutoCheck);
		VAR(nVidScrnAspectX);
		VAR(nVidScrnAspectY);
		VAR(nVidVerScrnAspectX);
		VAR(nVidVerScrnAspectY);
		VAR(nVidRotationAdjust);
		VAR(bVidAutoSwitchFull);
		VAR(bVidExclusiveFullscreen);
		VAR(nVidFullDisplay[0]);
		VAR(nVidFullDisplay[1]);
		VAR(nVidFullWidth[0]);
		VAR(nVidFullHeight[0]);
		VAR(nVidFullWidth[1]);
		VAR(nVidFullHeight[1]);
		// audio
		{
			char *szValue = LabelCheck(szLine, "szAudSDLDriver ");
			if (szValue) snprintf(szAudSDLDriver, sizeof(szAudSDLDriver), "%s", szValue);
			szValue = LabelCheck(szLine, "szAudSDLDevice ");
			if (szValue) snprintf(szAudSDLDevice, sizeof(szAudSDLDevice), "%s", szValue);
		}
		VAR(nAudSegCount);
		// misc
		VAR(bUiShowModeIndicator);
		VAR(bUiScanAtStart);
		VAR(bUiAutoSaveInputs);
		VAR(nUiTheme);
		VAR(bBurnUseBlend);
		VAR(BurnShiftEnabled);
		VAR(bBurnGunDrawReticles);
		STR(szUiCdLibraryDir);
		VAR(bUiCdLibrarySubdirs);
		VAR(bReplayReadOnly);
		VAR(bReplayFrameCounterDisplay);
		VAR(nIpsSelectedLanguage);
		STR(szAppCdCoversPath);
		STR(szAppCdPreviewsPath);
#endif
		VAR(bVidScanlines);
		VAR(bDoGamma);
		FLT(nGamma);
		VAR(nAudSampleRate[0]);
		VAR(nAudDSPModule[0]);
		VAR(nInterpolation);
		VAR(nFMInterpolation);
		VAR(EnableHiscores);
		// Other
		STR(szAppRomPaths[0]);
		STR(szAppRomPaths[1]);
		STR(szAppRomPaths[2]);
		STR(szAppRomPaths[3]);
		STR(szAppRomPaths[4]);
		STR(szAppRomPaths[5]);
		STR(szAppRomPaths[6]);
		STR(szAppRomPaths[7]);
		STR(szAppRomPaths[8]);
		STR(szAppRomPaths[9]);
		STR(szAppRomPaths[10]);
		STR(szAppRomPaths[11]);
		STR(szAppRomPaths[12]);
		STR(szAppRomPaths[13]);
		STR(szAppRomPaths[14]);
		STR(szAppRomPaths[15]);
		STR(szAppRomPaths[16]);
		STR(szAppRomPaths[17]);
		STR(szAppRomPaths[18]);
		STR(szAppRomPaths[19]);
		STR(szAppPreviewsPath);
		STR(szAppTitlesPath);
		STR(szAppCheatsPath);
		STR(szAppHiscorePath);
		STR(szAppSamplesPath);
		STR(szAppHDDPath);
		STR(szAppIpsPath);
		STR(szAppIconsPath);
		STR(szAppBlendPath);
		STR(szAppSelectPath);
		STR(szAppVersusPath);
		STR(szAppHowtoPath);
		STR(szAppScoresPath);
		STR(szAppBossesPath);
		STR(szAppGameoverPath);
		STR(szAppFlyersPath);
		STR(szAppMarqueesPath);
		STR(szAppControlsPath);
		STR(szAppCabinetsPath);
		STR(szAppPCBsPath);
		STR(szAppHistoryPath);
		STR(szAppCommandPath);
		STR(szAppEEPROMPath);
		STR(szAppListsPath);
		STR(szAppDatListsPath);
		STR(szAppArchivesPath);

#undef STR
#undef FLT
#undef VAR
	}

	fclose(f);
	return 0;
}

// Write out the config file for the whole application
int ConfigAppSave()
{
	char  szConfig[MAX_PATH];
	FILE* f;

	CreateConfigName(szConfig);

	if ((f = _tfopen(szConfig, _T("wt"))) == NULL)
	{
		return 1;
	}
#ifdef BUILD_SDL2_IMGUI
	CmdLineOptionsForSave(true);	// the ini keeps its values for the options of the command line
#endif

#define VAR(x)    fprintf(f, #x " %d\n", x)
#define FLT(x)    fprintf(f, #x " %f\n", x)
#define STR(x)    fprintf(f, #x " %s\n", x)

	fprintf(f, "\n// The application version this file was saved from\n");
	// We can't use the macros for this!
#ifdef BUILD_SDL2_IMGUI
	// the version of the license accepted on the first run (ui_misc.cpp), the
	// question comes back if the window was closed without an answer
	fprintf(f, "nIniVersion 0x%06X", nIniVersion);
#else
	fprintf(f, "nIniVersion 0x%06X", nBurnVer);
#endif

#ifndef BUILD_PI
	fprintf(f, "\n// video mode 0 = standard SDL. 1 = SDL1 opengl (don't use on SDL2!!!!!!)\n");
	VAR(nVidSelect);              // video mode select
	fprintf(f, "\n// If non-zero, allow stretching of the image to any size\n");
	VAR(bVidFullStretch);
	fprintf(f, "\n// Auto-Fire Rate, non-linear - use the GUI to change this setting!\n");
	VAR(nAutoFireRate);
	fprintf(f, "\n// Automatically go to the menu\n");
	VAR(bAlwaysMenu);
	fprintf(f, "\n// Last game selection\n");
	VAR(nGameSelect);
	fprintf(f, "\n// Last filter selection\n");
	VAR(nFilterSelect);
	fprintf(f, "\n// show available game only filter\n");
	VAR(bShowAvailableOnly);
	fprintf(f, "\n// show clones game filter\n");
	VAR(bShowClones);
	fprintf(f, "\n// Last game selected from filter\n");
	VAR(gameSelectedFromFilter);
#endif
#ifdef BUILD_SDL2_IMGUI
	fprintf(f, "\n// If non-zero, the menu bar is always shown in a window (it floats over the game otherwise)\n");
	VAR(bUiMenuPinned);
	fprintf(f, "\n// If non-zero, pause the game while a menu or a dialog is open\n");
	VAR(bUiPauseOnMenu);
	fprintf(f, "\n// If non-zero, pause the game when the window loses the focus\n");
	VAR(bUiPauseOnFocusLoss);
	fprintf(f, "\n// Sound volume (0 - 10000)\n");
	VAR(nAudVolume);
	fprintf(f, "\n// Game selector filters, a set bit hides the games that have it (the same as the Win32 build)\n");
	fprintf(f, "nLoadMenuShowX 0x%016llX\n", (unsigned long long)nLoadMenuShowX);
	VAR(nLoadMenuShowY);
	VAR(nLoadMenuBoardTypeFilter);
	VAR(nLoadMenuGenreFilter);
	VAR(nLoadMenuFamilyFilter);
	VAR(nLoadMenuFavoritesFilter);
	VAR(nLoadMenuExpand);
	fprintf(f, "\n// Game selector search\n");
	STR(szSelSearch);
	fprintf(f, "\n// Game selector icons, size 0 = 16, 1 = 24, 2 = 32 pixels\n");
	VAR(bEnableIcons);
	VAR(nIconsSize);
	VAR(bIconsOnlyParents);
	VAR(bIconsByHardwares);
	fprintf(f, "\n// The cartridges in the slots of the Neo Geo MVS system\n");
	for (int i = 0; i < 6; i++) {
		fprintf(f, "szSelMvsCarts[%d] %s\n", i, szSelMvsCarts[i]);
	}
	fprintf(f, "\n// The most recent games\n");
	for (int i = 0; i < 10; i++) {
		fprintf(f, "szPrevGames[%d] %s\n", i, szPrevGames[i]);
	}
	fprintf(f, "\n// The active save state slot (1 - 8)\n");
	VAR(nSavestateSlot);
	fprintf(f, "\n// If non-zero, the automatic state of a game (saved when it ends) holds everything, not only the NVRAM\n");
	VAR(bDrvSaveAll);
	fprintf(f, "\n// Rewind, and the memory it can use in MB\n");
	VAR(bRewindEnabled);
	fprintf(f, "nRewindMemory %d\n", (int)nRewindMemory);
	fprintf(f, "\n// If non-zero, run ahead one frame to take one frame of lag away\n");
	VAR(bRunAhead);
	fprintf(f, "\n// If zero, frames are skipped when the emulation is too slow\n");
	VAR(bAlwaysDrawFrames);
	UiHotkeysSave(f);
	fprintf(f, "\n// SOCD mode of each player: 0 off, 1 N, 2 L (4-way), 3 L (8-way), 4 F, 5 U, 6 D+L\n");
	VAR(nSocd[0]);
	VAR(nSocd[1]);
	VAR(nSocd[2]);
	VAR(nSocd[3]);
	VAR(nSocd[4]);
	VAR(nSocd[5]);
	fprintf(f, "\n// The default controls of the players: device | analog mode << 4, or a preset file\n");
	for (int i = 0; i < 4; i++) {
		fprintf(f, "nPlayerDefaultControls[%d] %d\n", i, nPlayerDefaultControls[i]);
		fprintf(f, "szPlayerDefaultIni[%d] %s\n", i, szPlayerDefaultIni[i]);
	}
	fprintf(f, "\n// If non-zero, wait for the vertical sync of the display\n");
	VAR(vsync);
	fprintf(f, "\n// Fullscreen, and the size of the window for a game (1 - 4 times, 0 = maximized)\n");
	VAR(bAppFullscreen);
	VAR(nWindowScale);
	fprintf(f, "\n// Stretch: bVidFullStretch fills the window, bVidCorrectAspect keeps the aspect ratio (Normal without both), bIntegerScale whole multiples\n");
	VAR(bVidCorrectAspect);
	VAR(bIntegerScale);
	fprintf(f, "\n// Filtering of the scaled picture: 0 nearest, 1 linear, 2 best\n");
	fprintf(f, "videofiltering %c\n", videofiltering[0]);
	fprintf(f, "\n// Software effect (the index of vid_softfx.h), -1 = none\n");
	VAR(nVidSoftFX);
	fprintf(f, "\n// Scanlines turn with the vertical games, and their brightness (0x000000 black - 0xFFFFFF none)\n");
	VAR(bVidScanRotate);
	fprintf(f, "nVidScanIntensity 0x%06X\n", nVidScanIntensity);
	fprintf(f, "\n// If zero, the aspect ratio of the monitor (horizontal, and vertical in fullscreen) instead of square pixels\n");
	VAR(bMonitorAutoCheck);
	VAR(nVidScrnAspectX);
	VAR(nVidScrnAspectY);
	VAR(nVidVerScrnAspectX);
	VAR(nVidVerScrnAspectY);
	fprintf(f, "\n// Vertical games: 1 = not rotated, 2 = rotated the other way (mirrored vertical alignment)\n");
	VAR(nVidRotationAdjust);
	fprintf(f, "\n// If non-zero, go to fullscreen when a game is loaded\n");
	VAR(bVidAutoSwitchFull);
	fprintf(f, "\n// Fullscreen of the games: exclusive mode (another resolution, 0 = the desktop's), and the display (-1 = the window's), [0] horizontal and [1] vertical games\n");
	VAR(bVidExclusiveFullscreen);
	VAR(nVidFullDisplay[0]);
	VAR(nVidFullDisplay[1]);
	VAR(nVidFullWidth[0]);
	VAR(nVidFullHeight[0]);
	VAR(nVidFullWidth[1]);
	VAR(nVidFullHeight[1]);
	fprintf(f, "\n// SDL audio driver and output device, empty for the default ones\n");
	STR(szAudSDLDriver);
	STR(szAudSDLDevice);
	fprintf(f, "\n// Frames of sound in the buffer\n");
	VAR(nAudSegCount);
	fprintf(f, "\n// If non-zero, show FFWD / PAUSE over the game\n");
	VAR(bUiShowModeIndicator);
	fprintf(f, "\n// If non-zero, scan the roms at start-up when there's no list of the available ones\n");
	VAR(bUiScanAtStart);
	fprintf(f, "\n// If non-zero, the input mapping of a game is saved when it ends\n");
	VAR(bUiAutoSaveInputs);
	fprintf(f, "\n// Interface theme: 0 dark, 1 light, 2 classic\n");
	VAR(nUiTheme);
	fprintf(f, "\n// If non-zero, use the alpha blend effects, show the gear-shift indicator and the lightgun reticles of the games that have them\n");
	VAR(bBurnUseBlend);
	VAR(BurnShiftEnabled);
	VAR(bBurnGunDrawReticles);
	fprintf(f, "\n// The folder of the CD library (Neo Geo CD, PC Engine CD), and its subfolders\n");
	STR(szUiCdLibraryDir);
	VAR(bUiCdLibrarySubdirs);
	fprintf(f, "\n// Input playback: read-only, the frame counter over the game\n");
	VAR(bReplayReadOnly);
	VAR(bReplayFrameCounterDisplay);
	fprintf(f, "\n// The language of the IPS patch descriptions (0 English, 1 Simplified Chinese...)\n");
	VAR(nIpsSelectedLanguage);
	fprintf(f, "\n// The covers and previews of the CD library (include trailing slash)\n");
	STR(szAppCdCoversPath);
	STR(szAppCdPreviewsPath);
#endif
	fprintf(f, "\n// If non-zero, enable scanlines\n");
	VAR(bVidScanlines);
	fprintf(f, "\n// If non-zero, enable software gamma correction\n");
	VAR(bDoGamma);
	_ftprintf(f, _T("\n// Gamma to correct with\n"));
	FLT(nGamma);
	fprintf(f, "\n// Audio sample rate\n");
	VAR(nAudSampleRate[0]);
	fprintf(f, "\n// If non-zero, enable DSP filter\n");
	VAR(nAudDSPModule[0]);
	_ftprintf(f, _T("\n// The order of PCM/ADPCM interpolation\n"));
	VAR(nInterpolation);
	_ftprintf(f, _T("\n// The order of FM interpolation\n"));
	VAR(nFMInterpolation);
	_ftprintf(f, _T("\n// If non-zero, enable high score saving support.\n"));
	VAR(EnableHiscores);

	fprintf(f, "\n// The paths to search for rom zips. (include trailing slash)\n");
	STR(szAppRomPaths[0]);
	STR(szAppRomPaths[1]);
	STR(szAppRomPaths[2]);
	STR(szAppRomPaths[3]);
	STR(szAppRomPaths[4]);
	STR(szAppRomPaths[5]);
	STR(szAppRomPaths[6]);
	STR(szAppRomPaths[7]);
	STR(szAppRomPaths[8]);
	STR(szAppRomPaths[9]);
	STR(szAppRomPaths[10]);
	STR(szAppRomPaths[11]);
	STR(szAppRomPaths[12]);
	STR(szAppRomPaths[13]);
	STR(szAppRomPaths[14]);
	STR(szAppRomPaths[15]);
	STR(szAppRomPaths[16]);
	STR(szAppRomPaths[17]);
	STR(szAppRomPaths[18]);
	STR(szAppRomPaths[19]);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppPreviewsPath);
	fprintf(f, "\n// Path to titlescreen images for use on the menu (include trailing slash)\n");
	STR(szAppTitlesPath);
	fprintf(f, "\n// Cheat files path (include trailing slash)\n");
	STR(szAppCheatsPath);
	fprintf(f, "\n// Hiscore save path (include trailing slash)\n");
	STR(szAppHiscorePath);
	fprintf(f, "\n// Game Samples (where required) (include trailing slash)\n");
	STR(szAppSamplesPath);
	fprintf(f, "\n// HDD image path (include trailing slash)\n");
	STR(szAppHDDPath);
	fprintf(f, "\n// EEPROM save path (include trailing slash)\n");
	STR(szAppEEPROMPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppIpsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppIconsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppBlendPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppSelectPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppVersusPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppHowtoPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppScoresPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppBossesPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppGameoverPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppFlyersPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppMarqueesPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppControlsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppCabinetsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppPCBsPath);
	fprintf(f, "\n// history.xml path (include trailing slash)\n");
	STR(szAppHistoryPath);
	fprintf(f, "\n// command.dat path (include trailing slash)\n");
	STR(szAppCommandPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppListsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppDatListsPath);
	fprintf(f, "\n// UNUSED CURRENTLY (include trailing slash)\n");
	STR(szAppArchivesPath);
	fprintf(f, "\n\n\n");

#undef STR
#undef FLT
#undef VAR

	fclose(f);
#ifdef BUILD_SDL2_IMGUI
	CmdLineOptionsForSave(false);
#endif
	return 0;
}
