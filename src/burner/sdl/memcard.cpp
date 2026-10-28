// Memory card support (Neo Geo memory cards, PGM2 IC cards), the engine of
// win32/memcard.cpp with the file names given by the interface
#include "burner.h"

TCHAR szMemoryCardFile[MAX_PATH] = _T("");
int nMemoryCardStatus = 0;					// & 1 = file selected, & 2 = inserted
static int nMemoryCardSize;

static int nMinVersion;
static bool bMemCardFC1Format;

bool MemCardSupported()
{
	if (!bDrvOkay) {
		return false;
	}

	UINT32 nHardware = BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK;
	return nHardware == HARDWARE_SNK_NEOGEO || nHardware == HARDWARE_IGS_PGM2;
}

// the folder of the cards, with the trailing slash
const TCHAR* MemCardFolder()
{
	static TCHAR szFolder[MAX_PATH] = _T("");

	if (szFolder[0] == _T('\0')) {
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
		char* pszPath = SDL_GetPrefPath("fbneo", "memcards");
		_stprintf(szFolder, _T("%s"), pszPath);
		SDL_free(pszPath);
#else
		_stprintf(szFolder, _T("config/memcards/"));
#ifdef _WIN32
		CreateDirectoryA("config", NULL);
		CreateDirectoryA("config/memcards", NULL);
#endif
#endif
	}

	return szFolder;
}

static int MemCardRead(const TCHAR* szFilename, unsigned char* pData, int nSize)
{
	const char* szHeader  = "FB1 FC1 ";				// File + chunk identifier
	char szReadHeader[8] = "";

	bMemCardFC1Format = false;

	FILE* fp = _tfopen(szFilename, _T("rb"));
	if (fp == NULL) {
		return 1;
	}

	fread(szReadHeader, 1, 8, fp);					// Read identifiers
	if (memcmp(szReadHeader, szHeader, 8) == 0) {

		// FB Alpha memory card file

		int nChunkSize = 0;
		int nVersion = 0;

		bMemCardFC1Format = true;

		fread(&nChunkSize, 1, 4, fp);				// Read chunk size
		if (nSize < nChunkSize - 32) {
			fclose(fp);
			return 1;
		}

		fread(&nVersion, 1, 4, fp);					// Read version
		if (nVersion < nMinVersion) {
			fclose(fp);
			return 1;
		}
		fread(&nVersion, 1, 4, fp);

		fseek(fp, 0x0C, SEEK_CUR);					// Move file pointer to the start of the data block

		fread(pData, 1, nChunkSize - 32, fp);		// Read the data
	} else {

		// Check for raw binary card file (MAME PGM2 SLE4442 format: 264 or 256 bytes)
		fseek(fp, 0, SEEK_END);
		long nFileSize = ftell(fp);
		fseek(fp, 0x00, SEEK_SET);

		if (nFileSize == 0x108 || nFileSize == 0x100) {
			// Raw binary card: read directly
			memset(pData, 0xFF, nSize);
			int nReadSize = (nFileSize <= nSize) ? (int)nFileSize : nSize;
			fread(pData, 1, nReadSize, fp);
			// If only 256 bytes (main data), init protection/security to defaults
			if (nFileSize == 0x100 && nSize >= 0x108) {
				memset(pData + 0x100, 0xFF, 4);		// protection: all writable
				pData[0x104] = 0x07;				// sec[0]: 3 auth attempts
				pData[0x105] = 0xFF;				// sec[1]: default PSC
				pData[0x106] = 0xFF;				// sec[2]: default PSC
				pData[0x107] = 0xFF;				// sec[3]: default PSC
			}
		} else {
			// MAME Neo Geo or old FB Alpha memory card file (half-size interleaved)
			unsigned char* pTemp = (unsigned char*)malloc(nSize >> 1);

			memset(pData, 0, nSize);

			if (pTemp) {
				fread(pTemp, 1, nSize >> 1, fp);

				for (int i = 1; i < nSize; i += 2) {
					pData[i] = pTemp[i >> 1];
				}

				free(pTemp);
			}
		}
	}

	fclose(fp);

	return 0;
}

