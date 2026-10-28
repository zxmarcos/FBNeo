// Game list of the game selector, see gamelist_model.h
#include "burner.h"
#include "gamelist_model.h"
#include <wchar.h>
#include <vector>
#include <algorithm>

static GameListEntry* pEntries = NULL;
static int* pnByName = NULL;				// drivers sorted by zip name, for GameListFind()
char* pGameListAv = NULL;

UINT64 nLoadMenuShowX = 0;
int nLoadMenuShowY = GAMELIST_AVAILABLE;
int nLoadMenuBoardTypeFilter = 0;
int nLoadMenuGenreFilter = 0;
int nLoadMenuFavoritesFilter = 0;
int nLoadMenuFamilyFilter = 0;
int nLoadMenuExpand = 0x10 | 0x01;			// the hardware node

GameListNode* pGameListNodes = NULL;
int nGameListNodes = 0;
int* pnGameListClones = NULL;
int nGameListShown = 0;
int nGameListTotal = 0;
int nGameListMissing = 0;

char szGameListFavorites[MAX_PATH] = "config/favorites.dat";
static std::vector<char*> UnknownFavorites;	// names of sets this build doesn't have, kept in the file

// ----------------------------------------------------------------------------
// Filters

#define HWBIT(x)		((UINT64)1 << (((UINT32)(x) >> 24) & 0x3f))

static UINT64 HardwareBit(UINT32 nHardware)
{
	return HWBIT(nHardware);
}

#define MASK_CAPCOM		(HWBIT(HARDWARE_PREFIX_CAPCOM) | HWBIT(HARDWARE_PREFIX_CPS2) | HWBIT(HARDWARE_PREFIX_CPS3) | HWBIT(HARDWARE_PREFIX_CAPCOM_MISC))
#define MASK_SEGA		(HWBIT(HARDWARE_PREFIX_SEGA_SG1000) | HWBIT(HARDWARE_PREFIX_SEGA_MASTER_SYSTEM) | HWBIT(HARDWARE_PREFIX_SEGA_MEGADRIVE) | HWBIT(HARDWARE_PREFIX_SEGA_GAME_GEAR) | HWBIT(HARDWARE_PREFIX_SEGA))

#define MASK_BOARD		(GAMELIST_BOARD_GENUINE | BDF_BOOTLEG | BDF_DEMO | BDF_HACK | BDF_HOMEBREW | BDF_PROTOTYPE)
#define MASK_FAMILY		(GAMELIST_FAMILY_OTHER | FBF_MSLUG | FBF_SF | FBF_KOF | FBF_DSTLK | FBF_FATFURY | FBF_SAMSHO | FBF_19XX | FBF_SONICWI | FBF_PWRINST | FBF_SONIC | FBF_DONPACHI | FBF_MAHOU)
#define MASK_GENRE		(GBF_HORSHOOT | GBF_VERSHOOT | GBF_SCRFIGHT | GBF_VSFIGHT | GBF_BIOS | GBF_BREAKOUT | GBF_CASINO | GBF_BALLPADDLE | GBF_MAZE | GBF_MINIGAMES | GBF_PINBALL | GBF_PLATFORM | GBF_PUZZLE | GBF_QUIZ | GBF_SPORTSMISC | GBF_SPORTSFOOTBALL | GBF_MISC | GBF_MAHJONG | GBF_RACING | GBF_SHOOT | GBF_MULTISHOOT | GBF_ACTION | GBF_RUNGUN | GBF_STRATEGY | GBF_RPG | GBF_SIM | GBF_ADV | GBF_CARD | GBF_BOARD)

