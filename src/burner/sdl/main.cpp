/*----------------
 * Stuff to finish:
 *
 * It wouldn't be a stretch of the imagination to think the whole of the sdl 'port' needs a redo but here are the main things wrong with this version:
 *
 *
 * The audio output code code could maybe do with some work, but it's getting there...
 * There are lots of problems with the opengl renderer, but you should just use the SDL2 build anyway
 *
 * TODO for SDL2:
 * Add menu for options e.g., saves, IPS patches
 * Maybe a better font output setup with some sort of scaling, maybe add sdl1 support?
 * figure out what is going on with the sdl sound output, something breaks after a few frames
 * ------------------*/

#include "burner.h"
#ifdef BUILD_SDL2_IMGUI
#include "ui.h"
#endif
#ifndef _WIN32
#include <sys/stat.h>
#endif

INT32 display_set_controls();

int  nAppVirtualFps = 0;         // App fps * 100
bool bRunPause = 0;
bool bAppFullscreen = 0;
bool bAlwaysProcessKeyboardInput = 0;
int usemenu = 0, usejoy = 0, vsync = 1, dat = 0;
bool bSaveconfig = 1;
bool bIntegerScale = false;
int nWindowScale = 2;			// Default to 2 for compatibility with previous hard coded value.
bool bAlwaysMenu = false;
int nGameSelect = 0;
int nFilterSelect = HARDWARE_PUBLIC_MASK;
bool bShowAvailableOnly = true;
bool bShowClones = true;
int gameSelectedFromFilter = -1;
TCHAR szAppBurnVer[16];
char videofiltering[3];
bool gamefound = 0;
const char* romname = NULL;
static bool bCmdLineIps = false;		// -ips: the game loads with its IPS patches
static const char* pszCmdLineIps = NULL;	// -ips a,b: those ones, saved as its patches
static const char* pszCmdLineRomData = NULL;	// -romdata <dat>: the game of a RomData dat

extern void InitSupportPaths();

#ifdef BUILD_SDL2
SDL_Window* sdlWindow = NULL;
#endif

#define set_commandline_option_not_config(i,v) i = v;
#ifdef BUILD_SDL2_IMGUI
// The video options of the command line are used for this run, over the ini,
// which keeps its own values unless they are changed in the menus
enum { CL_VSYNC, CL_INTEGERSCALE, CL_WINDOWSCALE, CL_FULLSCREEN, CL_FILTER, CL_COUNT };
static int nCmdLine[CL_COUNT] = { -1, -1, -1, -1, -1 };		// -1 = not given
static int nCmdLineIni[CL_COUNT];
static bool bCmdLineSwapped[CL_COUNT];

static int CmdLineGet(int n)
{
	switch (n) {
		case CL_VSYNC:			return vsync;
		case CL_INTEGERSCALE:	return bIntegerScale;
		case CL_WINDOWSCALE:	return nWindowScale;
		case CL_FULLSCREEN:		return bAppFullscreen;
		case CL_FILTER:			return videofiltering[0] - '0';
	}
	return 0;
}

static void CmdLineSet(int n, int v)
{
	switch (n) {
		case CL_VSYNC:			vsync = v; break;
		case CL_INTEGERSCALE:	bIntegerScale = v != 0; break;
		case CL_WINDOWSCALE:	nWindowScale = v; break;
		case CL_FULLSCREEN:		bAppFullscreen = v != 0; break;
		case CL_FILTER:			snprintf(videofiltering, 3, "%d", v); break;
	}
}

// After ConfigAppLoad()
void CmdLineOptionsApply()
{
	for (int n = 0; n < CL_COUNT; n++) {
		if (nCmdLine[n] >= 0) {
			nCmdLineIni[n] = CmdLineGet(n);
			CmdLineSet(n, nCmdLine[n]);
		}
	}
}

// ConfigAppSave(): the ini values in place of the options still as the command line gave them
void CmdLineOptionsForSave(bool bSave)
{
	for (int n = 0; n < CL_COUNT; n++) {
		if (bSave) {
			bCmdLineSwapped[n] = nCmdLine[n] >= 0 && CmdLineGet(n) == nCmdLine[n];
			if (bCmdLineSwapped[n]) CmdLineSet(n, nCmdLineIni[n]);
		} else if (bCmdLineSwapped[n]) {
			CmdLineSet(n, nCmdLine[n]);
			bCmdLineSwapped[n] = false;
		}
	}
}

