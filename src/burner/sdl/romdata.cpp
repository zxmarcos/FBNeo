// RomData: a dat (ZipName, DrvName, FullName and the roms) gives another romset
// to a driver, a hack. The engine of win32/romdata.cpp, the manager is in
// ui/ui_romdata.cpp.
#include "burner.h"

// the rom types of the drivers, the dats may use their names
#define CPS1_68K_PROGRAM_BYTESWAP			1
#define CPS1_68K_PROGRAM_NO_BYTESWAP		2
#define CPS1_Z80_PROGRAM					3
#define CPS1_TILES							4
#define CPS1_OKIM6295_SAMPLES				5
#define CPS1_QSOUND_SAMPLES					6
#define CPS1_PIC							7
#define CPS1_EXTRA_TILES_SF2EBBL_400000		8
#define CPS1_EXTRA_TILES_400000				9
#define CPS1_EXTRA_TILES_SF2KORYU_400000	10
#define CPS1_EXTRA_TILES_SF2B_400000		11
#define CPS1_EXTRA_TILES_SF2MKOT_400000		12

// d_cps2.cpp
#define CPS2_PRG_68K						1
#define CPS2_PRG_68K_SIMM					2
#define CPS2_PRG_68K_XOR_TABLE				3
#define CPS2_GFX							5
#define CPS2_GFX_SIMM						6
#define CPS2_GFX_SPLIT4						7
#define CPS2_GFX_SPLIT8						8
#define CPS2_GFX_19XXJ						9
#define CPS2_PRG_Z80						10
#define CPS2_QSND							12
#define CPS2_QSND_SIMM						13
#define CPS2_QSND_SIMM_BYTESWAP				14
#define CPS2_ENCRYPTION_KEY					15

// gal.h
#define GAL_ROM_Z80_PROG1				1
#define GAL_ROM_Z80_PROG2				2
#define GAL_ROM_Z80_PROG3				3
#define GAL_ROM_TILES_SHARED			4
#define GAL_ROM_TILES_CHARS				5
#define GAL_ROM_TILES_SPRITES			6
#define GAL_ROM_PROM					7
#define GAL_ROM_S2650_PROG1				8

// megadrive.h
#define SEGA_MD_ROM_LOAD_NORMAL								0x10
#define SEGA_MD_ROM_LOAD16_WORD_SWAP						0x20
#define SEGA_MD_ROM_LOAD16_BYTE								0x30
#define SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000	0x40
#define SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000		0x50
#define SEGA_MD_ROM_OFFS_000000								0x01
#define SEGA_MD_ROM_OFFS_000001								0x02
#define SEGA_MD_ROM_OFFS_020000								0x03
#define SEGA_MD_ROM_OFFS_080000								0x04
#define SEGA_MD_ROM_OFFS_100000								0x05
#define SEGA_MD_ROM_OFFS_100001								0x06
#define SEGA_MD_ROM_OFFS_200000								0x07
#define SEGA_MD_ROM_OFFS_300000								0x08
#define SEGA_MD_ROM_RELOAD_200000_200000					0x09
#define SEGA_MD_ROM_RELOAD_100000_300000					0x0a
#define SEGA_MD_ARCADE_SUNMIXING							(0x4000)

// sys16.h
#define SYS16_ROM_PROG_FLAT									25
#define SYS16_ROM_PROG										1
#define SYS16_ROM_TILES										2
#define SYS16_ROM_SPRITES									3
#define SYS16_ROM_Z80PROG									4
#define SYS16_ROM_KEY										5
#define SYS16_ROM_7751PROG									6
#define SYS16_ROM_7751DATA									7
#define SYS16_ROM_UPD7759DATA								8
#define SYS16_ROM_PROG2										9
#define SYS16_ROM_ROAD										10
#define SYS16_ROM_PCMDATA									11
#define SYS16_ROM_Z80PROG2									12
#define SYS16_ROM_Z80PROG3									13
#define SYS16_ROM_Z80PROG4									14
#define SYS16_ROM_PCM2DATA									15
#define SYS16_ROM_PROM 										16
#define SYS16_ROM_PROG3										17
#define SYS16_ROM_SPRITES2									18
#define SYS16_ROM_RF5C68DATA								19
#define SYS16_ROM_I8751										20
#define SYS16_ROM_MSM6295									21
#define SYS16_ROM_TILES_20000								22

// taito.h
#define TAITO_68KROM1										1
#define TAITO_68KROM1_BYTESWAP								2
#define TAITO_68KROM1_BYTESWAP_JUMPING						3
#define TAITO_68KROM1_BYTESWAP32							4
#define TAITO_68KROM2										5
#define TAITO_68KROM2_BYTESWAP								6
#define TAITO_68KROM3										7
#define TAITO_68KROM3_BYTESWAP								8
#define TAITO_Z80ROM1										9
#define TAITO_Z80ROM2										10
#define TAITO_CHARS											11
#define TAITO_CHARS_BYTESWAP								12
#define TAITO_CHARSB										13
#define TAITO_CHARSB_BYTESWAP								14
#define TAITO_SPRITESA										15
#define TAITO_SPRITESA_BYTESWAP								16
#define TAITO_SPRITESA_BYTESWAP32							17
#define TAITO_SPRITESA_TOPSPEED								18
#define TAITO_SPRITESB										19
#define TAITO_SPRITESB_BYTESWAP								20
#define TAITO_SPRITESB_BYTESWAP32							21
#define TAITO_ROAD											22
#define TAITO_SPRITEMAP										23
#define TAITO_YM2610A										24
#define TAITO_YM2610B										25
#define TAITO_MSM5205										26
#define TAITO_MSM5205_BYTESWAP								27
#define TAITO_CHARS_PIVOT									28
#define TAITO_MSM6295										29
#define TAITO_ES5505										30
#define TAITO_ES5505_BYTESWAP								31
#define TAITO_DEFAULT_EEPROM								32
#define TAITO_CHARS_BYTESWAP32								33
#define TAITO_CCHIP_BIOS									34
#define TAITO_CCHIP_EEPROM									35