// in the order of the Win32 filter tree
const GameListFilter GameListFilters[] = {
	{ "Genuine",							GAMELIST_FILTER_BOARD,		0, GAMELIST_BOARD_GENUINE },
	{ "Bootleg",							GAMELIST_FILTER_BOARD,		0, BDF_BOOTLEG },
	{ "Demo",								GAMELIST_FILTER_BOARD,		0, BDF_DEMO },
	{ "Hack",								GAMELIST_FILTER_BOARD,		0, BDF_HACK },
	{ "Homebrew",							GAMELIST_FILTER_BOARD,		0, BDF_HOMEBREW },
	{ "Prototype",							GAMELIST_FILTER_BOARD,		0, BDF_PROTOTYPE },

	{ "19XX",								GAMELIST_FILTER_FAMILY,		0, FBF_19XX },
	{ "DonPachi",							GAMELIST_FILTER_FAMILY,		0, FBF_DONPACHI },
	{ "Aero Fighters",						GAMELIST_FILTER_FAMILY,		0, FBF_SONICWI },
	{ "Darkstalkers",						GAMELIST_FILTER_FAMILY,		0, FBF_DSTLK },
	{ "Fatal Fury",							GAMELIST_FILTER_FAMILY,		0, FBF_FATFURY },
	{ "The King of Fighters",				GAMELIST_FILTER_FAMILY,		0, FBF_KOF },
	{ "Metal Slug",							GAMELIST_FILTER_FAMILY,		0, FBF_MSLUG },
	{ "Mahou Daisakusen",					GAMELIST_FILTER_FAMILY,		0, FBF_MAHOU },
	{ "Power Instinct",						GAMELIST_FILTER_FAMILY,		0, FBF_PWRINST },
	{ "Samurai Shodown",					GAMELIST_FILTER_FAMILY,		0, FBF_SAMSHO },
	{ "Sonic the Hedgehog",					GAMELIST_FILTER_FAMILY,		0, FBF_SONIC },
	{ "Street Fighter",						GAMELIST_FILTER_FAMILY,		0, FBF_SF },
	{ "Other",								GAMELIST_FILTER_FAMILY,		0, GAMELIST_FAMILY_OTHER },

	{ "Action (Classic)",					GAMELIST_FILTER_GENRE,		0, GBF_ACTION },
	{ "Ball & Paddle",						GAMELIST_FILTER_GENRE,		0, GBF_BALLPADDLE },
	{ "Breakout",							GAMELIST_FILTER_GENRE,		0, GBF_BREAKOUT },
	{ "Casino",								GAMELIST_FILTER_GENRE,		0, GBF_CASINO },
	{ "Fighting / Beat 'em Up",				GAMELIST_FILTER_GENRE,		0, GBF_SCRFIGHT },
	{ "Fighting / Versus",					GAMELIST_FILTER_GENRE,		0, GBF_VSFIGHT },
	{ "Mahjong",							GAMELIST_FILTER_GENRE,		0, GBF_MAHJONG },
	{ "Maze",								GAMELIST_FILTER_GENRE,		0, GBF_MAZE },
	{ "Mini-Games",							GAMELIST_FILTER_GENRE,		0, GBF_MINIGAMES },
	{ "Misc",								GAMELIST_FILTER_GENRE,		0, GBF_MISC },
	{ "Pinball",							GAMELIST_FILTER_GENRE,		0, GBF_PINBALL },
	{ "Platformer",							GAMELIST_FILTER_GENRE,		0, GBF_PLATFORM },
	{ "Puzzle",								GAMELIST_FILTER_GENRE,		0, GBF_PUZZLE },
	{ "Quiz",								GAMELIST_FILTER_GENRE,		0, GBF_QUIZ },
	{ "Racing",								GAMELIST_FILTER_GENRE,		0, GBF_RACING },
	{ "Run 'n Gun (Shooter)",				GAMELIST_FILTER_GENRE,		0, GBF_RUNGUN },
	{ "Shooter",							GAMELIST_FILTER_GENRE,		0, GBF_SHOOT },
	{ "Shooter / Multi-direction / Sh'mup",	GAMELIST_FILTER_GENRE,		0, GBF_MULTISHOOT },
	{ "Shooter / Horizontal / Sh'mup",		GAMELIST_FILTER_GENRE,		0, GBF_HORSHOOT },
	{ "Shooter / Vertical / Sh'mup",		GAMELIST_FILTER_GENRE,		0, GBF_VERSHOOT },
	{ "Sports",								GAMELIST_FILTER_GENRE,		0, GBF_SPORTSMISC },
	{ "Sports / Football",					GAMELIST_FILTER_GENRE,		0, GBF_SPORTSFOOTBALL },
	{ "Strategy",							GAMELIST_FILTER_GENRE,		0, GBF_STRATEGY },
	{ "RPG",								GAMELIST_FILTER_GENRE,		0, GBF_RPG },
	{ "Simulator",							GAMELIST_FILTER_GENRE,		0, GBF_SIM },
	{ "Adventure",							GAMELIST_FILTER_GENRE,		0, GBF_ADV },
	{ "Card Game",							GAMELIST_FILTER_GENRE,		0, GBF_CARD },
	{ "Board Game",							GAMELIST_FILTER_GENRE,		0, GBF_BOARD },
	{ "BIOS",								GAMELIST_FILTER_GENRE,		0, GBF_BIOS },

	{ "Capcom",								GAMELIST_FILTER_HARDWARE,	0, MASK_CAPCOM },
	{ "CPS-1",								GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_CAPCOM) },
	{ "CPS-2",								GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_CPS2) },
	{ "CPS-3",								GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_CPS3) },
	{ "Capcom (Other)",						GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_CAPCOM_MISC) },
	{ "Cave",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_CAVE) },
	{ "Data East",							GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_DATAEAST) },
	{ "Galaxian",							GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_GALAXIAN) },
	{ "Irem",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_IREM) },
	{ "Kaneko",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_KANEKO) },
	{ "Konami",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_KONAMI) },
	{ "Midway",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_MIDWAY) },
	{ "Neo Geo",							GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_SNK) },
	{ "Pacman",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_PACMAN) },
	{ "PGM",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_IGS_PGM) },
	{ "PGM2",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_IGS_PGM2) },
	{ "Psikyo",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_PSIKYO) },
	{ "Sega",								GAMELIST_FILTER_HARDWARE,	0, MASK_SEGA },
	{ "SG-1000",							GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_SEGA_SG1000) },
	{ "Master System",						GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_SEGA_MASTER_SYSTEM) },
	{ "Megadrive",							GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_SEGA_MEGADRIVE) },
	{ "Game Gear",							GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_SEGA_GAME_GEAR) },
	{ "Sega Arcade",						GAMELIST_FILTER_HARDWARE,	1, HWBIT(HARDWARE_PREFIX_SEGA) },
	{ "Seta",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_SETA) },
	{ "Taito",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_TAITO) },
	{ "Technos",							GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_TECHNOS) },
	{ "Toaplan",							GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_TOAPLAN) },
	{ "PC-Engine",							GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_PCENGINE) },
	{ "ColecoVision",						GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_COLECO) },
	{ "MSX",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_MSX) },
	{ "ZX Spectrum",						GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_SPECTRUM) },
	{ "NES",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_NES) },
	{ "FDS",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_FDS) },
	{ "SNES",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_SNES) },
	{ "GBA",								GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_GBA) },
	{ "Neo Geo Pocket",						GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_NGP) },
	{ "Fairchild Channel F",				GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_CHANNELF) },
	{ "Bally Astrocade",					GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_ASTROHOME) },
	{ "Misc (Pre-90s)",						GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_MISC_PRE90S) },
	{ "Misc (Post-90s)",					GAMELIST_FILTER_HARDWARE,	0, HWBIT(HARDWARE_PREFIX_MISC_POST90S) },
};