#define set_commandline_option(i,v,n) nCmdLine[n] = v;
#define set_commandline_option_string(i, v, length, n) nCmdLine[n] = atoi(v);
#else
#define set_commandline_option(i,v,n) i = v; bSaveconfig = 0;
#define set_commandline_option_string(i, v, length, n) snprintf(i, length, v); bSaveconfig = 0;
#endif

int parseSwitches(int argc, char* argv[])
{
	for (int i = 1; i < argc; i++)
	{
		if (*argv[i] != '-' && !gamefound)
		{
			romname = argv[i];
			gamefound = 1;
		}
		if (strcmp(argv[i], "-joy") == 0)
		{
			set_commandline_option_not_config(usejoy, 1);
		}
		else if (strcmp(argv[i], "-menu") == 0)
		{
			set_commandline_option_not_config(usemenu, 1);
		}
		else if (strcmp(argv[i], "-novsync") == 0)
		{
			set_commandline_option(vsync, 0, CL_VSYNC);
		}
		else if (strcmp(argv[i], "-integerscale") == 0)
		{
			set_commandline_option(bIntegerScale, 1, CL_INTEGERSCALE);
		}
		else if (strcmp(argv[i], "-windowscale") == 0)
		{
			int num;

			if (++i >= argc)
			{
				return 1;
			}

			num = atoi(argv[i]);
			if (num < 1 || 20 < num)	// Check it's not too small or ridiculously too big.
			{
				return 1;
			}
			set_commandline_option(nWindowScale, num, CL_WINDOWSCALE);
		}
		else if (strcmp(argv[i], "-dat") == 0)
		{
			set_commandline_option_not_config(dat, 1);
		}
		else if (strcmp(argv[i], "-fullscreen") == 0)
		{
			set_commandline_option(bAppFullscreen, 1, CL_FULLSCREEN);
		}
		else if (strcmp(argv[i], "-nearest") == 0)
		{
			set_commandline_option_string(videofiltering, "0", 3, CL_FILTER);
		}
		else if (strcmp(argv[i], "-linear") == 0)
		{
			set_commandline_option_string(videofiltering, "1", 3, CL_FILTER);
		}
		else if (strcmp(argv[i], "-best") == 0)
		{
			set_commandline_option_string(videofiltering, "2", 3, CL_FILTER);
		}
		else if (strcmp(argv[i], "-ips") == 0)
		{
			bCmdLineIps = true;
			// the list comes after the game name, like in the Windows build
			if (gamefound && i + 1 < argc && argv[i + 1][0] != '-')
			{
				pszCmdLineIps = argv[++i];
			}
		}
		else if (strcmp(argv[i], "-romdata") == 0)
		{
			if (++i >= argc)
			{
				return 1;
			}
			pszCmdLineRomData = argv[i];
			gamefound = 1;					// the dat names the game
		}
		else if (strcmp(argv[i], "-autosave") == 0)
		{
			bDrvSaveAll = 1;
		}
		else if (strcmp(argv[i], "-cd") == 0)
		{
			i++;
			_tcscpy(CDEmuImage, argv[i]);
		}
	}
	return 0;
}

// Creates every directory of the path
void MakeDirs(const char* pszPath)
{
	char szDir[MAX_PATH];
	snprintf(szDir, sizeof(szDir), "%s", pszPath);

	for (char* p = szDir + 1; ; p++) {
		if (*p == '/' || *p == '\\' || *p == '\0') {
			char c = *p;
			*p = '\0';
#ifdef _WIN32
			CreateDirectoryA(szDir, NULL);
#else
			mkdir(szDir, 0755);
#endif
			*p = c;
			if (c == '\0') {
				break;
			}
		}
	}
}

