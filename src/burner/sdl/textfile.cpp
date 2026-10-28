// Text files of any encoding (ANSI, UTF-8 with or without a BOM, UTF-16 LE
// or BE) as UTF-8 in memory, for the IPS and RomData dats, which the Win32
// build reads through AdaptiveEncodingReads()
#include "burner.h"

static int Utf8Put(char* pszOut, UINT32 c)
{
	if (c < 0x80) {
		pszOut[0] = (char)c;
		return 1;
	}
	if (c < 0x800) {
		pszOut[0] = (char)(0xC0 | (c >> 6));
		pszOut[1] = (char)(0x80 | (c & 0x3F));
		return 2;
	}
	if (c < 0x10000) {
		pszOut[0] = (char)(0xE0 | (c >> 12));
		pszOut[1] = (char)(0x80 | ((c >> 6) & 0x3F));
		pszOut[2] = (char)(0x80 | (c & 0x3F));
		return 3;
	}
	pszOut[0] = (char)(0xF0 | (c >> 18));
	pszOut[1] = (char)(0x80 | ((c >> 12) & 0x3F));
	pszOut[2] = (char)(0x80 | ((c >> 6) & 0x3F));
	pszOut[3] = (char)(0x80 | (c & 0x3F));
	return 4;
}

char* TextFileLoad(const char* pszFile)
{
	FILE* fp = fopen(pszFile, "rb");
	if (fp == NULL) {
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	long nSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (nSize < 0) {
		fclose(fp);
		return NULL;
	}

	UINT8* pData = (UINT8*)malloc(nSize + 2);
	if (pData == NULL) {
		fclose(fp);
		return NULL;
	}
	long nRead = (long)fread(pData, 1, nSize, fp);
	fclose(fp);
	pData[nRead] = pData[nRead + 1] = 0;

	bool bLE = nRead >= 2 && pData[0] == 0xFF && pData[1] == 0xFE;
	bool bBE = nRead >= 2 && pData[0] == 0xFE && pData[1] == 0xFF;
	if (!bLE && !bBE) {
		if (nRead >= 3 && pData[0] == 0xEF && pData[1] == 0xBB && pData[2] == 0xBF) {
			memmove(pData, pData + 3, nRead - 3 + 1);
		}
		return (char*)pData;
	}

	// UTF-16, with the surrogate pairs
	char* pszText = (char*)malloc((nRead / 2) * 3 + 4);
	if (pszText == NULL) {
		free(pData);
		return NULL;
	}
	int n = 0;
	for (long i = 2; i + 1 < nRead; i += 2) {
		UINT32 c = bLE ? (pData[i] | (pData[i + 1] << 8)) : ((pData[i] << 8) | pData[i + 1]);
		if (c >= 0xD800 && c < 0xDC00 && i + 3 < nRead) {
			UINT32 c2 = bLE ? (pData[i + 2] | (pData[i + 3] << 8)) : ((pData[i + 2] << 8) | pData[i + 3]);
			if (c2 >= 0xDC00 && c2 < 0xE000) {
				c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00);
				i += 2;
			}
		}
		n += Utf8Put(pszText + n, c);
	}
	pszText[n] = '\0';
	free(pData);

	return pszText;
}

char* TextFileNextLine(char** ppszPos)
{
	char* pszLine = *ppszPos;
	if (pszLine == NULL || *pszLine == '\0') {
		return NULL;
	}

	char* p = pszLine;
	while (*p && *p != '\n') {
		p++;
	}
	*ppszPos = (*p == '\n') ? p + 1 : p;
	*p = '\0';
	if (p > pszLine && p[-1] == '\r') {
		p[-1] = '\0';
	}

	return pszLine;
}