static int MemCardWrite(const TCHAR* szFilename, unsigned char* pData, int nSize)
{
	FILE* fp = _tfopen(szFilename, _T("wb"));
	if (fp == NULL) {
		return 1;
	}

	// a .bin file is written raw (MAME compatible)
	int nLen = (int)_tcslen(szFilename);
	bool bRawBin = nLen >= 4 && _tcsicmp(szFilename + nLen - 4, _T(".bin")) == 0;

	if (bRawBin) {
		fwrite(pData, 1, nSize, fp);
	} else if (bMemCardFC1Format) {

		// FB Alpha memory card file

		const char* szFileHeader  = "FB1 ";				// File identifier
		const char* szChunkHeader = "FC1 ";				// Chunk identifier
		const int nZero = 0;

		int nChunkSize = nSize + 32;

		fwrite(szFileHeader, 1, 4, fp);
		fwrite(szChunkHeader, 1, 4, fp);

		fwrite(&nChunkSize, 1, 4, fp);					// Chunk size

		fwrite(&nBurnVer, 1, 4, fp);					// Version of FBA this was saved from
		fwrite(&nMinVersion, 1, 4, fp);					// Min version of FBA data will work with

		fwrite(&nZero, 1, 4, fp);						// Reserved
		fwrite(&nZero, 1, 4, fp);						//
		fwrite(&nZero, 1, 4, fp);						//

		fwrite(pData, 1, nSize, fp);
	} else {

		// MAME or old FB Alpha memory card file

		unsigned char* pTemp = (unsigned char*)malloc(nSize >> 1);
		if (pTemp) {
			for (int i = 1; i < nSize; i += 2) {
				pTemp[i >> 1] = pData[i];
			}

			fwrite(pTemp, 1, nSize >> 1, fp);

			free(pTemp);
		}
	}

	fclose(fp);

	return 0;
}

static int __cdecl MemCardDoGetSize(struct BurnArea* pba)
{
	nMemoryCardSize = pba->nLen;

	return 0;
}

static int MemCardGetSize()
{
	nMemoryCardSize = 0;
	BurnAcb = MemCardDoGetSize;
	BurnAreaScan(ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);

	return 0;
}

int MemCardCreate(const TCHAR* pszFile)
{
	MemCardGetSize();
	if (nMemoryCardSize <= 0) {
		return 1;
	}

	unsigned char* pCard = (unsigned char*)calloc(nMemoryCardSize, 1);
	if (pCard == NULL) {
		return 1;
	}

	bMemCardFC1Format = true;
	int nRet = MemCardWrite(pszFile, pCard, nMemoryCardSize);
	free(pCard);
	if (nRet) {
		return 1;
	}

	_tcscpy(szMemoryCardFile, pszFile);
	nMemoryCardStatus = 1;

	return 0;
}

int MemCardSelect(const TCHAR* pszFile)
{
	FILE* fp = _tfopen(pszFile, _T("rb"));
	if (fp == NULL) {
		return 1;
	}
	fclose(fp);

	MemCardGetSize();
	if (nMemoryCardSize <= 0) {
		return 1;
	}

	_tcscpy(szMemoryCardFile, pszFile);
	nMemoryCardStatus = 1;

	return 0;
}

static int __cdecl MemCardDoInsert(struct BurnArea* pba)
{
	if (MemCardRead(szMemoryCardFile, (unsigned char*)pba->Data, pba->nLen)) {
		return 1;
	}

	nMemoryCardStatus |= 2;

	return 0;
}

int MemCardInsert()
{
	if ((nMemoryCardStatus & 1) && (nMemoryCardStatus & 2) == 0) {
		BurnAcb = MemCardDoInsert;
		BurnAreaScan(ACB_WRITE | ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);
	}

	return (nMemoryCardStatus & 2) ? 0 : 1;
}

static int __cdecl MemCardDoEject(struct BurnArea* pba)
{
	if (MemCardWrite(szMemoryCardFile, (unsigned char*)pba->Data, pba->nLen) == 0) {
		nMemoryCardStatus &= ~2;

		return 0;
	}

	return 1;
}

int MemCardEject()
{
	if ((nMemoryCardStatus & 1) && (nMemoryCardStatus & 2)) {
		BurnAcb = MemCardDoEject;
		nMinVersion = 0;
		BurnAreaScan(ACB_READ | ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);
	}

	return 0;
}

int MemCardToggle()
{
	if (nMemoryCardStatus & 1) {
		if (nMemoryCardStatus & 2) {
			return MemCardEject();
		} else {
			return MemCardInsert();
		}
	}

	return 1;
}