// The dat files of -dat and of Misc > Generate Dat Files
const DatSystem DatSystems[] = {
#if defined(ALT_DAT)
	{ "All systems",					"FBNeo_-_ALL_SYSTEMS.dat",							-1 },
#endif
	{ "Arcade",							"FBNeo_-_Arcade.dat",								DAT_ARCADE_ONLY },
	{ "Mega Drive",						"FBNeo_-_MegaDrive.dat",							DAT_MEGADRIVE_ONLY },
	{ "PC Engine",						"FBNeo_-_PC_ENGINE.dat",							DAT_PCENGINE_ONLY },
	{ "TurboGrafx-16",					"FBNeo_-_TurboGrafx-16.dat",						DAT_TG16_ONLY },
	{ "PC Engine SuperGrafx",			"FBNeo_-_PC_Engine_SuperGrafx.dat",					DAT_SGX_ONLY },
	{ "Sega SG-1000",					"FBNeo_-_Sega_SG-1000.dat",							DAT_SG1000_ONLY },
	{ "ColecoVision",					"FBNeo_-_ColecoVision.dat",							DAT_COLECO_ONLY },
	{ "Sega Master System",				"FBNeo_-_Sega_Master_System.dat",					DAT_MASTERSYSTEM_ONLY },
	{ "Sega Game Gear",					"FBNeo_-_Sega_Game_Gear.dat",						DAT_GAMEGEAR_ONLY },
	{ "MSX",							"FBNeo_-_MSX.dat",									DAT_MSX_ONLY },
	{ "ZX Spectrum",					"FBNeo_-_Sinclair_ZX_Spectrum.dat",					DAT_SPECTRUM_ONLY },
	{ "Neo Geo",						"FBNeo_-_Neogeo.dat",								DAT_NEOGEO_ONLY },
	{ "NES",							"FBNeo_-_Nintendo_Entertainment_System.dat",		DAT_NES_ONLY },
	{ "Famicom Disk System",			"FBNeo_-_Nintendo_Famicom_Disk_System.dat",			DAT_FDS_ONLY },
	{ "SNES",							"FBNeo_-_Super_Nintendo_Entertainment_System.dat",	DAT_SNES_ONLY },
	{ "Game Boy Advance",				"FBNeo_-_Nintendo_Game_Boy_Advance.dat",			DAT_GBA_ONLY },
	{ "Neo Geo Pocket",					"FBNeo_-_Neo_Geo_Pocket.dat",						DAT_NGP_ONLY },
	{ "Fairchild Channel F",			"FBNeo_-_Fairchild_Channel_F.dat",					DAT_CHANNELF_ONLY },
	{ "Bally Astrocade",				"FBNeo_-_Bally_Astrocade.dat",						DAT_ASTROHOME_ONLY },
};
const int nDatSystems = sizeof(DatSystems) / sizeof(DatSystems[0]);

void generateDats()
{
	char filename[1024] = { 0 };

#if defined(BUILD_SDL2)
	MakeDirs(szAppDatListsPath);
	printf("Creating fbneo dats in %s\n", szAppDatListsPath);
	const char* pszFolder = szAppDatListsPath;
#else
	printf("Creating fbneo dats\n");
	const char* pszFolder = "";
#endif

	for (int i = 0; i < nDatSystems; i++)
	{
		snprintf(filename, sizeof(filename), "%s%s", pszFolder, DatSystems[i].pszFile);
		create_datfile(filename, DatSystems[i].nType);
	}
}


// -ips a,b,"c d": the dats of the folder of the game (nBurnDrvActive) become its patches
static void CmdLineIpsSave()
{
	if (pszCmdLineIps == NULL)
	{
		return;
	}

	char szList[1024];
	snprintf(szList, sizeof(szList), "%s", pszCmdLineIps);
	char szDats[32][MAX_PATH];
	const char* pszDats[32];
	int nDats = 0;

	for (char* pszName = strtok(szList, ","); pszName && nDats < 32; pszName = strtok(NULL, ","))
	{
		while (*pszName == ' ' || *pszName == '"') pszName++;
		int nLen = strlen(pszName);
		while (nLen > 0 && (pszName[nLen - 1] == ' ' || pszName[nLen - 1] == '"')) pszName[--nLen] = '\0';
		if (nLen == 0)
		{
			continue;
		}
		bool bExt = nLen > 4 && _stricmp(pszName + nLen - 4, ".dat") == 0;
		snprintf(szDats[nDats], MAX_PATH, "%s%s%s", IpsPatchFolder(BurnDrvGetTextA(DRV_NAME)), pszName, bExt ? "" : ".dat");
		FILE* fp = fopen(szDats[nDats], "rb");
		if (fp)
		{
			fclose(fp);
			pszDats[nDats] = szDats[nDats];
			nDats++;
		}
		else
		{
			printf("IPS patch not found: %s\n", szDats[nDats]);
		}
	}

	if (nDats > 0)
	{
		IpsSaveActivePatches(BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME), pszDats, nDats);
	}
}

