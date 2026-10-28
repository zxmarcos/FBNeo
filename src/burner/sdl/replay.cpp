// Recording and playback of the inputs (.fr), the engine of win32/replay.cpp
// with the same file format. The dialogs are in ui/ui_record.cpp: a recording
// from power-on restarts the game first and is started once it runs again.
#include "burner.h"
#include "inputbuf.h"
#ifdef BUILD_SDL2_IMGUI
#include "ui.h"
#endif

#define MAX_METADATA		1024

#define MOVIE_FLAG_FROM_POWERON			(1<<1)
#define MOVIE_FLAG_WITH_NVRAM			(1<<2)	// to be used w/FROM_POWERON (if selected)

INT32  nReplayStatus = 0;					// 1 record, 2 replay, 0 nothing
INT32  nReplayUndoCount = 0;
UINT32 nReplayCurrentFrame = 0;
UINT32 nStartFrame = 0;
UINT32 nTotalFrames = 0;
bool   bReplayReadOnly = false;
bool   bReplayShowMovement = false;
bool   bReplayFrameCounterDisplay = true;
INT32  nRunInitAutoState = -1;				// run.cpp RunInit(): -1 the automatic state, 0 nothing, 1 the NVRAM only

// If a driver has external data that needs to be recorded every frame.
INT32 nReplayExternalDataCount = 0;
UINT8 *ReplayExternalData = NULL;

extern bool bWithEEPROM;					// state.cpp

static const UINT32 nMovieVersion = 0x0404;
static const UINT32 nMinimumMovieVersion = 0x0404;	// anything lower will not work w/this version of FB Neo

static FILE* fp = NULL;
static INT32 nSizeOffset;
static INT32 movieFlags = 0;
static UINT32 nThisMovieVersion = 0;
static UINT32 nThisFBVersion = 0;
static short nPrevInputs[0x0100];
static char szCurrentMovie[MAX_PATH] = "";
static UINT16 wszMetadata[MAX_METADATA];	// UTF-16 in the file

struct MovieExtInfo
{
	// date & time
	UINT32 year, month;
	UINT16 day, dayofweek;
	UINT32 hour, minute, second;
};

static struct MovieExtInfo MovieInfo = { 0, 0, 0, 0, 0, 0, 0 };

const char* ReplayCurrentFile()
{
	return szCurrentMovie;
}

// ----------------------------------------------------------------------------
// UTF-8 <-> the UTF-16 of the metadata

static int Utf8ToUtf16(const char* pszText, UINT16* pszOut, int nMax)
{
	int n = 0;
	const UINT8* s = (const UINT8*)pszText;
	while (*s && n < nMax - 2) {
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
		if (c >= 0x10000) {
			c -= 0x10000;
			pszOut[n++] = (UINT16)(0xD800 | (c >> 10));
			pszOut[n++] = (UINT16)(0xDC00 | (c & 0x3FF));
		} else {
			pszOut[n++] = (UINT16)c;
		}
	}
	pszOut[n] = 0;
	return n;
}

