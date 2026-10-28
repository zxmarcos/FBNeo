// IPS patches: the engine of win32/ips_manager.cpp (the active patches of a
// game, its #define and #include lines, the patching of the roms). The
// selection of the patches is done in ui/ui_ips.cpp.
#include "burner.h"

#define MAX_ACTIVE_PATCHES	1024

#define IPS_SIGNATURE		"PATCH"
#define IPS_TAG_EOF			"EOF"
#define IPS_EXT				".ips"

#define BYTE3_TO_UINT(bp)						\
	 (((UINT32)(bp)[0] << 16) & 0x00FF0000) |	\
	 (((UINT32)(bp)[1] <<  8) & 0x0000FF00) |	\
	 (( UINT32)(bp)[2]        & 0x000000FF)

#define BYTE2_TO_UINT(bp)						\
	(((UINT32)(bp)[0] << 8) & 0xFF00) |			\
	(( UINT32)(bp)[1]       & 0x00FF)

const char* IpsLanguageNames[IPS_LANGUAGES] = {
	"English (US)", "Simplified Chinese", "Traditional Chinese", "Japanese", "Korean", "French",
	"Spanish", "Italian", "German", "Portuguese", "Polish", "Hungarian",
};
const char* IpsLanguageCodes[IPS_LANGUAGES] = {
	"en_US", "zh_CN", "zh_TW", "ja_JP", "ko_KR", "fr_FR", "es_ES", "it_IT", "de_DE", "pt_BR", "pl_PL", "hu_HU",
};

bool bDoIpsPatch = false;
INT32 nIpsSelectedLanguage = 0;
UINT32 nIpsDrvDefine = 0, nIpsMemExpLen[SND2_ROM + 1] = { 0 };

static INT32 nRomOffset = 0;
static char (*pszActivePatches)[MAX_PATH] = NULL;	// MAX_ACTIVE_PATCHES of them
static INT32 nActivePatches = 0;

// #include "other.dat": before or after the patch lines of the dat that has it
struct IpsExtraPatch {
	char** pszDat;
	UINT32 nCount;
};
static IpsExtraPatch IpsEarly = { NULL, 0 }, IpsLast = { NULL, 0 };

static bool FileExists(const char* pszName)
{
	FILE* fp = fopen(pszName, "rb");
	if (fp) {
		fclose(fp);
		return true;
	}
	return false;
}

const char* IpsPatchFolder(const char* pszDrvName)
{
	static char szFolder[MAX_PATH * 2];
	int nLen = strlen(szAppIpsPath);
	bool bSlash = nLen > 0 && (szAppIpsPath[nLen - 1] == '/' || szAppIpsPath[nLen - 1] == '\\');
	snprintf(szFolder, sizeof(szFolder), "%s%s%s/", szAppIpsPath, bSlash ? "" : "/", pszDrvName);
	return szFolder;
}

static const char* IpsConfigName(const char* pszDrvName)
{
	static char szName[MAX_PATH * 2];
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	char* pszPath = SDL_GetPrefPath("fbneo", "config/ips");
	snprintf(szName, sizeof(szName), "%s%s.ini", pszPath ? pszPath : "", pszDrvName);
	SDL_free(pszPath);
#else
	snprintf(szName, sizeof(szName), "config/ips/%s.ini", pszDrvName);
#endif
	return szName;
}

// ----------------------------------------------------------------------------
// The active patches of the game

static bool ActiveAlloc()
{
	if (pszActivePatches == NULL) {
		pszActivePatches = (char(*)[MAX_PATH])calloc(MAX_ACTIVE_PATCHES, MAX_PATH);
	}
	return pszActivePatches != NULL;
}

INT32 LoadIpsActivePatches()
{
	nActivePatches = 0;
	if (!ActiveAlloc()) {
		return 0;
	}

	const char* pszDrvName = BurnDrvGetTextA(DRV_NAME);
	char* pszText = TextFileLoad(IpsConfigName(pszDrvName));
	if (pszText == NULL) {
		return 0;
	}

	char* pszPos = pszText;
	char* pszLine;
	while ((pszLine = TextFileNextLine(&pszPos)) != NULL && nActivePatches < MAX_ACTIVE_PATCHES) {
		if (strncmp(pszLine, "//", 2) == 0 || pszLine[0] == '\0') {
			continue;
		}
		snprintf(pszActivePatches[nActivePatches], MAX_PATH, "%s%s", IpsPatchFolder(pszDrvName), pszLine);
		if (FileExists(pszActivePatches[nActivePatches])) {		// the dat may have been removed since
			nActivePatches++;
		}
	}
	free(pszText);

	return nActivePatches;
}