struct RDMacroMap {
	const TCHAR* pszDRMacro;
	const UINT32 nRDMacro;
};
#define X(a) { _T(#a), a }
static const RDMacroMap RDMacroTable[] = {
	X(BRF_PRG),
	X(BRF_GRA),
	X(BRF_SND),
	X(BRF_ESS),
	X(BRF_BIOS),
	X(BRF_SELECT),
	X(BRF_OPT),
	X(BRF_NODUMP),
	X(CPS1_68K_PROGRAM_BYTESWAP),
	X(CPS1_68K_PROGRAM_NO_BYTESWAP),
	X(CPS1_Z80_PROGRAM),
	X(CPS1_TILES),
	X(CPS1_OKIM6295_SAMPLES),
	X(CPS1_QSOUND_SAMPLES),
	X(CPS1_PIC),
	X(CPS1_EXTRA_TILES_SF2EBBL_400000),
	X(CPS1_EXTRA_TILES_400000),
	X(CPS1_EXTRA_TILES_SF2KORYU_400000),
	X(CPS1_EXTRA_TILES_SF2B_400000),
	X(CPS1_EXTRA_TILES_SF2MKOT_400000),
	X(CPS2_PRG_68K),
	X(CPS2_PRG_68K_SIMM),
	X(CPS2_PRG_68K_XOR_TABLE),
	X(CPS2_GFX),
	X(CPS2_GFX_SIMM),
	X(CPS2_GFX_SPLIT4),
	X(CPS2_GFX_SPLIT8),
	X(CPS2_GFX_19XXJ),
	X(CPS2_PRG_Z80),
	X(CPS2_QSND),
	X(CPS2_QSND_SIMM),
	X(CPS2_QSND_SIMM_BYTESWAP),
	X(CPS2_ENCRYPTION_KEY),
	X(GAL_ROM_Z80_PROG1),
	X(GAL_ROM_Z80_PROG2),
	X(GAL_ROM_Z80_PROG3),
	X(GAL_ROM_TILES_SHARED),
	X(GAL_ROM_TILES_CHARS),
	X(GAL_ROM_TILES_SPRITES),
	X(GAL_ROM_PROM),
	X(GAL_ROM_S2650_PROG1),
	X(SEGA_MD_ROM_LOAD_NORMAL),
	X(SEGA_MD_ROM_LOAD16_WORD_SWAP),
	X(SEGA_MD_ROM_LOAD16_BYTE),
	X(SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000),
	X(SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000),
	X(SEGA_MD_ROM_OFFS_000000),
	X(SEGA_MD_ROM_OFFS_000001),
	X(SEGA_MD_ROM_OFFS_020000),
	X(SEGA_MD_ROM_OFFS_080000),
	X(SEGA_MD_ROM_OFFS_100000),
	X(SEGA_MD_ROM_OFFS_100001),
	X(SEGA_MD_ROM_OFFS_200000),
	X(SEGA_MD_ROM_OFFS_300000),
	X(SEGA_MD_ROM_RELOAD_200000_200000),
	X(SEGA_MD_ROM_RELOAD_100000_300000),
	X(SEGA_MD_ARCADE_SUNMIXING),
	X(SYS16_ROM_PROG_FLAT),
	X(SYS16_ROM_PROG),
	X(SYS16_ROM_TILES),
	X(SYS16_ROM_SPRITES),
	X(SYS16_ROM_Z80PROG),
	X(SYS16_ROM_KEY),
	X(SYS16_ROM_7751PROG),
	X(SYS16_ROM_7751DATA),
	X(SYS16_ROM_UPD7759DATA),
	X(SYS16_ROM_PROG2),
	X(SYS16_ROM_ROAD),
	X(SYS16_ROM_PCMDATA),
	X(SYS16_ROM_Z80PROG2),
	X(SYS16_ROM_Z80PROG3),
	X(SYS16_ROM_Z80PROG4),
	X(SYS16_ROM_PCM2DATA),
	X(SYS16_ROM_PROM),
	X(SYS16_ROM_PROG3),
	X(SYS16_ROM_SPRITES2),
	X(SYS16_ROM_RF5C68DATA),
	X(SYS16_ROM_I8751),
	X(SYS16_ROM_MSM6295),
	X(SYS16_ROM_TILES_20000),
	X(TAITO_68KROM1),
	X(TAITO_68KROM1_BYTESWAP),
	X(TAITO_68KROM1_BYTESWAP_JUMPING),
	X(TAITO_68KROM1_BYTESWAP32),
	X(TAITO_68KROM2),
	X(TAITO_68KROM2_BYTESWAP),
	X(TAITO_68KROM3),
	X(TAITO_68KROM3_BYTESWAP),
	X(TAITO_Z80ROM1),
	X(TAITO_Z80ROM2),
	X(TAITO_CHARS),
	X(TAITO_CHARS_BYTESWAP),
	X(TAITO_CHARSB),
	X(TAITO_CHARSB_BYTESWAP),
	X(TAITO_SPRITESA),
	X(TAITO_SPRITESA_BYTESWAP),
	X(TAITO_SPRITESA_BYTESWAP32),
	X(TAITO_SPRITESA_TOPSPEED),
	X(TAITO_SPRITESB),
	X(TAITO_SPRITESB_BYTESWAP),
	X(TAITO_SPRITESB_BYTESWAP32),
	X(TAITO_ROAD),
	X(TAITO_SPRITEMAP),
	X(TAITO_YM2610A),
	X(TAITO_YM2610B),
	X(TAITO_MSM5205),
	X(TAITO_MSM5205_BYTESWAP),
	X(TAITO_CHARS_PIVOT),
	X(TAITO_MSM6295),
	X(TAITO_ES5505),
	X(TAITO_ES5505_BYTESWAP),
	X(TAITO_DEFAULT_EEPROM),
	X(TAITO_CHARS_BYTESWAP32),
	X(TAITO_CCHIP_BIOS),
	X(TAITO_CCHIP_EEPROM)
};
#undef X