static void Utf16ToUtf8(const UINT16* pszText, int nLen, char* pszOut, int nSize)
{
	int n = 0;
	for (int i = 0; i < nLen && pszText[i] && n < nSize - 5; i++) {
		UINT32 c = pszText[i];
		if (c >= 0xD800 && c < 0xDC00 && i + 1 < nLen && pszText[i + 1] >= 0xDC00 && pszText[i + 1] < 0xE000) {
			c = 0x10000 + ((c - 0xD800) << 10) + (pszText[++i] - 0xDC00);
		}
		if (c < 0x80) {
			pszOut[n++] = (char)c;
		} else if (c < 0x800) {
			pszOut[n++] = (char)(0xC0 | (c >> 6));
			pszOut[n++] = (char)(0x80 | (c & 0x3F));
		} else if (c < 0x10000) {
			pszOut[n++] = (char)(0xE0 | (c >> 12));
			pszOut[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
			pszOut[n++] = (char)(0x80 | (c & 0x3F));
		} else {
			pszOut[n++] = (char)(0xF0 | (c >> 18));
			pszOut[n++] = (char)(0x80 | ((c >> 12) & 0x3F));
			pszOut[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
			pszOut[n++] = (char)(0x80 | (c & 0x3F));
		}
	}
	pszOut[n] = '\0';
}

static void SetPause(bool bPause)
{
#ifdef BUILD_SDL2_IMGUI
	UiSetPause(bPause);
#else
	bRunPause = bPause;
#endif
}

// ----------------------------------------------------------------------------
// Each frame

INT32 RecordInput()
{
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));

	for (UINT32 i = 0; i < nGameInpCount; i++) {
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				if (*bii.pShortVal != nPrevInputs[i]) {
					inputbuf_addbuffer(i);
					inputbuf_addbuffer(*bii.pShortVal >> 8);
					inputbuf_addbuffer(*bii.pShortVal & 0xFF);
					nPrevInputs[i] = *bii.pShortVal;
				}
			} else {
				if (*bii.pVal != nPrevInputs[i]) {
					inputbuf_addbuffer(i);
					inputbuf_addbuffer(*bii.pVal);
					nPrevInputs[i] = *bii.pVal;
				}
			}
		}
	}
	inputbuf_addbuffer(0xFF);

	if (nReplayExternalDataCount && ReplayExternalData) {
		for (INT32 i = 0; i < nReplayExternalDataCount; i++) {
			inputbuf_addbuffer(ReplayExternalData[i]);
		}
	}

	return 0;
}

// 1 at the end of the recording, which is stopped then
INT32 ReplayInput()
{
	UINT8 n;
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));

	if (inputbuf_eof()) {
		bprintf(0, _T("eof at beginning of ReplayInput()!\n"));
		StopReplay();
		return 1;
	}

	// Just to be safe, restore the inputs to the known correct settings
	for (UINT32 i = 0; i < nGameInpCount; i++) {
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				*bii.pShortVal = nPrevInputs[i];
			} else {
				*bii.pVal = nPrevInputs[i];
			}
		}
	}

	// Now read all inputs that need to change from the .fr file
	while ((n = inputbuf_getbuffer()) != 0xFF) {
		BurnDrvGetInputInfo(&bii, n);
		if (bii.pVal) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				UINT8 nHi = inputbuf_getbuffer();
				UINT8 nLo = inputbuf_getbuffer();
				*bii.pShortVal = nPrevInputs[n] = (nHi << 8) | nLo;
			} else {
				*bii.pVal = nPrevInputs[n] = inputbuf_getbuffer();
			}
		} else {
			inputbuf_getbuffer();
		}
	}

	if (nReplayExternalDataCount && ReplayExternalData) {
		for (INT32 i = 0; i < nReplayExternalDataCount; i++) {
			ReplayExternalData[i] = inputbuf_getbuffer();
		}
	}

	if ((GetCurrentFrame() - nStartFrame) == (nTotalFrames - 1)) {
		bprintf(0, _T("*** Replay: pausing before end of video!  Input-eof?: %x\n"), inputbuf_eof());
		SetPause(true);
	}

	if (inputbuf_eof()) {
		StopReplay();
		return 1;
	}

	return 0;
}

// ----------------------------------------------------------------------------
// Start and stop

static INT32 FileError(const char* pszText)
{
	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	AppError((TCHAR*)pszText, 0);
	movieFlags = 0;
	return 1;
}