const int nGameListFilters = sizeof(GameListFilters) / sizeof(GameListFilters[0]);

UINT64 GameListFilterAll(int nGroup)
{
	switch (nGroup) {
		case GAMELIST_FILTER_BOARD:  return MASK_BOARD;
		case GAMELIST_FILTER_FAMILY: return MASK_FAMILY;
		case GAMELIST_FILTER_GENRE:  return MASK_GENRE;
	}

	UINT64 nMask = 0;
	for (int i = 0; i < nGameListFilters; i++) {
		if (GameListFilters[i].nGroup == GAMELIST_FILTER_HARDWARE) {
			nMask |= GameListFilters[i].nMask;
		}
	}
	return nMask;
}

UINT64 GameListFilterValue(int nGroup)
{
	switch (nGroup) {
		case GAMELIST_FILTER_BOARD:  return (UINT32)nLoadMenuBoardTypeFilter;
		case GAMELIST_FILTER_FAMILY: return (UINT32)nLoadMenuFamilyFilter;
		case GAMELIST_FILTER_GENRE:  return (UINT32)nLoadMenuGenreFilter;
	}
	return nLoadMenuShowX;
}

void GameListFilterSet(int nGroup, UINT64 nMask, bool bShow)
{
	UINT64 nValue = GameListFilterValue(nGroup);
	nValue = bShow ? (nValue & ~nMask) : (nValue | nMask);

	switch (nGroup) {
		case GAMELIST_FILTER_BOARD:  nLoadMenuBoardTypeFilter = (int)nValue; break;
		case GAMELIST_FILTER_FAMILY: nLoadMenuFamilyFilter = (int)nValue; break;
		case GAMELIST_FILTER_GENRE:  nLoadMenuGenreFilter = (int)nValue; break;
		default:                     nLoadMenuShowX = nValue; break;
	}
}

