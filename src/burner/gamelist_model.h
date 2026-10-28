// Game list of the game selector: a cache of the driver data, the filters,
// the favorites and the parent/clone tree. No interface code in here.
//
// The cache is filled once, so the list can be filtered and drawn while
// another thread scans the roms (BzipOpen() changes nBurnDrvActive).
#pragma once

struct GameListEntry {
	const char* pszName;					// zip name
	const char* pszFullName;				// latin title, the alternative titles follow it after a '\0'
	const wchar_t* pszFullNameW;			// title in the original language, can be NULL
	const char* pszParent;					// the set it takes roms from, the parent for a clone
	const char* pszBoardRom;
	const char* pszSampleName;
	const char* pszManufacturer;
	const char* pszSystem;
	const char* pszDate;
	const char* pszComment;
	UINT32 nFlags;
	UINT32 nGenre;
	UINT32 nFamily;
	UINT32 nHardware;
	INT32 nMaxPlayers;
	INT32 nParent;							// driver number of the parent, -1 if it isn't a clone
	INT32 nWidth, nHeight;
	INT32 nAspectX, nAspectY;
	bool bWorking;
	bool bFavorite;
};

// Rom availability, the values of the Win32 selector
#define GAMELIST_AV_MISSING		0			// essential roms missing
#define GAMELIST_AV_OPTIONAL	1			// playable, optional roms missing or bad
#define GAMELIST_AV_OK			3

extern char* pGameListAv;					// one per driver

// The filter variables and bits are the ones of the Win32 selector (sel.cpp),
// the ini files can share them. A set bit hides the games that have it.
extern UINT64 nLoadMenuShowX;				// hardware, one bit per HARDWARE_PREFIX
extern int nLoadMenuShowY;					// options below
extern int nLoadMenuBoardTypeFilter;
extern int nLoadMenuGenreFilter;
extern int nLoadMenuFavoritesFilter;		// non-zero: favorites only
extern int nLoadMenuFamilyFilter;
extern int nLoadMenuExpand;					// open nodes of the filter tree

#define GAMELIST_UNAVAILABLE	(1 << 27)	// show the unavailable games
#define GAMELIST_AVAILABLE		(1 << 28)	// show the available games
#define GAMELIST_AUTOEXPAND		(1 << 29)	// always show the clones
#define GAMELIST_SHOWSHORT		(1 << 30)	// zip names
#define GAMELIST_ASCIIONLY		(1 << 31)	// latin titles only

#define GAMELIST_BOARD_GENUINE	(1)			// nLoadMenuBoardTypeFilter: the boards without any of the BDF_ below
#define GAMELIST_FAMILY_OTHER	0x10000000	// nLoadMenuFamilyFilter: the games without a family

// One line of the filter tree. The hardware groups (Capcom, Sega) come
// before their members, which have nDepth 1.
enum { GAMELIST_FILTER_BOARD, GAMELIST_FILTER_FAMILY, GAMELIST_FILTER_GENRE, GAMELIST_FILTER_HARDWARE };

struct GameListFilter {
	const char* pszName;
	int nGroup;
	int nDepth;
	UINT64 nMask;
};

extern const GameListFilter GameListFilters[];
extern const int nGameListFilters;

UINT64 GameListFilterValue(int nGroup);		// the filter variable of the group
void GameListFilterSet(int nGroup, UINT64 nMask, bool bShow);
UINT64 GameListFilterAll(int nGroup);		// all the bits of the group

// The visible list: the parents, each one with its clones that pass the
// filters. A parent that doesn't pass is still there when a clone does.
struct GameListNode {
	int nDrv;
	int nFirstClone;						// index in pnGameListClones
	int nCloneCount;
	bool bMatch;							// false: only there for its clones
};

extern GameListNode* pGameListNodes;
extern int nGameListNodes;
extern int* pnGameListClones;
extern int nGameListShown;					// parents and clones listed
extern int nGameListTotal;					// all the sets but the board roms
extern int nGameListMissing;				// all the unavailable sets

enum { GAMELIST_SORT_NAME, GAMELIST_SORT_ZIP, GAMELIST_SORT_YEAR, GAMELIST_SORT_MANUFACTURER, GAMELIST_SORT_SYSTEM };

int GameListInit();							// after BurnLibInit()
void GameListExit();
GameListEntry* GameListGet(int nDrv);
int GameListFind(const char* pszName);		// driver number from the zip name, -1 if none

// nCartsOnly: only the Neo Geo cartridges (picking the MVS slots)
void GameListBuild(const char* pszSearch, int nSort, bool bDescending, bool bCartsOnly);
bool GameListExpandedByDefault(const GameListNode* pNode);

// Favorites, in config/favorites.dat like the Win32 build
extern char szGameListFavorites[MAX_PATH];
void GameListFavoritesLoad();
void GameListFavoriteSet(int nDrv, bool bFavorite);

// The info panel of the selector, latin-1 like the driver texts
const char* GameListTitle(int nDrv, bool bAsciiOnly, char* pszBuffer, int nSize);	// UTF-8
void GameListInfoRom(int nDrv, char* pszText, int nSize);		// "sf2ce (Clone of sf2)"
void GameListInfoBoard(int nDrv, char* pszText, int nSize);		// bootleg, players, board roms
void GameListInfoRelease(int nDrv, char* pszText, int nSize);	// "Capcom (1992, CPS-1 Hardware)"
void GameListInfoTitles(int nDrv, char* pszText, int nSize);	// all the titles
void GameListInfoNotes(int nDrv, char* pszText, int nSize);
void GameListInfoGenre(int nDrv, char* pszText, int nSize);

const char* GameListMameName(const char* pszName);	// the set name used by the Arcade Database
const char* GameListHardwareIcon(int nDrv);			// "icon_md"... for the icons by hardware