// The game is already running: fresh (power-on) or as it is (from a savestate)
INT32 StartRecord(const char* pszFile, const char* pszAuthor, bool bFromPowerOn, bool bWithNvram)
{
	StopReplay();

	movieFlags = 0;
	bReplayReadOnly = false;
	bReplayShowMovement = false;

	// the metadata: "romset,author" for the recordings from power-on
	char szMeta[MAX_METADATA * 3];
	if (bFromPowerOn) {
		snprintf(szMeta, sizeof(szMeta), "%s,%s", BurnDrvGetTextA(DRV_NAME), pszAuthor ? pszAuthor : "");
	} else {
		snprintf(szMeta, sizeof(szMeta), "%s", pszAuthor ? pszAuthor : "");
	}
	Utf8ToUtf16(szMeta, wszMetadata, MAX_METADATA);

	{
		// Get date/time -before- starting game.
		memset(&MovieInfo, 0, sizeof(MovieInfo));
		time_t nLocalTime = time(NULL);
		tm* tmLocalTime = localtime(&nLocalTime);

		MovieInfo.hour		= tmLocalTime->tm_hour;
		MovieInfo.minute	= tmLocalTime->tm_min;
		MovieInfo.second	= tmLocalTime->tm_sec;
		MovieInfo.month		= tmLocalTime->tm_mon;
		MovieInfo.day		= tmLocalTime->tm_mday;
		MovieInfo.dayofweek	= tmLocalTime->tm_wday;
		MovieInfo.year		= tmLocalTime->tm_year;
	}

	if (bFromPowerOn) {
		movieFlags |= MOVIE_FLAG_FROM_POWERON;
		if (bWithNvram) { // only applies w/FROM_POWERON
			movieFlags |= MOVIE_FLAG_WITH_NVRAM;
		}
	} else {
		// a rewind to before the start of the recording would break it
		StateRewindReset();
	}

	const char szFileHeader[] = "FB1 ";				// File identifier
	fp = fopen(pszFile, "w+b");
	if (fp == NULL) {
		return FileError("The recording file couldn't be created.\n");
	}
	snprintf(szCurrentMovie, sizeof(szCurrentMovie), "%s", pszFile);

	INT32 nRet = 0;
	fwrite(&szFileHeader, 1, 4, fp);
	INT32 flags_offset = ftell(fp);
	fwrite(&movieFlags, 1, 4, fp);
	if (bFromPowerOn) {
		if (movieFlags & MOVIE_FLAG_WITH_NVRAM) {
			bWithEEPROM = true;
			nRet = BurnStateSaveEmbed(fp, -1, 0);	// Embed contents of NVRAM
			bWithEEPROM = false;
			if (nRet == -1) {
				// game doesn't have NVRAM
				fseek(fp, flags_offset, SEEK_SET);
				movieFlags &= ~MOVIE_FLAG_WITH_NVRAM;
				fwrite(&movieFlags, 1, 4, fp);
				fseek(fp, 0, SEEK_END);
			}
		}
		nRet = 0;
	} else {
		bWithEEPROM = true;
		nRet = BurnStateSaveEmbed(fp, -1, 1);		// Embed Save-State + NVRAM
		bWithEEPROM = false;
	}
	if (nRet < 0) {
		return FileError("The state of the game couldn't be saved in the recording.\n");
	}

	const char szChunkHeader[] = "FR1 ";			// Chunk identifier
	INT32 nZero = 0;

	fwrite(&szChunkHeader, 1, 4, fp);				// Write chunk identifier
	nSizeOffset = ftell(fp);
	fwrite(&nZero, 1, 4, fp);						// reserve space for chunk size
	fwrite(&nZero, 1, 4, fp);						// reserve space for number of frames
	fwrite(&nZero, 1, 4, fp);						// undo count
	fwrite(&nMovieVersion, 1, 4, fp);				// ThisMovieVersion
	fwrite(&MovieInfo, 1, sizeof(MovieInfo), fp);	// date structure (starting point for timer chips)
	fwrite(&nBurnVer, 1, 4, fp);					// fb version#

	if (inputbuf_embed(fp)) {
		return FileError("The recording file couldn't be written.\n");
	}

	nReplayStatus = 1;								// Set record status
	nStartFrame = GetCurrentFrame();
	nReplayUndoCount = 0;

	// Create a baseline so we can just record the deltas afterwards
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));
	for (UINT32 i = 0; i < nGameInpCount; i++) {
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				inputbuf_addbuffer(*bii.pShortVal >> 8);
				inputbuf_addbuffer(*bii.pShortVal & 0xFF);
				nPrevInputs[i] = *bii.pShortVal;
			} else {
				inputbuf_addbuffer(*bii.pVal);
				nPrevInputs[i] = *bii.pVal;
			}
		} else {
			inputbuf_addbuffer(0);
		}
	}

	bprintf(0, _T("*** Recording of file %s started.\n"), pszFile);

	return 0;
}