// The games with a set bit are hidden, as in DoExtraFilters() of the Win32 selector
static bool PassesExtraFilters(const GameListEntry* e)
{
	if (nLoadMenuFavoritesFilter) {
		return e->bFavorite;
	}

	UINT32 nBoard = e->nFlags & (BDF_BOOTLEG | BDF_DEMO | BDF_HACK | BDF_HOMEBREW | BDF_PROTOTYPE);
	if (nBoard & nLoadMenuBoardTypeFilter) {
		return false;
	}
	if (nBoard == 0 && (nLoadMenuBoardTypeFilter & GAMELIST_BOARD_GENUINE)) {
		return false;
	}

	UINT32 nFamily = e->nFamily & (MASK_FAMILY & ~GAMELIST_FAMILY_OTHER);
	if (nFamily & nLoadMenuFamilyFilter) {
		return false;
	}
	if (nFamily == 0 && (nLoadMenuFamilyFilter & GAMELIST_FAMILY_OTHER)) {
		return false;
	}

	// a game with several genres is shown while one of them is
	if (e->nGenre && (e->nGenre & ~(UINT32)nLoadMenuGenreFilter) == 0) {
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------------
// Search: every word must be found in one of the texts of the game

static bool ContainsNoCase(const char* pszText, const char* pszWord, int nWordLen)
{
	if (pszText == NULL) {
		return false;
	}
	for (; *pszText; pszText++) {
		int i = 0;
		while (i < nWordLen && pszText[i] && tolower((unsigned char)pszText[i]) == tolower((unsigned char)pszWord[i])) {
			i++;
		}
		if (i == nWordLen) {
			return true;
		}
	}
	return false;
}

static bool TitleContains(const GameListEntry* e, const char* pszWord, int nLen)
{
	// the alternative titles follow the first one
	for (const char* p = e->pszFullName; p && *p; p += strlen(p) + 1) {
		if (ContainsNoCase(p, pszWord, nLen)) {
			return true;
		}
	}
	return false;
}

static bool SearchMatch(const GameListEntry* e, const char* pszSearch)
{
	const char* p = pszSearch;
	while (*p) {
		while (*p == ' ') p++;
		const char* pWord = p;
		while (*p && *p != ' ') p++;
		int nLen = p - pWord;
		if (nLen == 0) {
			continue;
		}
		if (!TitleContains(e, pWord, nLen) && !ContainsNoCase(e->pszName, pWord, nLen) && !ContainsNoCase(e->pszManufacturer, pWord, nLen)
		 && !ContainsNoCase(e->pszSystem, pWord, nLen) && !ContainsNoCase(e->pszDate, pWord, nLen)) {
			return false;
		}
	}
	return true;
}

// ----------------------------------------------------------------------------

static int CompareNames(const void* a, const void* b)
{
	return strcmp(pEntries[*(const int*)a].pszName, pEntries[*(const int*)b].pszName);
}

int GameListFind(const char* pszName)
{
	if (pnByName == NULL || pszName == NULL) {
		return -1;
	}

	int nLow = 0, nHigh = nBurnDrvCount - 1;
	while (nLow <= nHigh) {
		int nMid = (nLow + nHigh) / 2;
		int nCmp = strcmp(pszName, pEntries[pnByName[nMid]].pszName);
		if (nCmp == 0) {
			return pnByName[nMid];
		}
		if (nCmp < 0) {
			nHigh = nMid - 1;
		} else {
			nLow = nMid + 1;
		}
	}
	return -1;
}

GameListEntry* GameListGet(int nDrv)
{
	if (pEntries == NULL || nDrv < 0 || (UINT32)nDrv >= nBurnDrvCount) {
		return NULL;
	}
	return &pEntries[nDrv];
}

int GameListInit()
{
	GameListExit();

	pEntries = (GameListEntry*)calloc(nBurnDrvCount, sizeof(GameListEntry));
	pnByName = (int*)malloc(nBurnDrvCount * sizeof(int));
	pGameListAv = (char*)calloc(nBurnDrvCount, 1);
	pGameListNodes = (GameListNode*)malloc(nBurnDrvCount * sizeof(GameListNode));
	pnGameListClones = (int*)malloc(nBurnDrvCount * sizeof(int));
	if (pEntries == NULL || pnByName == NULL || pGameListAv == NULL || pGameListNodes == NULL || pnGameListClones == NULL) {
		GameListExit();
		return 1;
	}

	UINT32 nOldDrv = nBurnDrvActive;
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		GameListEntry* e = &pEntries[i];
		nBurnDrvActive = i;

		// BurnDrvGetTextA() gives the strings of the driver itself, they don't change
		e->pszName = BurnDrvGetTextA(DRV_NAME);
		e->pszFullName = BurnDrvGetTextA(DRV_FULLNAME);
		e->pszFullNameW = BurnDrvGetFullNameW(i);
		e->pszParent = BurnDrvGetTextA(DRV_PARENT);
		e->pszBoardRom = BurnDrvGetTextA(DRV_BOARDROM);
		e->pszSampleName = BurnDrvGetTextA(DRV_SAMPLENAME);
		e->pszManufacturer = BurnDrvGetTextA(DRV_MANUFACTURER);
		e->pszSystem = BurnDrvGetTextA(DRV_SYSTEM);
		e->pszDate = BurnDrvGetTextA(DRV_DATE);
		e->pszComment = BurnDrvGetTextA(DRV_COMMENT);
		e->nFlags = BurnDrvGetFlags();
		e->nGenre = BurnDrvGetGenreFlags();
		e->nFamily = BurnDrvGetFamilyFlags();
		e->nHardware = BurnDrvGetHardwareCode();
		e->nMaxPlayers = BurnDrvGetMaxPlayers();
		e->bWorking = BurnDrvIsWorking();
		BurnDrvGetFullSize(&e->nWidth, &e->nHeight);
		BurnDrvGetAspect(&e->nAspectX, &e->nAspectY);
		e->nParent = -1;

		pnByName[i] = i;
	}
	nBurnDrvActive = nOldDrv;

	qsort(pnByName, nBurnDrvCount, sizeof(int), CompareNames);

	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		GameListEntry* e = &pEntries[i];
		if (e->pszParent && (e->nFlags & BDF_CLONE)) {
			e->nParent = GameListFind(e->pszParent);
		}
	}

	GameListFavoritesLoad();

	return 0;
}

void GameListExit()
{
	free(pEntries);
	pEntries = NULL;
	free(pnByName);
	pnByName = NULL;
	free(pGameListAv);
	pGameListAv = NULL;
	free(pGameListNodes);
	pGameListNodes = NULL;
	free(pnGameListClones);
	pnGameListClones = NULL;
	nGameListNodes = nGameListShown = 0;

	for (size_t i = 0; i < UnknownFavorites.size(); i++) {
		free(UnknownFavorites[i]);
	}
	UnknownFavorites.clear();
}

// ----------------------------------------------------------------------------
// The tree