INT32 IpsActivePatchCount()
{
	return nActivePatches;
}

const char* IpsActivePatch(INT32 i)
{
	return (i >= 0 && i < nActivePatches) ? pszActivePatches[i] : NULL;
}

INT32 IpsSaveActivePatches(const char* pszDrvName, const char* pszFullName, const char* const* ppszDats, INT32 nCount)
{
	FILE* fp = fopen(IpsConfigName(pszDrvName), "wt");
	if (fp == NULL) {
		return 1;
	}
	fprintf(fp, "// " APP_TITLE " v%s --- IPS Config File for %s (%s)\n\n", szAppBurnVer, pszDrvName, pszFullName);
	for (INT32 i = 0; i < nCount; i++) {
		// the file name only, in the folder of the game
		const char* pszName = ppszDats[i];
		for (const char* p = ppszDats[i]; *p; p++) {
			if (*p == '/' || *p == '\\') pszName = p + 1;
		}
		fprintf(fp, "%s\n", pszName);
	}
	fclose(fp);

	return 0;
}

// The text of the [xx_XX] section, up to the next section
static char* DescByLanguage(const char* pszText, INT32 nLang)
{
	char szTag[16];
	snprintf(szTag, sizeof(szTag), "[%s]", IpsLanguageCodes[nLang]);
	int nTag = strlen(szTag);

	const char* p = pszText;
	while (p && *p) {
		const char* pEnd = strchr(p, '\n');
		if (strncmp(p, szTag, nTag) == 0) {
			const char* pStart = pEnd ? pEnd + 1 : p + strlen(p);
			const char* q = pStart;
			while (*q && *q != '[') {			// the lines up to the next section
				const char* qEnd = strchr(q, '\n');
				q = qEnd ? qEnd + 1 : q + strlen(q);
			}
			// without the line ends at the end
			const char* r = q;
			while (r > pStart && (r[-1] == '\n' || r[-1] == '\r')) r--;
			if (r == pStart) {
				return NULL;
			}
			char* pszDesc = (char*)malloc(r - pStart + 1);
			if (pszDesc) {
				char* d = pszDesc;
				for (const char* s = pStart; s < r; s++) {
					if (*s != '\r') *d++ = *s;
				}
				*d = '\0';
			}
			return pszDesc;
		}
		p = pEnd ? pEnd + 1 : NULL;
	}
	return NULL;
}

char* IpsPatchDesc(const char* pszDat, INT32 nLang)
{
	char* pszText = TextFileLoad(pszDat);
	if (pszText == NULL) {
		return NULL;
	}
	// the language, English, then Simplified Chinese, the reference one
	char* pszDesc = DescByLanguage(pszText, nLang);
	if (pszDesc == NULL) pszDesc = DescByLanguage(pszText, 0);
	if (pszDesc == NULL) pszDesc = DescByLanguage(pszText, 1);
	free(pszText);

	return pszDesc;
}

// ----------------------------------------------------------------------------
// Patching

static void PatchFile(const char* ips_path, UINT8* base, bool readonly)
{
	char buf[6];
	FILE* f      = NULL;
	INT32 Offset = 0, Size = 0;
	UINT8* mem8  = NULL;

	if (NULL == (f = fopen(ips_path, "rb"))) {
		bprintf(0, _T("IPS - Can't open file %s!  Aborting.\n"), ips_path);
		return;
	}

	memset(buf, 0, sizeof(buf));
	fread(buf, 1, 5, f);
	if (strcmp(buf, IPS_SIGNATURE)) {
		bprintf(0, _T("IPS - Bad IPS-Signature in: %s.\n"), ips_path);
		fclose(f);
		return;
	} else {
		bprintf(0, _T("IPS - Patching with: %s. (%s)\n"), ips_path, (readonly) ? "Read-Only" : "Write");
		UINT8 ch   = 0;
		INT32 bRLE = 0;
		while (!feof(f)) {
			// read patch address offset
			fread(buf, 1, 3, f);
			buf[3] = 0;
			if (strcmp(buf, IPS_TAG_EOF) == 0)
				break;

			Offset = BYTE3_TO_UINT(buf);

			// read patch length
			fread(buf, 1, 2, f);
			Size = BYTE2_TO_UINT(buf);

			bRLE = (Size == 0);
			if (bRLE) {
				fread(buf, 1, 2, f);
				Size = BYTE2_TO_UINT(buf);
				ch   = fgetc(f);
			}

			while (Size--) {
				if (!readonly) mem8 = base + Offset + nRomOffset;
				Offset++;
				if (readonly) {
					if (!bRLE) fgetc(f);
				} else {
					*mem8 = bRLE ? ch : fgetc(f);
				}
			}
		}
	}

	// Avoid memory out-of-bounds due to ips offset greater than rom length.
	if (readonly && (0 == nIpsMemExpLen[EXP_FLAG])) {	// Unspecified length.
		nIpsMemExpLen[LOAD_ROM] = Offset;
	}

	fclose(f);
}