// The right game is already running, restarted for a recording from power-on
INT32 StartReplay(const char* pszFile, bool bStartPaused)
{
	StopReplay();

	const char szFileHeader[] = "FB1 ";				// File identifier
	char ReadHeader[4];
	INT32 nRet = 0;

	fp = fopen(pszFile, "r+b");
	if (fp == NULL) {
		fp = fopen(pszFile, "rb");					// a read-only file
	}
	if (fp == NULL) {
		return FileError("The recording couldn't be opened.\n");
	}
	snprintf(szCurrentMovie, sizeof(szCurrentMovie), "%s", pszFile);
	wszMetadata[0] = 0;

	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fp);					// Read identifier
	if (memcmp(ReadHeader, szFileHeader, 4)) {		// Not the right file type
		return FileError("This file isn't an input recording.\n");
	}

	fread(&movieFlags, 1, 4, fp);					// Read movie flags
	if (movieFlags & MOVIE_FLAG_FROM_POWERON) {		// Starts from reset
		if (movieFlags & MOVIE_FLAG_WITH_NVRAM) {	// Get NVRAM
			bWithEEPROM = true;
			nRet = BurnStateLoadEmbed(fp, -1, 0, NULL);
			bWithEEPROM = false;
		}
	} else {										// Load the savestate associated with the recording
		bWithEEPROM = true;
		nRet = BurnStateLoadEmbed(fp, -1, 1, NULL);
		bWithEEPROM = false;
	}

	if (nRet == 0) {
		const char szChunkHeader[] = "FR1 ";		// Chunk identifier
		fread(ReadHeader, 1, 4, fp);				// Read identifier
		if (memcmp(ReadHeader, szChunkHeader, 4)) {	// Not the right file type
			nRet = 2;
		} else {
			INT32 nChunkSize = 0;
			nSizeOffset = ftell(fp);				// Save chunk size offset in case the file is re-recorded
			fread(&nChunkSize, 1, 0x04, fp);		// Read chunk size
			INT32 nChunkPosition = ftell(fp);		// For seeking to the metadata
			UINT32 nEndFrame = 0;
			fread(&nEndFrame, 1, 4, fp);			// Read framecount
			nTotalFrames = nEndFrame;

			nStartFrame = GetCurrentFrame();
			fread(&nReplayUndoCount, 1, 4, fp);
			fread(&nThisMovieVersion, 1, 4, fp);

			memset(&MovieInfo, 0, sizeof(MovieInfo));
			fread(&MovieInfo, 1, sizeof(MovieInfo), fp);
			fread(&nThisFBVersion, 1, 4, fp);
			INT32 nEmbedPosition = ftell(fp);

			// Read metadata
			const char szMetadataHeader[] = "FRM1";
			fseek(fp, nChunkPosition + nChunkSize, SEEK_SET);
			memset(ReadHeader, 0, 4);
			fread(ReadHeader, 1, 4, fp);
			if (memcmp(ReadHeader, szMetadataHeader, 4) == 0) {
				INT32 nMetaSize = 0;
				fread(&nMetaSize, 1, 4, fp);
				INT32 nMetaLen = nMetaSize >> 1;
				if (nMetaLen >= MAX_METADATA) {
					nMetaLen = MAX_METADATA - 1;
				}
				INT32 i;
				for (i = 0; i < nMetaLen; ++i) {
					UINT16 c = fgetc(fp) & 0xFF;
					c |= (fgetc(fp) & 0xFF) << 8;
					wszMetadata[i] = c;
				}
				wszMetadata[i] = 0;
			}

			// Seek back to the beginning of inputbuf data
			fseek(fp, nEmbedPosition, SEEK_SET);
			nRet = inputbuf_embed(fp);
		}
	}

	if (nRet) {
		if (nRet == 4) return FileError("This recording is too old for this version.\n");
		if (nRet == 5) return FileError("This recording is from a newer version.\n");
		return FileError("The recording couldn't be loaded.\n");
	}

	nReplayStatus = 2;								// Set replay status
	SetPause(bStartPaused);

	inputbuf_load();

	// Get the baseline
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));
	for (UINT32 i = 0; i < nGameInpCount; i++) {
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				UINT8 nHi = inputbuf_getbuffer();
				UINT8 nLo = inputbuf_getbuffer();
				*bii.pShortVal = nPrevInputs[i] = (nHi << 8) | nLo;
			} else {
				*bii.pVal = nPrevInputs[i] = inputbuf_getbuffer();
			}
		} else {
			inputbuf_getbuffer();
		}
	}

	bprintf(0, _T("*** Replay of file %s started.\n"), pszFile);

	return 0;
}