static int nSortKey = GAMELIST_SORT_NAME;
static bool bSortDescending = false;

static int CompareText(const char* a, const char* b)
{
	return _stricmp(a ? a : "", b ? b : "");
}

static int CompareEntries(int a, int b)
{
	const GameListEntry* ea = &pEntries[a];
	const GameListEntry* eb = &pEntries[b];
	int nCmp = 0;

	switch (nSortKey) {
		case GAMELIST_SORT_YEAR:         nCmp = CompareText(ea->pszDate, eb->pszDate); break;
		case GAMELIST_SORT_MANUFACTURER: nCmp = CompareText(ea->pszManufacturer, eb->pszManufacturer); break;
		case GAMELIST_SORT_SYSTEM:       nCmp = CompareText(ea->pszSystem, eb->pszSystem); break;
		case GAMELIST_SORT_ZIP:          nCmp = strcmp(ea->pszName, eb->pszName); break;
	}
	if (nCmp == 0) {
		nCmp = (nSortKey == GAMELIST_SORT_ZIP) ? 0 : CompareText(ea->pszFullName, eb->pszFullName);
	}
	if (nCmp == 0) {
		nCmp = strcmp(ea->pszName, eb->pszName);
	}

	return bSortDescending ? -nCmp : nCmp;
}

void GameListBuild(const char* pszSearch, int nSort, bool bDescending, bool bCartsOnly)
{
	if (pEntries == NULL) {
		return;
	}

	nSortKey = nSort;
	bSortDescending = bDescending;

	UINT64 nAllHardware = GameListFilterAll(GAMELIST_FILTER_HARDWARE);
	bool bSearch = pszSearch && pszSearch[0];

	std::vector<char> Match(nBurnDrvCount, 0);
	nGameListTotal = 0;
	nGameListMissing = 0;

	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		const GameListEntry* e = &pEntries[i];
		if (e->nFlags & BDF_BOARDROM) {
			continue;
		}

		nGameListTotal++;
		if (pGameListAv[i] == GAMELIST_AV_MISSING) {
			nGameListMissing++;
		}

		UINT64 nHardware = HardwareBit(e->nHardware);
		if ((nHardware & nAllHardware) && (nHardware & nLoadMenuShowX)) {
			continue;
		}
		if (pGameListAv[i] == GAMELIST_AV_MISSING ? !(nLoadMenuShowY & GAMELIST_UNAVAILABLE) : !(nLoadMenuShowY & GAMELIST_AVAILABLE)) {
			continue;
		}
		if (bCartsOnly && (e->nHardware & HARDWARE_PREFIX_CARTRIDGE) != HARDWARE_PREFIX_CARTRIDGE) {
			continue;
		}
		if (!PassesExtraFilters(e)) {
			continue;
		}
		if (bSearch && !SearchMatch(e, pszSearch)) {
			continue;
		}
		Match[i] = 1;
	}

	// the parents, and the ones that are only there for a clone
	std::vector<int> NodeOf(nBurnDrvCount, -1);
	std::vector<int> Parents;
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		const GameListEntry* e = &pEntries[i];
		if (e->nFlags & BDF_BOARDROM) {
			continue;
		}
		int nParent = e->nParent;
		if (nParent < 0 || (pEntries[nParent].nFlags & BDF_BOARDROM)) {
			if (Match[i] && NodeOf[i] < 0) {
				NodeOf[i] = 0;
				Parents.push_back(i);
			}
		} else if (Match[i] && NodeOf[nParent] < 0) {
			NodeOf[nParent] = 0;
			Parents.push_back(nParent);
		}
	}

	std::sort(Parents.begin(), Parents.end(), [](int a, int b) { return CompareEntries(a, b) < 0; });

	nGameListNodes = Parents.size();
	for (int i = 0; i < nGameListNodes; i++) {
		GameListNode* n = &pGameListNodes[i];
		n->nDrv = Parents[i];
		n->bMatch = Match[Parents[i]] != 0;
		n->nFirstClone = 0;
		n->nCloneCount = 0;
		NodeOf[Parents[i]] = i;
	}

	// the clones, grouped by parent
	std::vector<int> Clones;
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		const GameListEntry* e = &pEntries[i];
		if (Match[i] && e->nParent >= 0 && !(pEntries[e->nParent].nFlags & BDF_BOARDROM) && !(e->nFlags & BDF_BOARDROM)) {
			Clones.push_back(i);
		}
	}
	std::sort(Clones.begin(), Clones.end(), [&NodeOf](int a, int b) {
		int na = NodeOf[pEntries[a].nParent], nb = NodeOf[pEntries[b].nParent];
		if (na != nb) {
			return na < nb;
		}
		return CompareEntries(a, b) < 0;
	});

	for (size_t i = 0; i < Clones.size(); i++) {
		GameListNode* n = &pGameListNodes[NodeOf[pEntries[Clones[i]].nParent]];
		if (n->nCloneCount == 0) {
			n->nFirstClone = i;
		}
		n->nCloneCount++;
		pnGameListClones[i] = Clones[i];
	}

	nGameListShown = nGameListNodes + Clones.size();
}

