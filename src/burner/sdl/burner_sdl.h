// Header for SDL 1.2 & SDL2
#include <SDL.h>

#ifdef BUILD_SDL2
#include "sdl2_gui.h"
#include "sdl2_inprint.h"
#endif

// defines to override various #ifndef _WIN32
#ifndef _WIN32
typedef struct tagRECT
{
	int left;
	int top;
	int right;
	int bottom;
} RECT, * PRECT, * LPRECT;
typedef const RECT* LPCRECT;
typedef UINT32 DWORD;
typedef UINT8 BYTE;
#else
#include <windows.h>
#endif


#ifndef MAX_PATH
#define MAX_PATH    511
#endif

#ifndef __cdecl
#define __cdecl
#endif

//main.cpp
int SetBurnHighCol(int nDepth);

extern int   nAppVirtualFps;
extern bool  bRunPause;
extern bool  bAppDoFast;    // TODO: bad
extern char  fpsstring[20]; // TODO: also bad
extern bool  bAppShowFPS;   // TODO: Also also bad
extern bool  bAlwaysProcessKeyboardInput;
extern TCHAR szAppBurnVer[16];
extern bool  bAppFullscreen;
extern bool bIntegerScale;
extern int  nWindowScale;
extern bool bAlwaysMenu;
extern int 	nGameSelect;
extern int 	nFilterSelect;
extern bool bShowAvailableOnly;
extern bool bShowClones;
extern int gameSelectedFromFilter;

#ifdef BUILD_SDL2
extern SDL_Window* sdlWindow;
extern int nJoystickCount;
#endif
extern TCHAR* GetIsoPath();

void MakeDirs(const char* pszPath);				// every directory of the path

struct DatSystem {
	const char* pszName;
	const char* pszFile;							// in szAppDatListsPath
	int nType;										// DAT_*_ONLY
};
extern const DatSystem DatSystems[];
extern const int nDatSystems;

TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize);
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize);

#define _TtoA(a)    TCHARToANSI(a, NULL, 0)
#define _AtoT(a)    ANSIToTCHAR(a, NULL, 0)


bool AppProcessKeyboardInput();

//config.cpp
int ConfigAppLoad();
int ConfigAppSave();

// drv.cpp
extern int  bDrvOkay; // 1 if the Driver has been initted okay, and it's okay to use the BurnDrv functions
extern char szAppRomPaths[DIRS_MAX][MAX_PATH];
int DrvInit(int nDrvNum, bool bRestore);
int DrvInitCallback(); // Used when Burn library needs to load a game. DrvInit(nBurnSelect, false)
int DrvExit();
int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs);
int AppError(TCHAR* szText, int bWarning);

#ifdef BUILD_SDL2_IMGUI
// ui/ui_popup.cpp
#define PUF_TYPE_ERROR			(1)
#define PUF_TYPE_WARNING		(2)
#define PUF_TYPE_INFO			(3)
#define PUF_TYPE_LOGONLY		(8)
#define PUF_TEXT_TRANSLATE		(1 << 16)
#define PUF_TEXT_NO_TRANSLATE	(0)
#define PUF_TEXT_DEFAULT		(PUF_TEXT_TRANSLATE)

int FBAPopupDisplay(int nFlags);
int FBAPopupAddText(int nFlags, TCHAR* pszFormat, ...);
int FBAPopupDestroyText();
#endif

//run.cpp
extern int RunMessageLoop();
extern int RunReset();

#define MESSAGE_MAX_FRAMES 180 // assuming 60fps this would be 3 seconds...
#define MESSAGE_MAX_LENGTH 255

extern UINT32 messageFrames;
extern char lastMessage[MESSAGE_MAX_LENGTH];

void UpdateMessage(char* message);
int StatedAuto(int bSave);

// media.cpp
int MediaInit();
int MediaExit();

//inpdipsw.cpp
#define DIP_MAX_NAME 64
#define MAXDIPSWITCHES 32
#define MAXDIPOPTIONS 32
bool setDIPSwitchOption(int dipgroup, int dipoption);
int InpDIPSWCreate();
void InpDIPSWResetDIPs();