static char* stristr_int(const char* str1, const char* str2)
{
	const char* p1 = str1;
	const char* p2 = str2;
	const char* r = (!*p2) ? str1 : NULL;

	while (*p1 && *p2) {
		if (tolower((UINT8)*p1) == tolower((UINT8)*p2)) {
			if (!r) {
				r = p1;
			}
			p2++;
		} else {
			p2 = str2;
			if (r) {
				p1 = r + 1;
			}
			if (tolower((UINT8)*p1) == tolower((UINT8)*p2)) {
				r = p1;
				p2++;
			} else {
				r = NULL;
			}
		}
		p1++;
	}

	return (*p2) ? NULL : (char*)r;
}

static UINT32 hexto32(const char* s)
{
	UINT32 val = 0;
	char c;

	while ((c = *s++)) {
		UINT8 v = ((c & 0xf) + (c >> 6)) | ((c >> 3) & 0x8);
		val = (val << 4) | (UINT32)v;
	}

	return val;
}

// strtok() with the quoted strings as one token
static char* strqtoken(char* s, const char* delims)
{
	static char* prev_str = NULL;
	char* token = NULL;

	if (!s) {
		if (!prev_str) return NULL;
		s = prev_str;
	}

	s += strspn(s, delims);
	if (s[0] == '\0') {
		prev_str = NULL;
		return NULL;
	}

	if (s[0] == '\"') {
		token = ++s;
		if ((s = strpbrk(token, "\""))) {
			*(s++) = '\0';
		} else {
			s = token + strlen(token);
		}
	} else {
		token = s;
	}

	if ((s = strpbrk(s, delims))) {
		*(s++) = '\0';
		prev_str = s;
	} else {
		prev_str = NULL;					// the end of the line
	}

	return token;
}

static const char* PathSeparator(const char* path)
{
	if (!path || !*path)
		return "";

	char last = path[strlen(path) - 1];
	return (last != '\\' && last != '/') ? "/" : "";
}

