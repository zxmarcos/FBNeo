// The texts of history.xml and command.dat for a driver (Game Info), the
// parsers of win32/gameinfo.cpp without the RTF
#include "burner.h"
#include "gameinfo_text.h"

struct TextBuffer {
	char* pszText;
	int nLen;
	int nSize;
};

static void BufferAdd(TextBuffer* b, const char* pszText, int nLen)
{
	if (nLen < 0) {
		nLen = strlen(pszText);
	}
	if (b->nLen + nLen + 1 > b->nSize) {
		int nNewSize = (b->nLen + nLen + 1) * 2;
		if (nNewSize < 4096) nNewSize = 4096;
		char* pszNew = (char*)realloc(b->pszText, nNewSize);
		if (pszNew == NULL) {
			return;
		}
		b->pszText = pszNew;
		b->nSize = nNewSize;
	}
	memcpy(b->pszText + b->nLen, pszText, nLen);
	b->nLen += nLen;
	b->pszText[b->nLen] = '\0';
}

static void BufferAddChar(TextBuffer* b, unsigned int c)
{
	char s[4];
	int n = 0;
	if (c < 0x80) {
		s[n++] = (char)c;
	} else if (c < 0x800) {
		s[n++] = (char)(0xC0 | (c >> 6));
		s[n++] = (char)(0x80 | (c & 0x3F));
	} else {
		s[n++] = (char)(0xE0 | (c >> 12));
		s[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
		s[n++] = (char)(0x80 | (c & 0x3F));
	}
	BufferAdd(b, s, n);
}

// the text, or NULL if nothing was added
static char* BufferEnd(TextBuffer* b)
{
	// no blank lines at the end
	while (b->nLen > 1 && b->pszText[b->nLen - 1] == '\n' && b->pszText[b->nLen - 2] == '\n') {
		b->pszText[--b->nLen] = '\0';
	}
	if (b->nLen == 0) {
		free(b->pszText);
		return NULL;
	}
	return b->pszText;
}

static void StripLineEnd(char* pszLine)
{
	int nLen = strlen(pszLine);
	while (nLen > 0 && (pszLine[nLen - 1] == '\n' || pszLine[nLen - 1] == '\r')) {
		pszLine[--nLen] = '\0';
	}
}

#define LINE_SIZE 16384

// ----------------------------------------------------------------------------
// history.xml: <system name="sf2"/> (or <item list="nes" name="smb"/> for the
// consoles) lines, then the entry between <text> and </text>

static void AddXmlLine(TextBuffer* b, const char* pszLine)
{
	static const struct { const char* pszEntity; char c; } Entities[] = {
		{ "&quot;", '"' }, { "&amp;", '&' }, { "&apos;", '\'' }, { "&gt;", '>' }, { "&lt;", '<' },
	};

	for (const char* p = pszLine; *p; ) {
		bool bEntity = false;
		if (p[0] == '&' && p[1] == '#') {
			// &#169; or &#xA9;
			char* pEnd = NULL;
			unsigned long c = (p[2] == 'x' || p[2] == 'X') ? strtoul(p + 3, &pEnd, 16) : strtoul(p + 2, &pEnd, 10);
			if (pEnd && *pEnd == ';' && c > 0 && c <= 0xFFFF) {
				BufferAddChar(b, (unsigned int)c);
				p = pEnd + 1;
				continue;
			}
		}
		if (*p == '&') {
			for (unsigned int i = 0; i < sizeof(Entities) / sizeof(Entities[0]); i++) {
				int nLen = strlen(Entities[i].pszEntity);
				if (strncmp(p, Entities[i].pszEntity, nLen) == 0) {
					BufferAdd(b, &Entities[i].c, 1);
					p += nLen;
					bEntity = true;
					break;
				}
			}
		}
		if (!bEntity) {
			BufferAdd(b, p, 1);
			p++;
		}
	}
	BufferAdd(b, "\n", 1);
}

char* GameInfoHistory(int nDrv)
{
	char szFile[MAX_PATH * 2];
	snprintf(szFile, sizeof(szFile), "%shistory.xml", szAppHistoryPath);
	FILE* fp = fopen(szFile, "rt");
	if (fp == NULL) {
		return NULL;
	}

	UINT32 nOldDrv = nBurnDrvActive;
	nBurnDrvActive = nDrv;

	char szToken[256];
	char szName[256];
	GetHistoryDatHardwareToken(szToken);
	snprintf(szName, sizeof(szName), "%s", BurnDrvGetTextA(DRV_NAME));
	if (strncmp("\t\t\t<system name=\"", szToken, 17)) {
		// not an arcade game: "nes_smb" is "smb" in the list of the system
		char* p = strchr(szName, '_');
		if (p) memmove(szName, p + 1, strlen(p));
	}
	nBurnDrvActive = nOldDrv;

	char szFind[512];
	snprintf(szFind, sizeof(szFind), "%s%s\"", szToken, szName);
	int nFind = strlen(szFind);

	TextBuffer b = { NULL, 0, 0 };
	char* pszLine = (char*)malloc(LINE_SIZE);
	int nState = 0;						// 1: the game was found, 2: in its text

	while (pszLine && fgets(pszLine, LINE_SIZE, fp)) {
		if (nState == 0) {
			if (strncmp(szFind, pszLine, nFind) == 0) {
				nState = 1;
			}
			continue;
		}
		if (nState == 1) {
			if (strstr(pszLine, "<text>")) {
				nState = 2;
			}
			continue;
		}
		if (strstr(pszLine, "</text>")) {
			break;
		}
		StripLineEnd(pszLine);
		if (b.nLen == 0 && pszLine[0] == '\0') {
			continue;						// the title is the first line with something
		}
		AddXmlLine(&b, pszLine);
	}

	free(pszLine);
	fclose(fp);

	return BufferEnd(&b);
}

// ----------------------------------------------------------------------------
// command.dat: "$info=name,name2," then the lines up to "$end". The codes of
// the buttons and the moves become symbols (check_expands() of the Win32 build).

struct CommandCode {
	const char* pszCode;
	unsigned int nChar;				// in place of the \x01 of pszText, " \x01 " if pszText is NULL
	const char* pszText;
};

static const CommandCode CommandCodes[] = {
	{ "_A", 0x24B6, NULL }, { "_B", 0x24B7, NULL }, { "_C", 0x24B8, NULL }, { "_D", 0x24B9, NULL },	// circled A-D
	{ "_H", 0x24BD, NULL }, { "_Z", 0x24CF, NULL },

	{ "_a", 0x2460, NULL }, { "_b", 0x2461, NULL }, { "_c", 0x2462, NULL }, { "_d", 0x2463, NULL },	// circled 1-10
	{ "_e", 0x2464, NULL }, { "_f", 0x2465, NULL }, { "_g", 0x2466, NULL }, { "_h", 0x2467, NULL },
	{ "_i", 0x2468, NULL }, { "_j", 0x2469, NULL },

	{ "_k", 0, " [Half Circle Back] " },
	{ "_l", 0, " [Half Circle Front Up] " },
	{ "_m", 0, " [Half Circle Front] " },
	{ "_n", 0, " [Half Circle Back Up] " },
	{ "_o", 0, " [1/4 Circle Forward 2 Down] " },
	{ "_p", 0, " [1/4 Circle Down 2 Back] " },
	{ "_q", 0, " [1/4 Circle Back 2 Up] " },
	{ "_r", 0, " [1/4 Circle Up 2 Forward] " },
	{ "_s", 0, " [1/4 Circle Back 2 Down] " },
	{ "_t", 0, " [1/4 Circle Down 2 Forward] " },
	{ "_u", 0, " [1/4 Circle Forward 2 Up] " },
	{ "_v", 0, " [1/4 Circle Up 2 Back] " },
	{ "_w", 0, " [Full Clock Forward] " },
	{ "_x", 0, " [Full Clock Back] " },
	{ "_y", 0, " [Full Count Forward] " },
	{ "_z", 0, " [Full Count Back] " },
	{ "_L", 0, " [2x Forward] " },
	{ "_M", 0, " [2x Back] " },
	{ "_Q", 0, " [Dragon Screw Forward] " },
	{ "_R", 0, " [Dragon Screw Back] " },

	{ "_S", 0, "[Start]" },
	{ "_P", 0, "[Punch]" },
	{ "_K", 0, "[Kick]" },
	{ "_G", 0, "[Guard]" },
	{ "^S", 0, "[Select]" },
	{ "^s", 0x24C8, NULL },			// circled S

	{ "@A-button", 0x24B6, NULL }, { "@B-button", 0x24B7, NULL }, { "@C-button", 0x24B8, NULL },	// circled A-Z
	{ "@D-button", 0x24B9, NULL }, { "@E-button", 0x24BA, NULL }, { "@F-button", 0x24BB, NULL },
	{ "@G-button", 0x24BC, NULL }, { "@H-button", 0x24BD, NULL }, { "@I-button", 0x24BE, NULL },
	{ "@J-button", 0x24BF, NULL }, { "@K-button", 0x24C0, NULL }, { "@L-button", 0x24C1, NULL },
	{ "@M-button", 0x24C2, NULL }, { "@N-button", 0x24C3, NULL }, { "@O-button", 0x24C4, NULL },
	{ "@P-button", 0x24C5, NULL }, { "@Q-button", 0x24C6, NULL }, { "@R-button", 0x24C7, NULL },
	{ "@S-button", 0x24C8, NULL }, { "@T-button", 0x24C9, NULL }, { "@U-button", 0x24CA, NULL },
	{ "@V-button", 0x24CB, NULL }, { "@W-button", 0x24CC, NULL }, { "@X-button", 0x24CD, NULL },
	{ "@Y-button", 0x24CE, NULL }, { "@Z-button", 0x24CF, NULL },

	{ "_N", 0, "N" },
	{ "_`", 0, "." },
	{ "_)", 0x25CB, NULL },			// circle
	{ "_@", 0x25CE, NULL },			// bullseye
	{ "_(", 0x25CF, NULL },			// black circle
	{ "_&", 0x2605, NULL },			// black star
	{ "_*", 0x2606, NULL },			// white star
	{ "_#", 0x25A3, NULL },			// square in a square
	{ "_]", 0x25A1, NULL },			// square
	{ "_[", 0x25A0, NULL },			// black square
	{ "_%", 0x25B3, NULL },			// triangle
	{ "_$", 0x25B2, NULL },			// black triangle
	{ "_<", 0x25C6, NULL },			// black diamond
	{ "_>", 0x25C7, NULL },			// diamond

	{ "_+", 0, " + " },
	{ "_^", 0, "[air]" },
	{ "_?", 0, "[dir]" },
	{ "_X", 0, "[tap]" },
	{ "_|", 0, "[Jump]" },
	{ "_O", 0, "[Hold]" },
	{ "_-", 0, "[Air]" },
	{ "_=", 0, "[Squatting]" },
	{ "_~", 0, "[Charge]" },
	{ "_!", 0x21C9, NULL },			// "continue" arrows

	{ "_9", 0x2197, NULL }, { "_8", 0x2191, NULL }, { "_7", 0x2196, NULL },	// arrows
	{ "_6", 0x2192, NULL }, { "_5", 0x25CF, NULL }, { "_4", 0x2190, NULL },
	{ "_3", 0x2198, NULL }, { "_2", 0x2193, NULL }, { "_1", 0x2199, NULL },
	{ "_.", 0x2026, NULL },			// "..."

	{ "^9", 0x21D7, " \x01 . " }, { "^8", 0x21D1, " \x01 . " }, { "^7", 0x21D6, " \x01 . " },	// double arrows
	{ "^6", 0x21D2, " \x01 . " }, { "^4", 0x21D0, " \x01 . " }, { "^3", 0x21D8, " \x01 . " },
	{ "^2", 0x21D3, " \x01 . " }, { "^1", 0x21D9, " \x01 . " },

	{ "^-", 0, " [Close] " },
	{ "^=", 0, " [Away] " },
	{ "^*", 0, " [Spam Button] " },
	{ "^?", 0, " [Any Button] " },

	{ "^E", 0, "[LP]" },
	{ "^F", 0, "[MP]" },
	{ "^G", 0, "[SP]" },
	{ "^H", 0, "[LK]" },
	{ "^I", 0, "[MK]" },
	{ "^J", 0, "[SK]" },
	{ "^T", 0, "[3x Kick]" },
	{ "^U", 0, "[3x Punch]" },
	{ "^V", 0, "[2x Kick]" },
	{ "^W", 0, "[2x Punch]" },
	{ "^!", 0x21B3, NULL },			// down then right arrow
	{ "^M", 0, " MAX " },
};

static void AddCommandLine(TextBuffer* b, const char* pszLine)
{
	for (const char* p = pszLine; *p; ) {
		const CommandCode* pCode = NULL;
		if (*p == '_' || *p == '^' || *p == '@') {
			for (unsigned int i = 0; i < sizeof(CommandCodes) / sizeof(CommandCodes[0]); i++) {
				if (strncmp(p, CommandCodes[i].pszCode, strlen(CommandCodes[i].pszCode)) == 0) {
					pCode = &CommandCodes[i];
					break;
				}
			}
		}
		if (pCode == NULL) {
			BufferAdd(b, p, 1);
			p++;
			continue;
		}

		const char* pszText = pCode->pszText ? pCode->pszText : " \x01 ";
		for (const char* t = pszText; *t; t++) {
			if (*t == '\x01') {
				BufferAddChar(b, pCode->nChar);
			} else {
				BufferAdd(b, t, 1);
			}
		}
		p += strlen(pCode->pszCode);
	}
	BufferAdd(b, "\n", 1);
}

char* GameInfoCommands(int nDrv)
{
	char szFile[MAX_PATH * 2];
	snprintf(szFile, sizeof(szFile), "%scommand.dat", szAppCommandPath);
	FILE* fp = fopen(szFile, "rt");
	if (fp == NULL) {
		return NULL;
	}

	UINT32 nOldDrv = nBurnDrvActive;
	nBurnDrvActive = nDrv;
	char szName[256];
	snprintf(szName, sizeof(szName), "%s", BurnDrvGetTextA(DRV_NAME));
	nBurnDrvActive = nOldDrv;

	TextBuffer b = { NULL, 0, 0 };
	char* pszLine = (char*)malloc(LINE_SIZE);
	bool bFound = false;

	while (pszLine && fgets(pszLine, LINE_SIZE, fp)) {
		StripLineEnd(pszLine);
		if (!bFound) {
			if (strncmp("$info=", pszLine, 6) == 0) {
				// the names the entry is for
				for (char* pszToken = strtok(pszLine + 6, ","); pszToken; pszToken = strtok(NULL, ",")) {
					if (strcmp(pszToken, szName) == 0) {
						bFound = true;
						break;
					}
				}
			}
			continue;
		}
		if (strncmp("$end", pszLine, 4) == 0) {
			break;
		}
		if (pszLine[0] == '$') {
			continue;						// $cmd and the other markers
		}
		if (b.nLen == 0 && pszLine[0] == '\0') {
			continue;
		}
		AddCommandLine(&b, pszLine);
	}

	free(pszLine);
	fclose(fp);

	return BufferEnd(&b);
}