static void CloseRecord()
{
	INT32 nFrames = GetCurrentFrame() - nStartFrame;

	inputbuf_save();

	INT32 nMetadataOffset = ftell(fp);				// save FRM1 chunk after inputbuf.
	INT32 nChunkSize = ftell(fp) - 4 - nSizeOffset;	// Fill in chunk size and no of recorded frames
	fseek(fp, nSizeOffset, SEEK_SET);
	fwrite(&nChunkSize, 1, 4, fp);
	fwrite(&nFrames, 1, 4, fp);
	fwrite(&nReplayUndoCount, 1, 4, fp);

	// write metadata
	INT32 nMetaLen = 0;
	while (nMetaLen < MAX_METADATA && wszMetadata[nMetaLen]) nMetaLen++;
	if (nMetaLen > 0) {
		fseek(fp, nMetadataOffset, SEEK_SET);
		const char szChunkHeader[] = "FRM1";
		fwrite(szChunkHeader, 1, 4, fp);
		INT32 nMetaSize = nMetaLen * 2;
		fwrite(&nMetaSize, 1, 4, fp);
		for (INT32 i = 0; i < nMetaLen; ++i) {
			fputc(wszMetadata[i] & 0xFF, fp);
			fputc((wszMetadata[i] >> 8) & 0xFF, fp);
		}
	}

	fclose(fp);
	fp = NULL;
}

void StopReplay()
{
	if (nReplayStatus) {
		if (nReplayStatus == 1) {
			bprintf(0, _T(" ** Recording stopped, recorded %d frames.\n"), GetCurrentFrame() - nStartFrame);
			CloseRecord();
		} else {
			bprintf(0, _T(" ** Replay stopped, replayed %d frames.\n"), GetCurrentFrame() - nStartFrame);
			if (fp) {
				fclose(fp);
				fp = NULL;
			}
		}

		inputbuf_exit();

		nReplayStatus = 0;
		nStartFrame = 0;
		memset(&MovieInfo, 0, sizeof(MovieInfo));
	}
}

// A state loaded during a recording or a playback (stated.cpp): the playback
// goes on as a recording unless it's read-only, a read-only recording goes on
// as a playback
void ReplayBeforeStateLoad()
{
	if (!bReplayReadOnly && nReplayStatus == 2) {
		nReplayStatus = 1;
	}
	if (bReplayReadOnly && nReplayStatus == 1) {
		char szFile[MAX_PATH];
		snprintf(szFile, sizeof(szFile), "%s", szCurrentMovie);
		StopReplay();
#ifdef BUILD_SDL2_IMGUI
		bool bPaused = UiGamePaused();
#else
		bool bPaused = bRunPause;
#endif
		if (StartReplay(szFile, bPaused) == 0) {
			nReplayStatus = 2;
		}
	}
}

// ----------------------------------------------------------------------------
// The properties of a recording, for the playback dialog

