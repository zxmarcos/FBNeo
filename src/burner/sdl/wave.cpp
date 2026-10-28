// Wave log module: the sound of the game in a .wav (win32/wave.cpp without the
// file dialog, the samples are written by RunGetNextSound() in run.cpp)
#include "burner.h"

FILE* WaveLog = NULL;

// Make a wave header so we can write out raw samples after it to make a wave file
static int WaveLogHeaderStart(FILE* Hand, int nSamRate)
{
	UINT32 ua = 0x10;
	UINT16 wFormatTag = 1;						// PCM
	UINT16 nChannels = 2;						// stereo
	UINT32 nSamplesPerSec = nSamRate;
	UINT32 nAvgBytesPerSec = nSamRate * 4;		// samrate*2*short
	UINT16 nBlockAlign = 4;						// 2*short
	UINT16 wBitsPerSample = 16;					// 16-bit

	if (Hand == NULL)
		return 1;
	fwrite("RIFF    WAVEfmt ", 1, 0x10, Hand);	// fill in 0x04 later
	fwrite(&ua, 1, sizeof(ua), Hand);
	fwrite(&wFormatTag, 1, sizeof(wFormatTag), Hand);
	fwrite(&nChannels, 1, sizeof(nChannels), Hand);
	fwrite(&nSamplesPerSec, 1, sizeof(nSamplesPerSec), Hand);
	fwrite(&nAvgBytesPerSec, 1, sizeof(nAvgBytesPerSec), Hand);
	fwrite(&nBlockAlign, 1, sizeof(nBlockAlign), Hand);
	fwrite(&wBitsPerSample, 1, sizeof(wBitsPerSample), Hand);
	fwrite("data    ", 1, 0x08, Hand);			// fill in 0x28 later
	return 0;
}

// Fill in the RIFF and data values
static int WaveLogHeaderFillIn(FILE* Hand)
{
	UINT32 nLen = 0;
	if (Hand == NULL)
		return 1;
	fseek(Hand, 0, SEEK_END); nLen = ftell(Hand);
	fseek(Hand, 0x04, SEEK_SET); nLen -= 8;  fwrite(&nLen, 1, sizeof(nLen), Hand);	// RIFF value
	fseek(Hand, 0x28, SEEK_SET); nLen -= 36; fwrite(&nLen, 1, sizeof(nLen), Hand);	// data value
	fseek(Hand, 0, SEEK_END);
	return 0;
}

INT32 WaveLogStart(const char* pszFile)
{
	WaveLogStop();								// make sure old log is closed

	WaveLog = fopen(pszFile, "wb");
	if (WaveLog == NULL) {
		return 1;
	}
	WaveLogHeaderStart(WaveLog, nAudSampleRate[nAudSelect]);

	return 0;
}

INT32 WaveLogStop()
{
	if (WaveLog) {
		WaveLogHeaderFillIn(WaveLog);
		fclose(WaveLog);
		WaveLog = NULL;
	}

	return 0;
}