struct GroupOfDIPSwitches
{
	BurnDIPInfo dipSwitch;
	UINT16 DefaultDIPOption;
	UINT16 SelectedDIPOption;
	char OptionsNamesWithCheckBoxes[MAXDIPOPTIONS][DIP_MAX_NAME];
	BurnDIPInfo dipSwitchesOptions[MAXDIPOPTIONS];
};

//interface/inp_interface.cpp
int InputInit();
int InputExit();
int InputMake(bool bCopy);

// stated.cpp
int QuickState(int bSave);
extern int bDrvSaveAll;
extern int nSavestateSlot;						// 1 to 8
const TCHAR* StatedSlotFile(int nSlot);
int StatedLoad(int nSlot);						// 0 if done, the BurnStateLoad() codes otherwise
int StatedSave(int nSlot);
int StatedUNDO(int nSlot);
int StatedLoadFile(const TCHAR* pszFile);
int StatedSaveFile(const TCHAR* pszFile);
int StatedFileGame(const TCHAR* pszFile, char* pszName);	// the zip name of the game of a state file
const TCHAR* StatedFolder();

// bzip.cpp
extern TCHAR szAppQuickPath[MAX_PATH];			// folder of the archive of a quick load

// memcard.cpp
extern TCHAR szMemoryCardFile[MAX_PATH];
extern int nMemoryCardStatus;					// & 1 = file selected, & 2 = inserted
bool MemCardSupported();
const TCHAR* MemCardFolder();
int MemCardCreate(const TCHAR* pszFile);
int MemCardSelect(const TCHAR* pszFile);
int MemCardInsert();
int MemCardEject();
int MemCardToggle();
void MemCardExit();
extern TCHAR szPgm2CardFile[4][MAX_PATH];
extern int nPgm2CardStatus[4];
int MemCardPgm2Slots();						// 0 if the game has no PGM2 card slots
int MemCardCreatePGM2Slot(int nSlot, const TCHAR* pszFile);
int MemCardSelectPGM2Slot(int nSlot, const TCHAR* pszFile);
int MemCardInsertPGM2Slot(int nSlot);
int MemCardEjectPGM2Slot(int nSlot);

// run.cpp
extern bool bAppDoStep;							// frame advance while paused
extern bool bAppDoRewind;
extern int nSlowMo;								// 0 = normal speed, 1 to 4 slower
extern int bAlwaysDrawFrames;

//stringset.cpp
class StringSet {
public:
	TCHAR* szText;
	int    nLen;
	// printf function to add text to the Bzip string
	int __cdecl Add(TCHAR* szFormat, ...);
	int Reset();

	StringSet();
	~StringSet();
};

// textfile.cpp
char* TextFileLoad(const char* pszFile);		// UTF-8 whatever the encoding, free() it; NULL if it can't be read
char* TextFileNextLine(char** ppszPos);			// the next line, cut in place without the line end; NULL at the end

// ips_manager.cpp
#define IPS_LANGUAGES 12
extern const char* IpsLanguageNames[IPS_LANGUAGES];
extern const char* IpsLanguageCodes[IPS_LANGUAGES];	// [en_US] ... sections of the dats
extern INT32 nIpsSelectedLanguage;
const char* IpsPatchFolder(const char* pszDrvName);	// szAppIpsPath<drv>/
INT32 LoadIpsActivePatches();					// the ones of config/ips/<drv>.ini, for nBurnDrvActive
INT32 IpsActivePatchCount();
const char* IpsActivePatch(INT32 i);			// the path of the dat
INT32 IpsSaveActivePatches(const char* pszDrvName, const char* pszFullName, const char* const* ppszDats, INT32 nCount);
char* IpsPatchDesc(const char* pszDat, INT32 nLang);	// the language, else English, else Chinese; UTF-8, free() it

// support_paths.cpp
extern TCHAR szAppListsPath[MAX_PATH];
extern TCHAR szAppDatListsPath[MAX_PATH];
extern TCHAR szAppArchivesPath[MAX_PATH];