// gal.h
#undef GAL_ROM_Z80_PROG1
#undef GAL_ROM_Z80_PROG2
#undef GAL_ROM_Z80_PROG3
#undef GAL_ROM_TILES_SHARED
#undef GAL_ROM_TILES_CHARS
#undef GAL_ROM_TILES_SPRITES
#undef GAL_ROM_PROM
#undef GAL_ROM_S2650_PROG1

// megadrive.h
#undef SEGA_MD_ROM_LOAD_NORMAL
#undef SEGA_MD_ROM_LOAD16_WORD_SWAP
#undef SEGA_MD_ROM_LOAD16_BYTE
#undef SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000
#undef SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000
#undef SEGA_MD_ROM_OFFS_000000
#undef SEGA_MD_ROM_OFFS_000001
#undef SEGA_MD_ROM_OFFS_020000
#undef SEGA_MD_ROM_OFFS_080000
#undef SEGA_MD_ROM_OFFS_100000
#undef SEGA_MD_ROM_OFFS_100001
#undef SEGA_MD_ROM_OFFS_200000
#undef SEGA_MD_ROM_OFFS_300000
#undef SEGA_MD_ROM_RELOAD_200000_200000
#undef SEGA_MD_ROM_RELOAD_100000_300000
#undef SEGA_MD_ARCADE_SUNMIXING

// sys16.h
#undef SYS16_ROM_PROG_FLAT
#undef SYS16_ROM_PROG
#undef SYS16_ROM_TILES
#undef SYS16_ROM_SPRITES
#undef SYS16_ROM_Z80PROG
#undef SYS16_ROM_KEY
#undef SYS16_ROM_7751PROG
#undef SYS16_ROM_7751DATA
#undef SYS16_ROM_UPD7759DATA
#undef SYS16_ROM_PROG2
#undef SYS16_ROM_ROAD
#undef SYS16_ROM_PCMDATA
#undef SYS16_ROM_Z80PROG2
#undef SYS16_ROM_Z80PROG3
#undef SYS16_ROM_Z80PROG4
#undef SYS16_ROM_PCM2DATA
#undef SYS16_ROM_PROM
#undef SYS16_ROM_PROG3
#undef SYS16_ROM_SPRITES2
#undef SYS16_ROM_RF5C68DATA
#undef SYS16_ROM_I8751
#undef SYS16_ROM_MSM6295
#undef SYS16_ROM_TILES_20000

// taito.h
#undef TAITO_68KROM1
#undef TAITO_68KROM1_BYTESWAP
#undef TAITO_68KROM1_BYTESWAP_JUMPING
#undef TAITO_68KROM1_BYTESWAP32
#undef TAITO_68KROM2
#undef TAITO_68KROM2_BYTESWAP
#undef TAITO_68KROM3
#undef TAITO_68KROM3_BYTESWAP
#undef TAITO_Z80ROM1
#undef TAITO_Z80ROM2
#undef TAITO_CHARS
#undef TAITO_CHARS_BYTESWAP
#undef TAITO_CHARSB
#undef TAITO_CHARSB_BYTESWAP
#undef TAITO_SPRITESA
#undef TAITO_SPRITESA_BYTESWAP
#undef TAITO_SPRITESA_BYTESWAP32
#undef TAITO_SPRITESA_TOPSPEED
#undef TAITO_SPRITESB
#undef TAITO_SPRITESB_BYTESWAP
#undef TAITO_SPRITESB_BYTESWAP32
#undef TAITO_ROAD
#undef TAITO_SPRITEMAP
#undef TAITO_YM2610A
#undef TAITO_YM2610B
#undef TAITO_MSM5205
#undef TAITO_MSM5205_BYTESWAP
#undef TAITO_CHARS_PIVOT
#undef TAITO_MSM6295
#undef TAITO_ES5505
#undef TAITO_ES5505_BYTESWAP
#undef TAITO_DEFAULT_EEPROM
#undef TAITO_CHARS_BYTESWAP32
#undef TAITO_CCHIP_BIOS
#undef TAITO_CCHIP_EEPROM

static UINT32 RDGetRomsType(const TCHAR* pszDrvName, const UINT32 nBaseType, const INT32 nMinIdx, const INT32 nMaxIdx)
{
	char* pRomName;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup

	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & nBaseType) && ((ri.nType & 0x0f) >= nMinIdx) && ((ri.nType & 0x0f) <= nMaxIdx)) {
			nBurnDrvActive = nOldDrvSel;		// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;				// Restore
	return nBaseType;
}

// Consoles will automatically get the Type of the ROMs
static UINT32 RDGetConsoleRomsType(const TCHAR* pszDrvName)
{
	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup

	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & BRF_PRG) {
			nBurnDrvActive = nOldDrvSel;		// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;				// Restore
	return BRF_PRG;
}

// FBNeo style is preferred, universal solutions are not recommended
static UINT32 RDGetUniversalRomsType(const TCHAR* pszDrvName, const TCHAR* pszMask)
{
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Z80")) || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("BoardPLD")) || 0 == _tcsicmp(pszMask, _T("OPT1"))) {
		nBaseType = BRF_OPT;
	}

	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup

	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & nBaseType) {
			nBurnDrvActive = nOldDrvSel;		// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;				// Restore
	return nBaseType;
}