INT32 ReplayGetInfo(const char* pszFile, ReplayInfo* pInfo)
{
	memset(pInfo, 0, sizeof(*pInfo));

	FILE* fd = fopen(pszFile, "rb");
	if (fd == NULL) {
		snprintf(pInfo->szError, sizeof(pInfo->szError), "The file can't be opened.");
		return 1;
	}
	FILE* fw = fopen(pszFile, "r+b");
	pInfo->bReadOnlyFile = fw == NULL;
	if (fw) fclose(fw);

	char ReadHeader[4];
	INT32 nChunkSize = 0;

	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fd);
	if (memcmp(ReadHeader, "FB1 ", 4)) {
		fclose(fd);
		snprintf(pInfo->szError, sizeof(pInfo->szError), "Not a FB Neo input recording file.");
		return 1;
	}
	INT32 nFlags = 0;
	fread(&nFlags, 1, 4, fd);
	pInfo->bFromPowerOn = (nFlags & MOVIE_FLAG_FROM_POWERON) != 0;
	pInfo->bWithNvram = pInfo->bFromPowerOn && (nFlags & MOVIE_FLAG_WITH_NVRAM);

	if (!pInfo->bFromPowerOn || pInfo->bWithNvram) {
		memset(ReadHeader, 0, 4);
		fread(ReadHeader, 1, 4, fd);
		if (memcmp(ReadHeader, "FS1 ", 4)) {
			fclose(fd);
			snprintf(pInfo->szError, sizeof(pInfo->szError), "Incompatible file-type. Try playback with an earlier version of FB Neo.");
			return 1;
		}
		fread(&nChunkSize, 1, 4, fd);
		if (nChunkSize <= 0x40) {
			fclose(fd);
			snprintf(pInfo->szError, sizeof(pInfo->szError), "The recording is corrupt.");
			return 1;
		}
		INT32 nChunkDataPosition = ftell(fd);
		fread(&pInfo->nFBVersion, 1, 4, fd);		// Version of FB that this file was saved from
		INT32 t[3];
		fread(t, 1, 12, fd);						// the minimum versions and the compressed size
		fread(pInfo->szStateGame, 1, 32, fd);		// the game of the state
		pInfo->szStateGame[32] = '\0';
		fseek(fd, nChunkDataPosition + nChunkSize, SEEK_SET);
	}

	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fd);
	if (memcmp(ReadHeader, "FR1 ", 4)) {
		fclose(fd);
		snprintf(pInfo->szError, sizeof(pInfo->szError), "Incompatible file-type. Try playback with an earlier version of FB Neo.");
		return 1;
	}

	nChunkSize = 0;
	fread(&nChunkSize, 1, 4, fd);
	if (nChunkSize <= 0x10) {
		fclose(fd);
		snprintf(pInfo->szError, sizeof(pInfo->szError), "The recording is corrupt.");
		return 1;
	}

	INT32 nChunkDataPosition = ftell(fd);
	fread(&pInfo->nFrames, 1, 4, fd);
	fread(&pInfo->nUndoCount, 1, 4, fd);
	UINT32 nVersion = 0;
	fread(&nVersion, 1, 4, fd);
	struct MovieExtInfo Info;
	memset(&Info, 0, sizeof(Info));
	fread(&Info, 1, sizeof(Info), fd);
	if (nVersion < nMinimumMovieVersion) {
		fclose(fd);
		snprintf(pInfo->szError, sizeof(pInfo->szError), "Incompatible file-type. Try playback with an earlier version of FB Neo.");
		return 1;
	}
	UINT32 nFBVersion = 0;
	fread(&nFBVersion, 1, 4, fd);
	if (nFBVersion && !pInfo->nFBVersion) pInfo->nFBVersion = nFBVersion;

	int nHour = (Info.hour > 12) ? Info.hour - 12 : Info.hour;
	if (nHour == 0) nHour = 12;
	snprintf(pInfo->szDate, sizeof(pInfo->szDate), "%02d/%02d/%04d @ %02d:%02d:%02d%s", Info.month + 1, Info.day, 2000 + (Info.year % 100), nHour, Info.minute, Info.second, (Info.hour > 12) ? "pm" : "am");

	// metadata: "romset,author" from power-on, the author otherwise
	fseek(fd, nChunkDataPosition + nChunkSize, SEEK_SET);
	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fd);
	if (memcmp(ReadHeader, "FRM1", 4) == 0) {
		INT32 nMetaSize = 0;
		fread(&nMetaSize, 1, 4, fd);
		INT32 nMetaLen = nMetaSize >> 1;
		if (nMetaLen >= MAX_METADATA) nMetaLen = MAX_METADATA - 1;
		UINT16 wszMeta[MAX_METADATA];
		INT32 i;
		for (i = 0; i < nMetaLen; i++) {
			UINT16 c = fgetc(fd) & 0xFF;
			c |= (fgetc(fd) & 0xFF) << 8;
			wszMeta[i] = c;
		}
		wszMeta[i] = 0;

		char szMeta[MAX_METADATA * 3];
		Utf16ToUtf8(wszMeta, nMetaLen, szMeta, sizeof(szMeta));
		const char* pszAuthor = szMeta;
		if (pInfo->bFromPowerOn) {
			const char* pComma = strchr(szMeta, ',');
			int nGame = pComma ? (int)(pComma - szMeta) : (int)strlen(szMeta);
			if (nGame >= (int)sizeof(pInfo->szGame)) nGame = sizeof(pInfo->szGame) - 1;
			memcpy(pInfo->szGame, szMeta, nGame);
			pInfo->szGame[nGame] = '\0';
			pszAuthor = pComma ? pComma + 1 : "";
		}
		snprintf(pInfo->szAuthor, sizeof(pInfo->szAuthor), "%s", pszAuthor);
	}
	if (pInfo->szGame[0] == '\0') {
		snprintf(pInfo->szGame, sizeof(pInfo->szGame), "%s", pInfo->szStateGame);
	}
	fclose(fd);

	pInfo->bValid = true;
	return 0;
}