void DoGame(int gameToRun)
{
	if (bCmdLineIps)
	{
		nBurnDrvActive = gameToRun;
		CmdLineIpsSave();
		if (LoadIpsActivePatches() > 0)
		{
			bDoIpsPatch = true;
			IpsPatchInit();
		}
	}

	if (!DrvInit(gameToRun, 0))
	{
		MediaInit();
		display_set_controls();
		RunMessageLoop();
	}
	else
	{
		printf("There was an error loading your selected game.\n");
	}

	if (bSaveconfig)
	{
		ConfigAppSave();
	}
	DrvExit();
	MediaExit();
}

void bye(void)
{
#ifdef FBNEO_DEBUG
	printf("Doing exit cleanup\n");
#endif

	DrvExit();
	MediaExit();
	BurnLibExit();
	SDL_Quit();
}

TCHAR* AdaptiveEncodingReads(const TCHAR* pszFileName)
{
	return NULL;
}

static int __cdecl AppDebugPrintf(int nStatus, TCHAR* pszFormat, ...)
{

	va_list args;
	va_start(args, pszFormat);
	vfprintf(stderr, pszFormat, args);
	va_end(args);

	return 0;
}

int main(int argc, char* argv[])
{
	UINT32      i = 0;
	int fail = 0;
	atexit(bye);
	// TODO: figure out if we can use hardware Gamma until then, force software gamma
	bVidUseHardwareGamma = 0;
	bHardwareGammaOnly = 0;
	//

	// Make version string
	if (nBurnVer & 0xFF)
	{
		// private version (alpha)
		_stprintf(szAppBurnVer, _T("%x.%x.%x.%02x"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF, nBurnVer & 0xFF);
	}
	else
	{
		// public version
		_stprintf(szAppBurnVer, _T("%x.%x.%x"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF);
	}
	printf("FBNeo v%s\n", szAppBurnVer);


	// set default videofiltering
	snprintf(videofiltering, 3, "0");

	bool switchesOK = parseSwitches(argc, argv) == 0;

	// Do these bits before override via ConfigAppLoad
	bCheatsAllowed = 1;
	nAudDSPModule[0] = 0;
	EnableHiscores = 1;

	if (!switchesOK || ((romname == NULL) && !usemenu && !bAlwaysMenu && !dat && !pszCmdLineRomData))
	{
		printf("Usage: %s [-cd] [-joy] [-menu] [-novsync] [-integerscale] [-windowscale <num>] [-fullscreen] [-dat] [-autosave] [-nearest] [-linear] [-best] <romname> [-ips [patch1,patch2...]] [-romdata <dat>]\n", argv[0]);
		printf("Note the -menu switch does not require a romname\n");
		printf("e.g.: %s mslug\n", argv[0]);
		printf("e.g.: %s -windowscale 1 asteroid\n", argv[0]);
		printf("e.g.: %s -menu -joy\n", argv[0]);
		printf("For NeoCD games:\n");
		printf("%s neocdz -cd path/to/ccd/filename.cue (or .ccd)\n", argv[0]);
		printf("Usage is restricted by the license at https://raw.githubusercontent.com/finalburnneo/FBNeo/master/src/license.txt\n");
		return 0;
	}

#ifdef BUILD_SDL
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	SDL_WM_SetCaption("FinalBurn Neo", "FinalBurn Neo");
#endif

#ifdef BUILD_SDL2
#ifdef BUILD_SDL2_IMGUI
#ifdef SDL_HINT_WINDOWS_DPI_AWARENESS		// SDL 2.24
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif
#ifdef SDL_WINDOWS
	char *base_path = SDL_GetBasePath();

	// everything (fbneo.ini, config/, roms/, support/...) is relative to the
	// executable's directory, whatever the directory it was started from
	if (CDEmuImage[0])
	{
		char szFullPath[MAX_PATH];
		if (_fullpath(szFullPath, CDEmuImage, MAX_PATH))
		{
			_tcscpy(CDEmuImage, szFullPath);
		}
	}
	if (base_path)
	{
		SetCurrentDirectoryA(base_path);
	}
#define DIRCNT 9
	// Make sure there are roms and cfg subdirectories
	TCHAR szDirs[DIRCNT][MAX_PATH] = {
		{_T("config")},
		{_T("config/games")},
		{_T("config/ips")},
		{_T("config/localisation")},
		{_T("config/presets")},
		{_T("recordings")},
		{_T("roms")},
		{_T("savestates")},
		{_T("screenshots")},
	};
	TCHAR currentPath[MAX_PATH];
	for(int x = 0; x < DIRCNT; x++) {
		snprintf(currentPath, MAX_PATH, "%s%s", base_path ? base_path : "", szDirs[x]);
		CreateDirectory(currentPath, NULL);
	}
#undef DIRCNT
	SDL_free(base_path);

	SDL_setenv("SDL_AUDIODRIVER", "directsound", false);        // fix audio for windows (unless the user picked a driver)
#endif
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}
#endif

	//SDL_ShowCursor(SDL_DISABLE);

#if defined(BUILD_SDL2)
    // Load mapping from file if it exists (the input mapping menu saves it there on every system)
	char gamecontrollerdbfile[MAX_PATH] = _T("");
	TCHAR *szSDLconfigPath = NULL;
	szSDLconfigPath = SDL_GetPrefPath("fbneo", "config");
	_stprintf(gamecontrollerdbfile, _T("%sgamecontrollerdb.txt"), szSDLconfigPath);
	SDL_free(szSDLconfigPath);
	if (SDL_GameControllerAddMappingsFromFile(gamecontrollerdbfile) > 0) printf("Game controller mappings loaded from: %s\n", gamecontrollerdbfile);
#endif

	// create a default ini if one is not valid
	fail = ConfigAppLoad();
	if (fail && bSaveconfig)
	{
		#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
		InitSupportPaths();
		#endif
		ConfigAppSave();		// for SDL2 this will also create folders in ~/.local/share/fbneo/
	}
#ifdef BUILD_SDL2_IMGUI
	CmdLineOptionsApply();
#endif
	ComputeGammaLUT();
#if defined(BUILD_SDL2)
	bprintf = AppDebugPrintf;
#endif
	BurnLibInit();

	// Search for a game now, for use in the menu and loading a games
	if (romname != NULL)
	{
		for (i = 0; i < nBurnDrvCount; i++)
		{
			nBurnDrvActive = i;
			if (strcmp(BurnDrvGetTextA(DRV_NAME), romname) == 0)
			{
				break;
			}
		}
	}

	if (dat)
	{
		generateDats();
	}
#ifdef BUILD_SDL2_IMGUI
	else if (pszCmdLineRomData)
	{
		// the game of the dat, as if it was given by its name
		if (!UiRomDataPlay(pszCmdLineRomData))
		{
			printf("%s can't be used as a RomData dat.\n", pszCmdLineRomData);
			return 1;
		}
		UiMain(nBurnDrvActive, usemenu || bAlwaysMenu);
		if (bSaveconfig)
		{
			ConfigAppSave();
		}
	}
	else if (romname == NULL || i < nBurnDrvCount)		// an unknown romname is reported below
	{
		if (romname != NULL && bCmdLineIps)
		{
			nBurnDrvActive = i;
			CmdLineIpsSave();
			UiIpsArmLaunch(true);
		}
		UiMain(romname != NULL ? (int)i : -1, usemenu || bAlwaysMenu);
		if (bSaveconfig)
		{
			ConfigAppSave();
		}
	}
#elif defined(BUILD_SDL2)
	else if (usemenu || bAlwaysMenu)
	{
		bool quit = 0;

		while (!quit)
		{
			gui_init();
			int selectedOk = gui_process();
			gui_exit();

			switch (selectedOk)
			{
			case -1:			
				quit = 1;
				return 0;

			default:
				DoGame(selectedOk);
				break;
			}
		}
	}
#endif
	else
	{
		if (i == nBurnDrvCount)
		{
			printf("%s is not supported by FinalBurn Neo.\n", romname);
			return 1;
		}

		DoGame(i);
	}
	return 0;
}

/* const */ TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize)
{
#if defined (UNICODE)
	static TCHAR szStringBuffer[1024];

	TCHAR* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int    nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (MultiByteToWideChar(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize))
	{
		return pszBuffer;
	}

	return NULL;
#else
	if (pszOutString)
	{
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (TCHAR*)pszInString;
#endif
}

/* const */ char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize)
{
#if defined (UNICODE)
	static char szStringBuffer[1024];
	memset(szStringBuffer, 0, sizeof(szStringBuffer));

	char* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int   nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (WideCharToMultiByte(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize, NULL, NULL))
	{
		return pszBuffer;
	}

	return NULL;
#else
	if (pszOutString)
	{
		strcpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
#endif
}

bool AppProcessKeyboardInput()
{
#ifdef BUILD_SDL2_IMGUI
	return !UiWantsKeyboard();		// the game doesn't see the keys typed in the interface
#else
	return true;
#endif
}