// The Win32 rule: the clones of an unavailable or broken parent are shown when one of them is available
bool GameListExpandedByDefault(const GameListNode* pNode)
{
	if (pNode->nCloneCount == 0) {
		return false;
	}
	if (!pNode->bMatch) {
		return true;
	}

	bool bAutoExpand = (nLoadMenuShowY & GAMELIST_AUTOEXPAND) != 0;
	if (!bAutoExpand && pGameListAv[pNode->nDrv] && pEntries[pNode->nDrv].bWorking) {
		return false;
	}
	for (int i = 0; i < pNode->nCloneCount; i++) {
		if (pGameListAv[pnGameListClones[pNode->nFirstClone + i]] || bAutoExpand) {
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------------
// Favorites

void GameListFavoritesLoad()
{
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		pEntries[i].bFavorite = false;
	}
	for (size_t i = 0; i < UnknownFavorites.size(); i++) {
		free(UnknownFavorites[i]);
	}
	UnknownFavorites.clear();

	FILE* fp = fopen(szGameListFavorites, "rb");
	if (fp == NULL) {
		return;
	}

	char szLine[256];
	while (fgets(szLine, sizeof(szLine), fp)) {
		int nLen = strlen(szLine);
		while (nLen > 0 && (szLine[nLen - 1] == '\n' || szLine[nLen - 1] == '\r')) {
			szLine[--nLen] = '\0';
		}
		if (nLen < 2 || nLen > 64) {
			continue;
		}
		int nDrv = GameListFind(szLine);
		if (nDrv >= 0) {
			pEntries[nDrv].bFavorite = true;
		} else {
			UnknownFavorites.push_back(strdup(szLine));
		}
	}
	fclose(fp);
}

static void FavoritesSave()
{
	FILE* fp = fopen(szGameListFavorites, "wb");
	if (fp == NULL) {
		return;
	}
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		if (pEntries[i].bFavorite) {
			fprintf(fp, "%s\n", pEntries[i].pszName);
		}
	}
	for (size_t i = 0; i < UnknownFavorites.size(); i++) {
		fprintf(fp, "%s\n", UnknownFavorites[i]);
	}
	fclose(fp);
}

void GameListFavoriteSet(int nDrv, bool bFavorite)
{
	GameListEntry* e = GameListGet(nDrv);
	if (e == NULL || e->bFavorite == bFavorite) {
		return;
	}
	e->bFavorite = bFavorite;
	FavoritesSave();
}

// ----------------------------------------------------------------------------
// Texts

static bool IsUtf8(const unsigned char* s)
{
	while (*s) {
		int n = 0;
		if (*s < 0x80) n = 0;
		else if ((*s & 0xE0) == 0xC0) n = 1;
		else if ((*s & 0xF0) == 0xE0) n = 2;
		else if ((*s & 0xF8) == 0xF0) n = 3;
		else return false;
		s++;
		while (n--) {
			if ((*s & 0xC0) != 0x80) {
				return false;
			}
			s++;
		}
	}
	return true;
}

static int PutUtf8(char* pszOut, int nSize, int n, UINT32 c)
{
	if (c < 0x80) {
		if (n + 1 < nSize) pszOut[n++] = c;
	} else if (c < 0x800) {
		if (n + 2 < nSize) { pszOut[n++] = 0xC0 | (c >> 6); pszOut[n++] = 0x80 | (c & 0x3F); }
	} else if (c < 0x10000) {
		if (n + 3 < nSize) { pszOut[n++] = 0xE0 | (c >> 12); pszOut[n++] = 0x80 | ((c >> 6) & 0x3F); pszOut[n++] = 0x80 | (c & 0x3F); }
	} else {
		if (n + 4 < nSize) { pszOut[n++] = 0xF0 | (c >> 18); pszOut[n++] = 0x80 | ((c >> 12) & 0x3F); pszOut[n++] = 0x80 | ((c >> 6) & 0x3F); pszOut[n++] = 0x80 | (c & 0x3F); }
	}
	return n;
}

const char* GameListTitle(int nDrv, bool bAsciiOnly, char* pszBuffer, int nSize)
{
	const GameListEntry* e = GameListGet(nDrv);
	int n = 0;

	if (e == NULL || nSize < 1) {
		return "";
	}

	// all the titles, the alternative ones follow the first after a '\0'
	static const UINT32 Separator[] = { ' ', 0x25E6, ' ' };
	if (!bAsciiOnly && e->pszFullNameW && e->pszFullNameW[0]) {
		for (const wchar_t* p = e->pszFullNameW; *p; p += wcslen(p) + 1) {
			if (p != e->pszFullNameW) {
				for (int i = 0; i < 3; i++) n = PutUtf8(pszBuffer, nSize, n, Separator[i]);
			}
			for (const wchar_t* s = p; *s; s++) {
				UINT32 c = *s;
				if (c >= 0xD800 && c < 0xDC00 && s[1] >= 0xDC00 && s[1] < 0xE000) {	// UTF-16 surrogates (Windows)
					c = 0x10000 + ((c - 0xD800) << 10) + (s[1] - 0xDC00);
					s++;
				}
				n = PutUtf8(pszBuffer, nSize, n, c);
			}
		}
	} else {
		for (const char* p = e->pszFullName; *p; p += strlen(p) + 1) {
			if (p != e->pszFullName) {
				for (int i = 0; i < 3; i++) n = PutUtf8(pszBuffer, nSize, n, Separator[i]);
			}
			if (IsUtf8((const unsigned char*)p)) {
				for (const char* s = p; *s && n + 1 < nSize; s++) pszBuffer[n++] = *s;
			} else {
				for (const unsigned char* s = (const unsigned char*)p; *s; s++) {	// latin-1
					n = PutUtf8(pszBuffer, nSize, n, *s);
				}
			}
		}
	}
	pszBuffer[n] = '\0';

	return pszBuffer;
}

// the texts of the six lines of the Win32 info panel

void GameListInfoRom(int nDrv, char* pszText, int nSize)
{
	const GameListEntry* e = GameListGet(nDrv);
	if (e == NULL) {
		pszText[0] = '\0';
		return;
	}

	int n = snprintf(pszText, nSize, "%s", e->pszName);
	bool bBracket = false;

	if (e->nParent >= 0) {
		n += snprintf(pszText + n, nSize - n, " (Clone of %s", e->pszParent);
		if (pEntries[e->nParent].pszParent) {
			n += snprintf(pszText + n, nSize - n, ", uses ROMs from %s", pEntries[e->nParent].pszParent);
		}
		bBracket = true;
	} else if (e->pszParent) {
		n += snprintf(pszText + n, nSize - n, " (uses ROMs from %s", e->pszParent);
		bBracket = true;
	}
	if (e->pszSampleName && n < nSize) {
		n += snprintf(pszText + n, nSize - n, "%suses samples from %s", bBracket ? ", " : " (", e->pszSampleName);
		bBracket = true;
	}
	if (bBracket && n < nSize) {
		snprintf(pszText + n, nSize - n, ")");
	}
}

void GameListInfoBoard(int nDrv, char* pszText, int nSize)
{
	const GameListEntry* e = GameListGet(nDrv);
	if (e == NULL) {
		pszText[0] = '\0';
		return;
	}

	static const struct { UINT32 nFlag; const char* pszName; } Boards[] = {
		{ BDF_PROTOTYPE, "Prototype" }, { BDF_BOOTLEG, "Bootleg" }, { BDF_HACK, "Hack" }, { BDF_HOMEBREW, "Homebrew" }, { BDF_DEMO, "Demo" },
	};

	int n = 0;
	pszText[0] = '\0';
	for (unsigned int i = 0; i < sizeof(Boards) / sizeof(Boards[0]); i++) {
		if ((e->nFlags & Boards[i].nFlag) && n < nSize) {
			n += snprintf(pszText + n, nSize - n, "%s%s", n ? ", " : "", Boards[i].pszName);
		}
	}
	if (n < nSize) {
		n += snprintf(pszText + n, nSize - n, "%s%d Player%s", n ? ", " : "", e->nMaxPlayers, e->nMaxPlayers != 1 ? "s Max" : "");
	}
	if (e->pszBoardRom && n < nSize) {
		snprintf(pszText + n, nSize - n, ", uses Board-ROMs from %s", e->pszBoardRom);
	}
}

void GameListInfoRelease(int nDrv, char* pszText, int nSize)
{
	const GameListEntry* e = GameListGet(nDrv);
	if (e == NULL) {
		pszText[0] = '\0';
		return;
	}

	bool bMvsCart = ((e->nHardware & HARDWARE_SNK_MVS) == HARDWARE_SNK_MVS) && ((e->nHardware & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOGEO);
	snprintf(pszText, nSize, "%s (%s, %s Hardware)", e->pszManufacturer ? e->pszManufacturer : "Unknown", e->pszDate ? e->pszDate : "",
		bMvsCart ? "Neo Geo MVS Cartridge" : (e->pszSystem ? e->pszSystem : ""));
}

void GameListInfoTitles(int nDrv, char* pszText, int nSize)
{
	const GameListEntry* e = GameListGet(nDrv);
	pszText[0] = '\0';
	if (e == NULL || e->pszFullName == NULL) {
		return;
	}

	int n = 0;
	for (const char* p = e->pszFullName; *p && n < nSize; p += strlen(p) + 1) {
		n += snprintf(pszText + n, nSize - n, "%s%s", n ? SEPERATOR_2 : "", p);
	}
}

void GameListInfoNotes(int nDrv, char* pszText, int nSize)
{
	const GameListEntry* e = GameListGet(nDrv);
	if (e == NULL) {
		pszText[0] = '\0';
		return;
	}

	int n = snprintf(pszText, nSize, "%s", e->pszComment ? e->pszComment : "");
	if ((e->nFlags & BDF_HISCORE_SUPPORTED) && n < nSize) {
		snprintf(pszText + n, nSize - n, "%sHigh Scores Supported", n ? ", " : "");
	}
}

void GameListInfoGenre(int nDrv, char* pszText, int nSize)
{
	const GameListEntry* e = GameListGet(nDrv);
	pszText[0] = '\0';
	if (e == NULL) {
		return;
	}

	int n = 0;
	for (int i = 0; i < nGameListFilters && n < nSize; i++) {
		const GameListFilter* f = &GameListFilters[i];
		if (f->nGroup == GAMELIST_FILTER_GENRE && (e->nGenre & f->nMask)) {
			n += snprintf(pszText + n, nSize - n, "%s%s", n ? ", " : "", f->pszName);
		}
	}

	int nFamily = 0;
	for (int i = 0; i < nGameListFilters && n < nSize; i++) {
		const GameListFilter* f = &GameListFilters[i];
		if (f->nGroup == GAMELIST_FILTER_FAMILY && f->nMask != GAMELIST_FAMILY_OTHER && (e->nFamily & f->nMask)) {
			n += snprintf(pszText + n, nSize - n, "%s%s", nFamily++ ? ", " : (n ? " (" : "("), f->pszName);
		}
	}
	if (nFamily && n < nSize) {
		snprintf(pszText + n, nSize - n, ")");
	}
}

// Some of our sets have other names in MAME, the Arcade Database uses those
// (the same table as the Win32 selector and burn/hiscore.cpp)
const char* GameListMameName(const char* pszName)
{
	static const char* Names[][2] = {
		{ "vsraidbbay", "bnglngby" }, { "vsrbibbal", "rbibb" }, { "vsbattlecity", "btlecity" }, { "vscastlevania", "cstlevna" },
		{ "vsclucluland", "cluclu" }, { "vsdrmario", "drmario" }, { "vsduckhunt", "duckhunt" }, { "vsexcitebike", "excitebk" },
		{ "vsfreedomforce", "vsfdf" }, { "vsgoonies", "goonies" }, { "vsgradius", "vsgradus" }, { "vsgumshoe", "vsgshoe" },
		{ "vshogansalley", "hogalley" }, { "vsiceclimber", "iceclimb" }, { "vsmachrider", "nvs_machrider" }, { "vsmightybomjack", "nvs_mightybj" },
		{ "vsninjajkun", "jajamaru" }, { "vspinball", "vspinbal" }, { "vsplatoon", "nvs_platoon" }, { "vsslalom", "vsslalom" },
		{ "vssoccer", "vssoccer" }, { "vsstarluster", "starlstr" }, { "vssmgolf", "smgolf" }, { "vssmgolfla", "ladygolf" },
		{ "vssmb", "suprmrio" }, { "vssuperskykid", "vsskykid" }, { "vssuperxevious", "supxevs" }, { "vstetris", "vstetris" },
		{ "vstkoboxing", "tkoboxng" }, { "vstopgun", "topgun" },
	};

	for (unsigned int i = 0; i < sizeof(Names) / sizeof(Names[0]); i++) {
		if (strcmp(Names[i][0], pszName) == 0) {
			return Names[i][1];
		}
	}
	return pszName;
}

const char* GameListHardwareIcon(int nDrv)
{
	const GameListEntry* e = GameListGet(nDrv);
	if (e == NULL) {
		return "icon_arc";
	}

	static const struct { UINT32 nHardware; const char* pszIcon; } Icons[] = {
		{ HARDWARE_SEGA_MEGADRIVE, "icon_md" }, { HARDWARE_PCENGINE_PCENGINE, "icon_pce" }, { HARDWARE_PCENGINE_TG16, "icon_tg" },
		{ HARDWARE_PCENGINE_SGX, "icon_sgx" }, { HARDWARE_SEGA_SG1000, "icon_sg1k" }, { HARDWARE_COLECO, "icon_cv" },
		{ HARDWARE_SEGA_MASTER_SYSTEM, "icon_sms" }, { HARDWARE_SEGA_GAME_GEAR, "icon_gg" }, { HARDWARE_MSX, "icon_msx" },
		{ HARDWARE_SPECTRUM, "icon_spec" }, { HARDWARE_NES, "icon_nes" }, { HARDWARE_FDS, "icon_fds" }, { HARDWARE_SNES, "icon_snes" },
		{ HARDWARE_GBA, "icon_gba" },
	};

	UINT32 nCode = e->nHardware & HARDWARE_PUBLIC_MASK;
	for (unsigned int i = 0; i < sizeof(Icons) / sizeof(Icons[0]); i++) {
		if (nCode == (Icons[i].nHardware & HARDWARE_PUBLIC_MASK)) {
			return Icons[i].pszIcon;
		}
	}
	if ((e->nHardware & HARDWARE_SNK_NGPC) == HARDWARE_SNK_NGPC) return "icon_ngpc";
	if (nCode == (HARDWARE_SNK_NGP & HARDWARE_PUBLIC_MASK)) return "icon_ngp";
	if (nCode == (HARDWARE_CHANNELF & HARDWARE_PUBLIC_MASK)) return "icon_chf";
	if (nCode == (HARDWARE_ASTROHOME & HARDWARE_PUBLIC_MASK)) return "icon_astro";

	return "icon_arc";
}