static UINT32 RDGetCps1RomsType(const TCHAR* pszDrvName, const TCHAR* pszMask, const UINT32 nPrgLen)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = CPS1_68K_PROGRAM_BYTESWAP, nMaxIdx = CPS1_68K_PROGRAM_NO_BYTESWAP;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("ProgramS")) || 0 == _tcsicmp(pszMask, _T("PRGS"))) {
		return (BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_BYTESWAP);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("ProgramN")) || 0 == _tcsicmp(pszMask, _T("PRGN"))) {
		return (BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_NO_BYTESWAP);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Z80")) || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = CPS1_Z80_PROGRAM;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = CPS1_TILES;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = CPS1_OKIM6295_SAMPLES, nMaxIdx = CPS1_QSOUND_SAMPLES;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Pic")) || 0 == _tcsicmp(pszMask, _T("SNDB"))) {
		nMinIdx = nMaxIdx = CPS1_PIC;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Tiles")) || 0 == _tcsicmp(pszMask, _T("Extras")) || 0 == _tcsicmp(pszMask, _T("GRA2"))) {
		nMinIdx = CPS1_EXTRA_TILES_SF2EBBL_400000, nMaxIdx = CPS1_EXTRA_TILES_SF2MKOT_400000;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("BoardPLD")) || 0 == _tcsicmp(pszMask, _T("OPT1"))) {
		return BRF_OPT;
	}

	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup

	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & nBaseType) && ((ri.nType & 0x0f) >= nMinIdx) && ((ri.nType & 0x0f) <= nMaxIdx)) {
			if (nPrgLen > 0 && (CPS1_68K_PROGRAM_BYTESWAP == nMinIdx || CPS1_68K_PROGRAM_NO_BYTESWAP == nMaxIdx)) {
				if (nPrgLen >= 0x40000 && (CPS1_68K_PROGRAM_BYTESWAP == (ri.nType & 0x0f))) {
					ri.nType = (ri.nType & ~CPS1_68K_PROGRAM_BYTESWAP) | CPS1_68K_PROGRAM_NO_BYTESWAP;
				}
				else
				if (nPrgLen < 0x40000 && (CPS1_68K_PROGRAM_NO_BYTESWAP == (ri.nType & 0x0f))) {
					ri.nType = (ri.nType & ~CPS1_68K_PROGRAM_NO_BYTESWAP) | CPS1_68K_PROGRAM_BYTESWAP;
				}
			}
			nBurnDrvActive = nOldDrvSel;		// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;				// Restore
	return 0;
}

// d_cps1.cpp
#undef CPS1_68K_PROGRAM_BYTESWAP
#undef CPS1_68K_PROGRAM_NO_BYTESWAP
#undef CPS1_Z80_PROGRAM
#undef CPS1_TILES
#undef CPS1_OKIM6295_SAMPLES
#undef CPS1_QSOUND_SAMPLES
#undef CPS1_PIC
#undef CPS1_EXTRA_TILES_SF2EBBL_400000
#undef CPS1_EXTRA_TILES_400000
#undef CPS1_EXTRA_TILES_SF2KORYU_400000
#undef CPS1_EXTRA_TILES_SF2B_400000
#undef CPS1_EXTRA_TILES_SF2MKOT_400000

static UINT32 RDGetCps2RomsType(const TCHAR* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = CPS2_PRG_68K, nMaxIdx = CPS2_PRG_68K_XOR_TABLE;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = CPS2_GFX, nMaxIdx = CPS2_GFX_19XXJ;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Z80")) || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = CPS2_PRG_Z80;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = CPS2_QSND, nMaxIdx = CPS2_QSND_SIMM_BYTESWAP;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Decryption")) || 0 == _tcsicmp(pszMask, _T("KEY1"))) {
		return CPS2_ENCRYPTION_KEY;
	}

	return RDGetRomsType(pszDrvName, nBaseType, nMinIdx, nMaxIdx);
}

// d_cps2.cpp
#undef CPS2_PRG_68K
#undef CPS2_PRG_68K_SIMM
#undef CPS2_PRG_68K_XOR_TABLE
#undef CPS2_GFX
#undef CPS2_GFX_SIMM
#undef CPS2_GFX_SPLIT4
#undef CPS2_GFX_SPLIT8
#undef CPS2_GFX_19XXJ
#undef CPS2_PRG_Z80
#undef CPS2_QSND
#undef CPS2_QSND_SIMM
#undef CPS2_QSND_SIMM_BYTESWAP
#undef CPS2_ENCRYPTION_KEY

static UINT32 RDGetCps3RomsType(const TCHAR* pszMask)
{
	if (0 == _tcsicmp(pszMask, _T("Bios"))) {
		return (BRF_ESS | BRF_BIOS);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		return (BRF_ESS | BRF_PRG);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		return BRF_GRA;
	}
	return 0;
}

static INT32 RDGetPgmRomsType(const TCHAR* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = nMaxIdx = 1;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Tile")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = 2;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("SpriteData")) || 0 == _tcsicmp(pszMask, _T("GRA2"))) {
		nMinIdx = nMaxIdx = 3;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("SpriteMasks")) || 0 == _tcsicmp(pszMask, _T("GRA3"))) {
		nMinIdx = nMaxIdx = 4;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = nMaxIdx = 5;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("InternalARM7")) || 0 == _tcsicmp(pszMask, _T("PRG2"))) {
		nMinIdx = nMaxIdx = 7;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Ramdump"))) {
		//What's the point of existing when the rom length is 0 and the Nodump marker won't load?
		return (7 | BRF_PRG | BRF_ESS | BRF_NODUMP);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("ExternalARM7")) || 0 == _tcsicmp(pszMask, _T("PRG3"))) {
		nMinIdx = nMaxIdx = 8;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("ProtectionRom")) || 0 == _tcsicmp(pszMask, _T("PRG4"))) {
		nMinIdx = nMaxIdx = 9;
		nBaseType = BRF_PRG;
	}

	return RDGetRomsType(pszDrvName, nBaseType, nMinIdx, nMaxIdx);
}

