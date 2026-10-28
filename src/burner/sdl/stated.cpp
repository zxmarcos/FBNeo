// Save states: the quick state of the old menu, the slots 1 to 8 (with the
// backups and undo of state.cpp) and the states in files
#include "burner.h"
#include "cdlist.h"
#ifdef _WIN32
#include <direct.h>
#endif

int nSavestateSlot = 1;

// The automatic save
int QuickState(int bSave)
{
	static TCHAR szName[MAX_PATH] = _T("");
	int nRet;

#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	_stprintf(szName, _T("%squick_%s.fs"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
#else
	_stprintf(szName, _T("config/games/quick_%s.fs"), BurnDrvGetText(DRV_NAME));
#endif

	if (bSave == 0)
	{
		nRet = BurnStateLoad(szName, 1, NULL);		// Load ram
		UpdateMessage("Quicksave: State Loaded");
	}
	else
	{
		nRet = BurnStateSave(szName, 1);				// Save ram
		UpdateMessage("Quicksave: State Saved");
	}

	return nRet;
}

const TCHAR* StatedFolder()
{
	static TCHAR szFolder[MAX_PATH] = _T("");

	if (szFolder[0] == _T('\0')) {
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
		char* pszPath = SDL_GetPrefPath("fbneo", "savestates");
		_stprintf(szFolder, _T("%s"), pszPath);
		SDL_free(pszPath);
#else
		_stprintf(szFolder, _T("savestates/"));
#ifdef _WIN32
		_mkdir("savestates");
#endif
#endif
	}

	return szFolder;
}

// "<game> slot NN.fs", as the Win32 build (the backups only apply to these names)
const TCHAR* StatedSlotFile(int nSlot)
{
	static TCHAR szName[MAX_PATH];
	_stprintf(szName, _T("%s%s slot %02x.fs"), StatedFolder(), IsCDGame() ? CDInfo_GamePrefix() : BurnDrvGetText(DRV_NAME), nSlot);

	return szName;
}

// The zip name of the game a state was saved from, see BurnStateLoadEmbed()
int StatedFileGame(const TCHAR* pszFile, char* pszName)
{
	char szHeader[8];
	pszName[0] = '\0';

	FILE* fp = _tfopen(pszFile, _T("rb"));
	if (fp == NULL) {
		return 1;
	}

	int nRet = 1;
	if (fread(szHeader, 1, 8, fp) == 8 && memcmp(szHeader, "FB1 FS1 ", 8) == 0) {
		fseek(fp, 4 * 7, SEEK_SET);				// chunk size, versions and compressed size
		if (fread(pszName, 1, 32, fp) == 32) {
			pszName[32] = '\0';
			nRet = 0;
		}
	}
	fclose(fp);

	return nRet;
}

// Only the states of the running game are loaded here: BurnStateLoad() would
// exit the game first and load the other one by itself
static int LoadState(const TCHAR* pszFile)
{
	char szName[33];

	if (!bDrvOkay) {
		return 1;
	}
	if (StatedFileGame(pszFile, szName)) {
		return 2;								// no file, or not a state
	}
	if (strcmp(szName, BurnDrvGetTextA(DRV_NAME)) != 0) {
		return 3;
	}

	return BurnStateLoad((TCHAR*)pszFile, 1, NULL);
}

int StatedLoad(int nSlot)
{
	char szText[64];

	int nRet = LoadState(StatedSlotFile(nSlot));
	if (nRet == 0) {
		snprintf(szText, sizeof(szText), "State %d loaded", nSlot);
	} else {
		snprintf(szText, sizeof(szText), "State %d: %s", nSlot, nRet == 2 ? "nothing saved" : "can't be loaded");
	}
	UpdateMessage(szText);

	return nRet;
}

int StatedSave(int nSlot)
{
	char szText[64];

	if (!bDrvOkay) {
		return 1;
	}

	int nRet = BurnStateSave((TCHAR*)StatedSlotFile(nSlot), 1);
	snprintf(szText, sizeof(szText), nRet ? "State %d: can't be saved" : "State %d saved", nSlot);
	UpdateMessage(szText);

	return nRet;
}

// Puts back the state the last save of the slot replaced
int StatedUNDO(int nSlot)
{
	char szText[64];

	if (!bDrvOkay) {
		return 1;
	}

	int nRet = BurnStateUNDO((TCHAR*)StatedSlotFile(nSlot));
	snprintf(szText, sizeof(szText), nRet ? "State %d: nothing to undo" : "State %d: last save undone", nSlot);
	UpdateMessage(szText);

	return nRet;
}

int StatedLoadFile(const TCHAR* pszFile)
{
	int nRet = LoadState(pszFile);
	if (nRet == 0) {
		UpdateMessage("State loaded");
	}

	return nRet;
}

int StatedSaveFile(const TCHAR* pszFile)
{
	if (!bDrvOkay) {
		return 1;
	}

	int nRet = BurnStateSave((TCHAR*)pszFile, 1);
	if (nRet == 0) {
		UpdateMessage("State saved");
	}

	return nRet;
}