static void DoPatchGame(const char* patch_name, const char* game_name, const UINT32 crc, UINT8* base, bool readonly)
{
	char* pszText = TextFileLoad(patch_name);
	if (pszText == NULL) {
		return;
	}

	const char* pszIpsPath = szAppIpsPath;
	const char* pszDrvName = BurnDrvGetTextA(DRV_NAME);
	bool bTarget = false;
	char* pszPos = pszText;
	char* p;

	while ((p = TextFileNextLine(&pszPos)) != NULL) {
		if (p[0] == '[')	// reached info-section of .dat file, time to leave.
			break;

		// Can support linetypes: (space or tab)
		// "rom name.bin" "patch file.ips" CRC(abcd1234)
		// romname.bin patchfile CRC(abcd1234)
#define DELIM_TOKENS_NAME " \t\r\n"
#define DELIM_TOKENS      " \t\r\n()"

		char* rom_name = strqtoken(p, DELIM_TOKENS_NAME);
		if (!rom_name || *rom_name == '#')
			continue;

		char* ips_name = strqtoken(NULL, DELIM_TOKENS_NAME);
		if (!ips_name)
			continue;

		UINT32 nIps_crc = 0;
		nRomOffset = 0;
		char* ips_offs = strqtoken(NULL, DELIM_TOKENS);
		if (ips_offs) {	// Parameters of the offset increment
			static const char* pszOffsets[] = { "IPS_OFFSET_016", "IPS_OFFSET_032", "IPS_OFFSET_048", "IPS_OFFSET_064", "IPS_OFFSET_080",
				"IPS_OFFSET_096", "IPS_OFFSET_112", "IPS_OFFSET_128", "IPS_OFFSET_144" };
			for (int i = 0; i < 9; i++) {
				if (strcmp(ips_offs, pszOffsets[i]) == 0) {
					nRomOffset = 0x1000000 * (i + 1);
				}
			}
			if (nRomOffset != 0) { // better get next token (crc)
				ips_offs = strqtoken(NULL, DELIM_TOKENS);
			}
		}

		if (ips_offs && stristr_int(ips_offs, "crc")) {
			char* ips_crc = strqtoken(NULL, DELIM_TOKENS);
			if (ips_crc) {
				nIps_crc = hexto32(ips_crc);
			}
		}

#undef DELIM_TOKENS_NAME
#undef DELIM_TOKENS

		char* has_ext = stristr_int(ips_name, IPS_EXT);

		if (_stricmp(rom_name, game_name))	// name don't match?
			if (nIps_crc != crc)			// crc don't match?
				continue;					// not our file. next!

		bTarget = true;

		if (!readonly) {
			bprintf(0, _T("ips name:[%s]\n"), ips_name);
			bprintf(0, _T("rom name:[%s]\n"), rom_name);
			bprintf(0, _T("rom crc :[%x]\n"), nIps_crc);
		}

		char ips_path[MAX_PATH * 2] = { 0 };
		bool bHasDir = strchr(ips_name, '\\') || strchr(ips_name, '/');
		bool bIsAbsolutePath = false;
		const char* pszExt = has_ext ? "" : IPS_EXT;

		// Windows, UNC or unix absolute paths
		if ((ips_name[0] == '\\' && ips_name[1] == '\\') || (ips_name[0] == '/' && ips_name[1] == '/')) {
			bIsAbsolutePath = true;
		} else if (ips_name[0] != '\0' && ips_name[1] == ':') {
			bIsAbsolutePath = ips_name[2] == '\\' || ips_name[2] == '/';
		} else if (ips_name[0] == '\\' || ips_name[0] == '/') {
			bIsAbsolutePath = true;
		}

		if (bIsAbsolutePath) {
			snprintf(ips_path, sizeof(ips_path), "%s%s", ips_name, pszExt);
		} else if (ips_name[0] == '.' && ips_name[1] == '.' && (ips_name[2] == '/' || ips_name[2] == '\\')) {
			// ../ : from the ips folder
			snprintf(ips_path, sizeof(ips_path), "%s%s%s%s", pszIpsPath, PathSeparator(pszIpsPath), ips_name + 3, pszExt);
		} else if (ips_name[0] == '.' && (ips_name[1] == '/' || ips_name[1] == '\\')) {
			// ./ : from the folder of the game
			snprintf(ips_path, sizeof(ips_path), "%s%s%s/%s%s", pszIpsPath, PathSeparator(pszIpsPath), pszDrvName, ips_name + 2, pszExt);
		} else if (bHasDir) {
			snprintf(ips_path, sizeof(ips_path), "%s%s%s%s", pszIpsPath, PathSeparator(pszIpsPath), ips_name, pszExt);
		} else {
			snprintf(ips_path, sizeof(ips_path), "%s%s%s/%s%s", pszIpsPath, PathSeparator(pszIpsPath), pszDrvName, ips_name, pszExt);
		}
		for (char* c = ips_path; *c; c++) {
			if (*c == '\\') *c = '/';
		}

		PatchFile(ips_path, base, readonly);
	}
	free(pszText);

	if (!bTarget && (0 == nIpsMemExpLen[EXP_FLAG])) {
		// Must be reset to 0!
		nIpsMemExpLen[LOAD_ROM] = 0;
	}
}