static UINT32 RDGetNeoGeoRomsType(const TCHAR* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = nMaxIdx = 1;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Text")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = 2;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA2"))) {
		nMinIdx = nMaxIdx = 3;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Z80")) || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = 4;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = nMaxIdx = 5;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("SamplesAES")) || 0 == _tcsicmp(pszMask, _T("SND2"))) {
		// See nam1975RomDesc
		nMinIdx = nMaxIdx = 6;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("BoardPLD")) || 0 == _tcsicmp(pszMask, _T("OPT1"))) {
		return 0 | BRF_OPT;
	}

	return RDGetRomsType(pszDrvName, nBaseType, nMinIdx, nMaxIdx);
}

static INT32 GetDrvHardwareMask(const TCHAR* pszDrvName)
{
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup
	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	const INT32 nHardwareCode = BurnDrvGetHardwareCode();
	nBurnDrvActive = nOldDrvSel;				// Restore

	return (nHardwareCode & HARDWARE_PUBLIC_MASK);
}

static UINT32 RDSetRomsType(const TCHAR* pszDrvName, const TCHAR* pszMask, const UINT32 nPrgLen)
{
	INT32 nHardwareMask = GetDrvHardwareMask(pszDrvName);
	switch (nHardwareMask){
		case HARDWARE_IGS_PGM:
			return RDGetPgmRomsType(pszDrvName, pszMask);

		case HARDWARE_SNK_NEOGEO:
			return RDGetNeoGeoRomsType(pszDrvName, pszMask);

		case HARDWARE_CAPCOM_CPS1:
		case HARDWARE_CAPCOM_CPS1_QSOUND:
		case HARDWARE_CAPCOM_CPS1_GENERIC:
		case HARDWARE_CAPCOM_CPSCHANGER:
			return RDGetCps1RomsType(pszDrvName, pszMask, nPrgLen);

		case HARDWARE_CAPCOM_CPS2:
		case HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM:
			return RDGetCps2RomsType(pszDrvName, pszMask);

		case HARDWARE_CAPCOM_CPS3:
		case HARDWARE_CAPCOM_CPS3 | HARDWARE_CAPCOM_CPS3_NO_CD:
			return RDGetCps3RomsType(pszMask);	// win32/romdata.cpp passes the driver name

		case HARDWARE_SEGA_MEGADRIVE:
		case HARDWARE_PCENGINE_PCENGINE:
		case HARDWARE_PCENGINE_SGX:
		case HARDWARE_PCENGINE_TG16:
		case HARDWARE_SEGA_SG1000:
		case HARDWARE_COLECO:
		case HARDWARE_SEGA_MASTER_SYSTEM:
		case HARDWARE_SEGA_GAME_GEAR:
		case HARDWARE_MSX:
		case HARDWARE_SPECTRUM:
		case HARDWARE_NES:
		case HARDWARE_FDS:
		case HARDWARE_SNES:
		case HARDWARE_SNK_NGP:
		case HARDWARE_CHANNELF:
			return RDGetConsoleRomsType(pszDrvName);

		default:
			return RDGetUniversalRomsType(pszDrvName, pszMask);
	}
}

// ----------------------------------------------------------------------------
// The dats

struct BurnRomInfo* pDataRomDesc = NULL;

TCHAR szRomdataName[MAX_PATH] = _T("");
char szRomDataTitle[MAX_PATH * 3] = "";		// the FullName of the dat in use, UTF-8

static struct BurnRomInfo* pDRD = NULL;
static struct RomDataInfo RDI = { 0 };
RomDataInfo* pRDI = &RDI;

#define DELIM_TOKENS_NAME	_T(" \t\r\n,%:|{}")

// strtok() with the quoted strings as one token
static TCHAR* RDToken(TCHAR* s, const TCHAR* delims)
{
	static TCHAR* prev_str = NULL;
	TCHAR* token = NULL;

	if (!s) {
		if (!prev_str) return NULL;
		s = prev_str;
	}

	s += strspn(s, delims);
	if (s[0] == _T('\0')) {
		prev_str = NULL;
		return NULL;
	}

	if (s[0] == _T('\"')) {						// time to grab quoted string!
		token = ++s;
		if ((s = strpbrk(token, _T("\"")))) {
			*(s++) = _T('\0');
		} else {
			s = token + _tcslen(token);
		}
	} else {
		token = s;
	}

	if ((s = strpbrk(s, delims))) {
		*(s++) = _T('\0');
		prev_str = s;
	} else {
		prev_str = NULL;
	}

	return token;
}

// the sizes and CRCs are hexadecimal, with or without 0x
static bool StrToUint(const TCHAR* str, UINT32* result)
{
	if (NULL == str || _T('\0') == *str || NULL == result) return false;

	TCHAR* endPtr;
	unsigned long value = strtoul(str, &endPtr, 16);

	if (str == endPtr)       return false;
	if (_T('\0') != *endPtr) return false;
	if (value > UINT_MAX)    return false;

	*result = (UINT32)value;
	return true;
}

static bool FileExists(const TCHAR* pszName)
{
	FILE* fp = _tfopen(pszName, _T("rb"));
	if (fp) {
		fclose(fp);
		return true;
	}
	return false;
}