// ----------------------------------------------------------------------------
// The inputs of the recording in the savestates

static inline void Write32(UINT8*& ptr, const unsigned long v)
{
	*ptr++ = (UINT8)(v&0xff);
	*ptr++ = (UINT8)((v>>8)&0xff);
	*ptr++ = (UINT8)((v>>16)&0xff);
	*ptr++ = (UINT8)((v>>24)&0xff);
}

static inline UINT32 Read32(const UINT8*& ptr)
{
	UINT32 v;
	v = (UINT32)(*ptr++);
	v |= (UINT32)((*ptr++)<<8);
	v |= (UINT32)((*ptr++)<<16);
	v |= (UINT32)((*ptr++)<<24);
	return v;
}

static inline void Write16(UINT8*& ptr, const UINT16 v)
{
	*ptr++ = (UINT8)(v&0xff);
	*ptr++ = (UINT8)((v>>8)&0xff);
}

static inline UINT16 Read16(const UINT8*& ptr)
{
	UINT16 v;
	v = (UINT16)(*ptr++);
	v |= (UINT16)((*ptr++)<<8);
	return v;
}

INT32 FreezeInputSize()
{
	return 4 + 2*nGameInpCount;
}

INT32 FreezeInput(UINT8** buf, INT32* size)
{
	*size = 4 + 2*nGameInpCount;
	*buf = (UINT8*)malloc(*size);
	if (!*buf) {
		bprintf(0, _T("error in FreezeInput()\n"));
		return -1;
	}

	UINT8* ptr = *buf;
	Write32(ptr, nGameInpCount);
	for (UINT32 i = 0; i < nGameInpCount; i++) {
		Write16(ptr, nPrevInputs[i]);
	}

	return 0;
}

INT32 UnfreezeInput(const UINT8* buf, INT32 size)
{
	UINT32 n = Read32(buf);
	if (n > 0x100 || (unsigned)size < (4 + 2*n)) {
		bprintf(0, _T("error in UnfreezeInput()\n"));
		return -1;
	}

	for (UINT32 i = 0; i < n; i++) {
		nPrevInputs[i] = Read16(buf);
	}

	return 0;
}