static UINT32 GetIpsDefineExpValue()
{
	char* pszTmp = strqtoken(NULL, " \t\r\n");
	if (NULL == pszTmp)
		return 0U;

	static const struct { const char* pszName; UINT32 nValue; } Values[] = {
		{ "EXP_VALUE_001", 0x0010000 }, { "EXP_VALUE_002", 0x0020000 }, { "EXP_VALUE_003", 0x0030000 }, { "EXP_VALUE_004", 0x0040000 },
		{ "EXP_VALUE_005", 0x0050000 }, { "EXP_VALUE_006", 0x0060000 }, { "EXP_VALUE_007", 0x0070000 }, { "EXP_VALUE_008", 0x0080000 },
		{ "EXP_VALUE_010", 0x0100000 }, { "EXP_VALUE_020", 0x0200000 }, { "EXP_VALUE_030", 0x0300000 }, { "EXP_VALUE_040", 0x0400000 },
		{ "EXP_VALUE_050", 0x0500000 }, { "EXP_VALUE_060", 0x0600000 }, { "EXP_VALUE_070", 0x0700000 }, { "EXP_VALUE_080", 0x0800000 },
		{ "EXP_VALUE_100", 0x1000000 }, { "EXP_VALUE_200", 0x2000000 }, { "EXP_VALUE_300", 0x3000000 }, { "EXP_VALUE_400", 0x4000000 },
		{ "EXP_VALUE_500", 0x5000000 }, { "EXP_VALUE_600", 0x6000000 }, { "EXP_VALUE_700", 0x7000000 }, { "EXP_VALUE_800", 0x8000000 },
	};
	for (unsigned int i = 0; i < sizeof(Values) / sizeof(Values[0]); i++) {
		if (strcmp(pszTmp, Values[i].pszName) == 0) {
			return Values[i].nValue;
		}
	}

	unsigned int nRet = 0;
	sscanf(pszTmp, "%x", &nRet);
	return nRet;
}

static void ExtraPatchesExit()
{
	IpsExtraPatch* pLists[2] = { &IpsEarly, &IpsLast };
	for (int l = 0; l < 2; l++) {
		for (UINT32 i = 0; i < pLists[l]->nCount; i++) {
			free(pLists[l]->pszDat[i]);
		}
		free(pLists[l]->pszDat);
		pLists[l]->pszDat = NULL;
		pLists[l]->nCount = 0;
	}
}

static void ExtraPatchAdd(IpsExtraPatch* pList, const char* pszDat)
{
	char** ppNew = (char**)realloc(pList->pszDat, (pList->nCount + 1) * sizeof(char*));
	if (ppNew == NULL) {
		return;
	}
	pList->pszDat = ppNew;
	pList->pszDat[pList->nCount] = (char*)malloc(strlen(pszDat) + 1);
	if (pList->pszDat[pList->nCount]) {
		strcpy(pList->pszDat[pList->nCount], pszDat);
		pList->nCount++;
	}
}

// #include "kof97" / "kof97.dat": the dats of the same game folder, before
// the patches of the dat if the line comes before them, after otherwise
static void ExtraPatchesInit()
{
	ExtraPatchesExit();

	const char* pszFolder = IpsPatchFolder(BurnDrvGetTextA(DRV_NAME));

	for (INT32 i = 0; i < nActivePatches; i++) {
		char* pszText = TextFileLoad(pszActivePatches[i]);
		if (pszText == NULL) {
			continue;
		}
		INT32 nNumbers = 0;
		char* pszPos = pszText;
		char* pszLine;
		while ((pszLine = TextFileNextLine(&pszPos)) != NULL) {
			char* tmp = strqtoken(pszLine, " \t\r\n");
			if (NULL == tmp)
				continue;
			if ('[' == tmp[0])
				break;
			if ('/' == tmp[0] && '/' == tmp[1])
				continue;
			if (strcmp(tmp, "#define") == 0)
				continue;
			if (strcmp(tmp, "#include") != 0) {
				nNumbers++;
				continue;
			}
			if (NULL == (tmp = strqtoken(NULL, " \t\r\n")))
				continue;
			// Only dat files in the same directory are allowed
			if (strchr(tmp, '/') || strchr(tmp, '\\'))
				continue;

			char szInclude[MAX_PATH * 2];
			snprintf(szInclude, sizeof(szInclude), strchr(tmp, '.') ? "%s%s" : "%s%s.dat", pszFolder, tmp);
			if (!FileExists(szInclude))
				continue;

			ExtraPatchAdd((nNumbers > 0) ? &IpsLast : &IpsEarly, szInclude);
		}
		free(pszText);
	}
}

// all the dats: the early includes, the active ones, the late includes
static INT32 PatchDatCount()
{
	return IpsEarly.nCount + nActivePatches + IpsLast.nCount;
}

static const char* PatchDat(INT32 i)
{
	if (i < (INT32)IpsEarly.nCount) {
		return IpsEarly.pszDat[i];
	}
	i -= IpsEarly.nCount;
	if (i < nActivePatches) {
		return pszActivePatches[i];
	}
	return IpsLast.pszDat[i - nActivePatches];
}