static void Utf8ToWide(const char* pszText, wchar_t* pszOut, int nMax)
{
	int n = 0;
	const UINT8* s = (const UINT8*)pszText;
	while (*s && n < nMax - 1) {
		UINT32 c = *s++;
		if (c >= 0xF0 && s[0] && s[1] && s[2]) {
			c = ((c & 0x07) << 18) | ((s[0] & 0x3F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
			s += 3;
		} else if (c >= 0xE0 && s[0] && s[1]) {
			c = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
			s += 2;
		} else if (c >= 0xC0 && s[0]) {
			c = ((c & 0x1F) << 6) | (s[0] & 0x3F);
			s += 1;
		}
		if (sizeof(wchar_t) == 2 && c >= 0x10000) {
			if (n >= nMax - 2) break;
			c -= 0x10000;
			pszOut[n++] = (wchar_t)(0xD800 | (c >> 10));
			pszOut[n++] = (wchar_t)(0xDC00 | (c & 0x3FF));
		} else {
			pszOut[n++] = (wchar_t)c;
		}
	}
	pszOut[n] = 0;
}

// the rest of the line after the label, the words joined by one space (FullName)
static void JoinTokens(TCHAR* pszOut, int nSize)
{
	int nAdd = 0;
	TCHAR* pszInfo;
	pszOut[0] = _T('\0');
	while (NULL != (pszInfo = RDToken(NULL, DELIM_TOKENS_NAME)) && nAdd < nSize - 1) {
		nAdd += snprintf(pszOut + nAdd, nSize - nAdd, nAdd ? " %s" : "%s", pszInfo);
	}
}

// The value of "Label: value" (or of its other name) in a dat
static bool DatValue(const TCHAR* pszDat, const TCHAR* pszLabel1, const TCHAR* pszLabel2, bool bAllWords, TCHAR* pszOut, int nSize)
{
	char* pszText = TextFileLoad(pszDat);
	if (pszText == NULL) {
		return false;
	}

	bool bFound = false;
	char* pszPos = pszText;
	char* pszLine;
	while (!bFound && (pszLine = TextFileNextLine(&pszPos)) != NULL) {
		TCHAR* pszLabel = RDToken(pszLine, DELIM_TOKENS_NAME);
		if (NULL == pszLabel) continue;
		if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;
		if (_tcsicmp(pszLabel1, pszLabel) && _tcsicmp(pszLabel2, pszLabel)) continue;

		if (bAllWords) {
			JoinTokens(pszOut, nSize);
			bFound = pszOut[0] != _T('\0');
		} else {
			TCHAR* pszInfo = RDToken(NULL, DELIM_TOKENS_NAME);
			if (NULL == pszInfo) break;
			snprintf(pszOut, nSize, "%s", pszInfo);
			bFound = true;
		}
	}
	free(pszText);

	return bFound;
}

TCHAR* RomdataGetZipName(const TCHAR* pszFileName)
{
	static TCHAR szRet[100];
	return DatValue(pszFileName, _T("ZipName"), _T("RomName"), false, szRet, sizeof(szRet)) ? szRet : NULL;
}

TCHAR* RomdataGetDrvName(const TCHAR* pszFileName)
{
	static TCHAR szRet[100];
	return DatValue(pszFileName, _T("DrvName"), _T("Parent"), false, szRet, sizeof(szRet)) ? szRet : NULL;
}

TCHAR* RomdataGetFullName(const TCHAR* pszFileName)
{
	static TCHAR szRet[MAX_PATH * 3];
	return DatValue(pszFileName, _T("FullName"), _T("Game"), true, szRet, sizeof(szRet)) ? szRet : NULL;
}

// The driver of szRomdataName (core API, burn.h)
char* RomdataGetDrvName()
{
	return RomdataGetDrvName(szRomdataName);
}

// The roms of szRomdataName, in pDRD (NULL: only counted)
static INT32 LoadRomdata()
{
	char* pszText = TextFileLoad(szRomdataName);
	if (pszText == NULL) return -1;

	RDI.nDescCount = -1;						// Failed

	TCHAR szRomMask[30] = { 0 }, szDrvName[100] = { 0 };
	INT32 nDatMode = 0;							// FBNeo 0, Nebula 1

	memset(RDI.szExtraRom, 0, sizeof(RDI.szExtraRom));
	memset(RDI.szFullName, 0, sizeof(RDI.szFullName));

	char* pszPos = pszText;
	char* pszBuf;
	while ((pszBuf = TextFileNextLine(&pszPos)) != NULL) {
		TCHAR* pszLabel = RDToken(pszBuf, DELIM_TOKENS_NAME);
		TCHAR* pszInfo = NULL;
		if (NULL == pszLabel) continue;
		if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

		UINT32 nLen = _tcslen(pszLabel);
		if ((nLen > 2) && (_T('[') == pszLabel[0]) && (_T(']') == pszLabel[nLen - 1])) {
			pszLabel++, nLen -= 2;
			if (0 == _tcsicmp(_T("System"), pszLabel)) break;
			memset(szRomMask, 0, sizeof(szRomMask));
			_tcsncpy(szRomMask, pszLabel, nLen < sizeof(szRomMask) - 1 ? nLen : sizeof(szRomMask) - 1);
			continue;
		}
		if (0 == _tcsicmp(_T("System"), pszLabel)) {
			nDatMode = 1;							// Nebula
			continue;
		}
		if (0 == _tcsicmp(_T("ZipName"), pszLabel) || 0 == _tcsicmp(_T("RomName"), pszLabel)) {
			pszInfo = RDToken(NULL, DELIM_TOKENS_NAME);
			if (NULL == pszInfo) break;				// No romset specified
			if (NULL != pDRD) {
				snprintf(RDI.szZipName, sizeof(RDI.szZipName), "%s", pszInfo);
			}
			continue;
		}
		if (0 == _tcsicmp(_T("DrvName"), pszLabel) || 0 == _tcsicmp(_T("Parent"), pszLabel)) {
			pszInfo = RDToken(NULL, DELIM_TOKENS_NAME);
			if (NULL == pszInfo) break;				// No driver specified
			if (NULL != pDRD) {
				snprintf(RDI.szDrvName, sizeof(RDI.szDrvName), "%s", pszInfo);
			}
			snprintf(szDrvName, sizeof(szDrvName), "%s", pszInfo);
			continue;
		}
		if (0 == _tcsicmp(_T("ExtraRom"), pszLabel)) {
			pszInfo = RDToken(NULL, DELIM_TOKENS_NAME);
			if ((NULL != pszInfo) && (NULL != pDRD)) {
				snprintf(RDI.szExtraRom, sizeof(RDI.szExtraRom), "%s", pszInfo);
			}
			continue;
		}
		if (0 == _tcsicmp(_T("FullName"), pszLabel) || 0 == _tcsicmp(_T("Game"), pszLabel)) {
			TCHAR szMerger[MAX_PATH * 3];
			JoinTokens(szMerger, sizeof(szMerger));
			if (NULL != pDRD) {
				Utf8ToWide(szMerger, RDI.szFullName, MAX_PATH);
				snprintf(szRomDataTitle, sizeof(szRomDataTitle), "%s", szMerger);
			}
			continue;
		}

		{
			// romname, len, crc, type
			struct BurnRomInfo ri = { 0 };
			ri.nLen  = UINT_MAX;
			ri.nCrc  = UINT_MAX;
			ri.nType = 0U;

			if (1 == nDatMode) {
				// Skips content when recognized as a Nebula style
				pszInfo = RDToken(NULL, DELIM_TOKENS_NAME);
			}
			pszInfo = RDToken(NULL, DELIM_TOKENS_NAME);
			if (NULL == pszInfo) continue;
			StrToUint(pszInfo, &(ri.nLen));
			if ((UINT_MAX == ri.nLen) || (0 == ri.nLen)) continue;

			pszInfo = RDToken(NULL, DELIM_TOKENS_NAME);
			if (NULL == pszInfo) continue;
			StrToUint(pszInfo, &(ri.nCrc));
			if (UINT_MAX == ri.nCrc) continue;

			while (NULL != (pszInfo = RDToken(NULL, DELIM_TOKENS_NAME))) {
				UINT32 nValue = UINT_MAX;

				for (UINT32 i = 0; i < (sizeof(RDMacroTable) / sizeof(RDMacroMap)); i++) {
					if (0 == _tcscmp(pszInfo, RDMacroTable[i].pszDRMacro)) {
						ri.nType |= RDMacroTable[i].nRDMacro;
					}
				}

				if (StrToUint(pszInfo, &nValue)) {
					ri.nType |= nValue;
				}
			}
			// FBNeo style ROMs have a higher priority for nType than Nebula style
			if (0 == ri.nType) {
				ri.nType = RDSetRomsType(szDrvName, szRomMask, ri.nLen);
			}
			if (ri.nType > 0U) {
				RDI.nDescCount++;

				if (NULL != pDRD) {
					pDRD[RDI.nDescCount].szName = (char*)malloc(512);
					memset(pDRD[RDI.nDescCount].szName, 0, sizeof(char) * 512);
					snprintf(pDRD[RDI.nDescCount].szName, 512, "%s", pszLabel);

					pDRD[RDI.nDescCount].nLen  = ri.nLen;
					pDRD[RDI.nDescCount].nCrc  = ri.nCrc;
					pDRD[RDI.nDescCount].nType = ri.nType;
				}
			}
		}
	}
	free(pszText);

	if (NULL != pDRD) {
		pDataRomDesc = (struct BurnRomInfo*)malloc((RDI.nDescCount + 1) * sizeof(BurnRomInfo));
		if (NULL == pDataRomDesc) return -1;

		for (INT32 i = 0; i <= RDI.nDescCount; i++) {
			pDataRomDesc[i] = pDRD[i];			// the names move over
		}
		free(pDRD); pDRD = NULL;
	}

	return RDI.nDescCount;
}

// Before the load of the driver: its roms come from the dat szRomdataName,
// and its zip name is the one of the dat
void RomDataInit()
{
	if (szRomdataName[0] == _T('\0') || NULL != pDataRomDesc) {
		return;
	}

	INT32 nLen = LoadRomdata();

	if ((nLen >= 0) && (NULL == pDRD)) {
		pDRD = (struct BurnRomInfo*)malloc((nLen + 1) * sizeof(BurnRomInfo));
		if (NULL == pDRD) return;

		LoadRomdata();
		RDI.nDriverId = BurnDrvGetIndex(RDI.szDrvName);

		if (RDI.nDriverId >= 0) {
			BurnDrvSetZipName(RDI.szZipName, RDI.nDriverId);
		}
	}
}

void RomDataSetFullName()
{
	// At this point, the driver's default ZipName has been replaced with the ZipName from the rom data
	RDI.nDriverId = BurnDrvGetIndex(RDI.szZipName);

	if (RDI.nDriverId >= 0) {
		wchar_t* szOldName = BurnDrvGetFullNameW(RDI.nDriverId);
		memset(RDI.szOldName, '\0', sizeof(RDI.szOldName));
		if (szOldName) {
			wcsncpy(RDI.szOldName, szOldName, MAX_PATH - 1);
		}
		// BurnDrvSetFullNameW() is only for _UNICODE: the interface shows szRomDataTitle
	}
}

void RomDataExit()
{
	if (NULL != pDataRomDesc) {
		for (INT32 i = 0; i < RDI.nDescCount + 1; i++) {
			free(pDataRomDesc[i].szName);
		}
		free(pDataRomDesc); pDataRomDesc = NULL;

		if (RDI.nDriverId >= 0) {
			BurnDrvSetZipName(RDI.szDrvName, RDI.nDriverId);
		}

		memset(&RDI, 0, sizeof(RomDataInfo));
		RDI.nDescCount = -1;

		// only when it was in use: DrvInit() calls DrvExit() before using a new one
		memset(szRomdataName, 0, sizeof(szRomdataName));
		szRomDataTitle[0] = '\0';
	}
}

// The driver of a dat, -1 if there's none (the message is given by AppError())
INT32 RomDataCheck(const TCHAR* pszDatFile)
{
	char szText[MAX_PATH + 128];

	if (NULL == pszDatFile || !FileExists(pszDatFile)) {
		snprintf(szText, sizeof(szText), "RomData: %s doesn't exist.\n", pszDatFile ? pszDatFile : "");
		AppError(szText, 0);
		return -1;
	}

	const TCHAR* pszDrvName = RomdataGetDrvName(pszDatFile);
	INT32 nDrvIdx = pszDrvName ? BurnDrvGetIndex((char*)pszDrvName) : -1;
	if (nDrvIdx < 0) {
		snprintf(szText, sizeof(szText), "RomData: %s\n\nThe driver %s doesn't exist.\n", pszDatFile, pszDrvName ? pszDrvName : "(none)");
		AppError(szText, 0);
		return -3;
	}

	const TCHAR* pszZipName = RomdataGetZipName(pszDatFile);
	if (NULL == pszZipName) {
		snprintf(szText, sizeof(szText), "RomData: %s\n\nNo ZipName.\n", pszDatFile);
		AppError(szText, 0);
		return -7;
	}
	if (BurnDrvGetIndex((char*)pszZipName) >= 0) {
		snprintf(szText, sizeof(szText), "RomData: %s\n\nThe ZipName %s is the name of a set of this version.\n", pszDatFile, pszZipName);
		AppError(szText, 0);
		return -5;
	}

	return nDrvIdx;
}

// The fields of a dat for the list of the manager
INT32 RomDataGetListInfo(const TCHAR* pszDatFile, RomDataListInfo* pInfo)
{
	memset(pInfo, 0, sizeof(*pInfo));
	snprintf(pInfo->szDat, sizeof(pInfo->szDat), "%s", pszDatFile);

	const TCHAR* pszZip = RomdataGetZipName(pszDatFile);
	const TCHAR* pszDrv = RomdataGetDrvName(pszDatFile);
	if (pszZip == NULL || pszDrv == NULL) {
		return 1;
	}
	snprintf(pInfo->szZipName, sizeof(pInfo->szZipName), "%s", pszZip);
	snprintf(pInfo->szDrvName, sizeof(pInfo->szDrvName), "%s", pszDrv);
	pInfo->nDrv = BurnDrvGetIndex((char*)pszDrv);

	const TCHAR* pszFull = RomdataGetFullName(pszDatFile);
	snprintf(pInfo->szFullName, sizeof(pInfo->szFullName), "%s", pszFull ? pszFull : pszZip);

	if (pInfo->nDrv >= 0) {
		UINT32 nOldDrv = nBurnDrvActive;
		nBurnDrvActive = pInfo->nDrv;
		bool bMvs = HARDWARE_SNK_MVS == (BurnDrvGetHardwareCode() & HARDWARE_SNK_MVS) && HARDWARE_SNK_NEOGEO == (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK);
		snprintf(pInfo->szHardware, sizeof(pInfo->szHardware), "%s", bMvs ? "Neo Geo MVS Cartridge" : BurnDrvGetTextA(DRV_SYSTEM));
		nBurnDrvActive = nOldDrv;
	}

	return 0;
}

// A dat with the roms of a driver, to be edited into a hack's dat
INT32 RomDataExportTemplate(INT32 nDrv, const TCHAR* pszFile)
{
	FILE* fp = _tfopen(pszFile, _T("w"));
	if (NULL == fp) {
		return 1;
	}

	const UINT32 nOldDrvSel = nBurnDrvActive;
	nBurnDrvActive = nDrv;

	fprintf(fp, "// RomData template for FinalBurn Neo\n\n");
	fprintf(fp, "// The *** in the ZipName field cannot be empty and cannot be duplicated with DrvName\n");
	fprintf(fp, "ZipName: ***\n");
	fprintf(fp, "DrvName: %s\n", BurnDrvGetTextA(DRV_NAME));
	fprintf(fp, "FullName: \"%s\"\n\n", BurnDrvGetTextA(DRV_FULLNAME));
	fprintf(fp, "// Name\t\tLen\t\tCrc32\t\tType\n");

	char* pszRomName = NULL;
	for (INT32 i = 0; !BurnDrvGetRomName(&pszRomName, i, 0); i++) {
		struct BurnRomInfo ri = { 0 };

		BurnDrvGetRomInfo(&ri, i);	// Get info about the rom
		if ((NULL == pszRomName) || ('\0' == *pszRomName))
			continue;
		if ((ri.nType & BRF_BIOS) && (i >= 0x80))
			continue;

		fprintf(fp, "\"%s\",\t0x%08x,\t0x%08x,\t0x%08x\n", pszRomName, ri.nLen, ri.nCrc, ri.nType);
	}
	fclose(fp);
	nBurnDrvActive = nOldDrvSel;

	return 0;
}

#undef DELIM_TOKENS_NAME

// RomData driver API of the other ports, kept for them
extern "C" bool IsRomDataDrv(void)
{
	return NULL != pDataRomDesc;
}

extern "C" char* RomDataDrvGetDrvName(void)
{
	return pDataRomDesc ? RDI.szDrvName : NULL;
}

extern "C" struct BurnRomInfo* RomDataDrvGetRomInfo(UINT32* pRomCount)
{
	if (pRomCount) *pRomCount = pDataRomDesc ? RDI.nDescCount + 1 : 0;
	return pDataRomDesc;
}

extern "C" const char* RomDataDrvGetExtName(void)
{
	return (pDataRomDesc && RDI.szExtraRom[0]) ? RDI.szExtraRom : NULL;
}