// ----------------------------------------------------------------------------
// PGM2 card slots

extern INT32 Pgm2MaxCardSlots;
extern INT32 Pgm2ActiveCardSlot;
INT32 pgm2GetCardRomTemplate(UINT8* buffer, INT32 maxSize);

TCHAR szPgm2CardFile[4][MAX_PATH];
int nPgm2CardStatus[4] = { 0, 0, 0, 0 };	// per slot: & 1 = file selected, & 2 = inserted

int MemCardPgm2Slots()
{
	return (bDrvOkay && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_IGS_PGM2) ? Pgm2MaxCardSlots : 0;
}

int MemCardCreatePGM2Slot(int nSlot, const TCHAR* pszFile)
{
	if (nSlot < 0 || nSlot >= 4) return 1;

	// a fresh card from the template of the game's roms
	unsigned char cardData[0x108];
	int nLen = pgm2GetCardRomTemplate(cardData, sizeof(cardData));
	if (nLen <= 0) {
		// blank SLE4442 card
		memset(cardData, 0xFF, sizeof(cardData));
		cardData[0x104] = 0x07;
		nLen = 0x108;
	}

	FILE* fp = _tfopen(pszFile, _T("wb"));
	if (fp == NULL) return 1;
	fwrite(cardData, 1, nLen, fp);
	fclose(fp);

	_tcscpy(szPgm2CardFile[nSlot], pszFile);
	nPgm2CardStatus[nSlot] = 1;

	return 0;
}

int MemCardSelectPGM2Slot(int nSlot, const TCHAR* pszFile)
{
	if (nSlot < 0 || nSlot >= 4) return 1;

	FILE* fp = _tfopen(pszFile, _T("rb"));
	if (fp == NULL) return 1;
	fclose(fp);

	Pgm2ActiveCardSlot = nSlot;
	MemCardGetSize();
	if (nMemoryCardSize <= 0) {
		return 1;
	}

	_tcscpy(szPgm2CardFile[nSlot], pszFile);
	nPgm2CardStatus[nSlot] = 1;

	return 0;
}

static int __cdecl MemCardDoInsertSlot(struct BurnArea* pba)
{
	int nSlot = Pgm2ActiveCardSlot;
	if (nSlot < 0 || nSlot >= 4) return 1;

	if (MemCardRead(szPgm2CardFile[nSlot], (unsigned char*)pba->Data, pba->nLen)) {
		return 1;
	}

	nPgm2CardStatus[nSlot] |= 2;

	return 0;
}

int MemCardInsertPGM2Slot(int nSlot)
{
	if (nSlot < 0 || nSlot >= 4) return 1;
	if (!(nPgm2CardStatus[nSlot] & 1)) return 1;	// no file selected
	if (nPgm2CardStatus[nSlot] & 2) return 1;		// already inserted

	Pgm2ActiveCardSlot = nSlot;
	BurnAcb = MemCardDoInsertSlot;
	nMinVersion = 0;
	BurnAreaScan(ACB_WRITE | ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);

	return 0;
}

static int __cdecl MemCardDoEjectSlot(struct BurnArea* pba)
{
	int nSlot = Pgm2ActiveCardSlot;
	if (nSlot < 0 || nSlot >= 4) return 1;

	if (MemCardWrite(szPgm2CardFile[nSlot], (unsigned char*)pba->Data, pba->nLen) == 0) {
		nPgm2CardStatus[nSlot] &= ~2;
	}

	return 0;
}

int MemCardEjectPGM2Slot(int nSlot)
{
	if (nSlot < 0 || nSlot >= 4) return 1;
	if (!(nPgm2CardStatus[nSlot] & 1)) return 1;
	if (!(nPgm2CardStatus[nSlot] & 2)) return 1;	// not inserted

	Pgm2ActiveCardSlot = nSlot;
	BurnAcb = MemCardDoEjectSlot;
	nMinVersion = 0;
	BurnAreaScan(ACB_READ | ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);

	return 0;
}

// the cards are written back when the game ends
void MemCardExit()
{
	MemCardEject();
	nMemoryCardStatus = 0;

	for (int i = 0; i < 4; i++) {
		MemCardEjectPGM2Slot(i);
		nPgm2CardStatus[i] = 0;
	}
}