// Run once to get the definition & definition values of the DAT files.
static void GetIpsDrvDefine()
{
	if (!bDoIpsPatch)
		return;

	nIpsDrvDefine = 0;
	memset(nIpsMemExpLen, 0, sizeof(nIpsMemExpLen));

	ExtraPatchesInit();

	static const struct { const char* pszName; UINT32 nFlag; } Flags[] = {
		{ "IPS_NOT_PROTECT", IPS_NOT_PROTECT }, { "IPS_PGM_SPRHACK", IPS_PGM_SPRHACK }, { "IPS_PGM_MAPHACK", IPS_PGM_MAPHACK },
		{ "IPS_PGM_SNDOFFS", IPS_PGM_SNDOFFS }, { "IPS_SNES_VRAMHK", IPS_SNES_VRAMHK }, { "IPS_NEO_RAMHACK", IPS_NEO_RAMHACK },
	};
	static const struct { const char* pszName; UINT32 nFlag; INT32 nLen; } Expands[] = {
		{ "IPS_LOAD_EXPAND", IPS_LOAD_EXPAND, LOAD_ROM }, { "IPS_EXTROM_INCL", IPS_EXTROM_INCL, EXTR_ROM },
		{ "IPS_PRG1_EXPAND", IPS_PRG1_EXPAND, PRG1_ROM }, { "IPS_PRG2_EXPAND", IPS_PRG2_EXPAND, PRG2_ROM },
		{ "IPS_GRA1_EXPAND", IPS_GRA1_EXPAND, GRA1_ROM }, { "IPS_GRA2_EXPAND", IPS_GRA2_EXPAND, GRA2_ROM },
		{ "IPS_GRA3_EXPAND", IPS_GRA3_EXPAND, GRA3_ROM }, { "IPS_ACPU_EXPAND", IPS_ACPU_EXPAND, ACPU_ROM },
		{ "IPS_SND1_EXPAND", IPS_SND1_EXPAND, SND1_ROM }, { "IPS_SND2_EXPAND", IPS_SND2_EXPAND, SND2_ROM },
	};

	for (INT32 i = 0; i < PatchDatCount(); i++) {
		char* pszText = TextFileLoad(PatchDat(i));
		if (pszText == NULL) {
			continue;
		}
		char* pszPos = pszText;
		char* pszLine;
		while ((pszLine = TextFileNextLine(&pszPos)) != NULL) {
			char* tmp = strqtoken(pszLine, " \t\r\n");
			if (NULL == tmp)
				continue;
			if ('[' == tmp[0])
				break;
			if (strcmp(tmp, "#define") != 0)
				continue;
			if (NULL == (tmp = strqtoken(NULL, " \t\r\n")))
				continue;

			for (unsigned int f = 0; f < sizeof(Flags) / sizeof(Flags[0]); f++) {
				if (strcmp(tmp, Flags[f].pszName) == 0) {
					nIpsDrvDefine |= Flags[f].nFlag;
				}
			}
			for (unsigned int e = 0; e < sizeof(Expands) / sizeof(Expands[0]); e++) {
				if (strcmp(tmp, Expands[e].pszName) == 0) {
					nIpsDrvDefine |= Expands[e].nFlag;
					if (Expands[e].nLen == LOAD_ROM) {
						nIpsMemExpLen[EXP_FLAG] = 1;
					}
					UINT32 nNewValue = GetIpsDefineExpValue();
					if (nNewValue > nIpsMemExpLen[Expands[e].nLen]) {
						nIpsMemExpLen[Expands[e].nLen] = nNewValue;
					}
				}
			}
		}
		free(pszText);
	}
}

void IpsApplyPatches(UINT8* base, char* rom_name, UINT32 crc, bool readonly)
{
	if (!bDoIpsPatch)
		return;

	for (INT32 i = 0; i < PatchDatCount(); i++) {
		DoPatchGame(PatchDat(i), rom_name, crc, base, readonly);
	}
}

void IpsPatchInit()
{
	GetIpsDrvDefine();
}

void IpsPatchExit()
{
	ExtraPatchesExit();
	memset(nIpsMemExpLen, 0, sizeof(nIpsMemExpLen));

	nIpsDrvDefine = 0;
	bDoIpsPatch   = false;
}
