// SDL_Sound module

#include <SDL.h>
#include "burner.h"
#include "aud_dsp.h"
#include <math.h>

static unsigned int nSoundFps;

extern int delay_ticks(int ticks);

static int nSDLGain = 256;              // nAudVolume in 1/256 steps, above 256 it amplifies
int (*GetNextSound)(int);               // Callback used to request more sound

static SDL_AudioSpec audiospec;

#if SDL_VERSION_ATLEAST(2, 0, 0)
char szAudSDLDriver[32] = "";			// the SDL audio driver, "" = the one SDL picks
char szAudSDLDevice[256] = "";			// the output device, "" = the default one
static SDL_AudioDeviceID nAudDevice = 0;
static bool bDriverPicked = false;		// SDL_AudioInit() was called with szAudSDLDriver
#endif

static short* SDLAudBuffer;
static int nSDLPlayPos;
static int nSDLFillSeg;
static int nAudLoopLen;

// Copies samples with the volume, which can go up to 200%
static void CopyWithVolume(Uint8* pDest, const Uint8* pSrc, int nBytes)
{
	Sint16* pd = (Sint16*)pDest;
	const Sint16* ps = (const Sint16*)pSrc;
	int nGain = nSDLGain;

	for (int i = 0; i < (nBytes >> 1); i++) {
		int s = (ps[i] * nGain) >> 8;
		if (s > 32767) s = 32767;
		if (s < -32768) s = -32768;
		pd[i] = (Sint16)s;
	}
}

void audiospec_callback(void* /* data */, Uint8* stream, int len)
{
	int end = nSDLPlayPos + len;
	if (end > nAudLoopLen)
	{
		CopyWithVolume(stream, (Uint8*)SDLAudBuffer + nSDLPlayPos, nAudLoopLen - nSDLPlayPos);
		end -= nAudLoopLen;
		CopyWithVolume(stream + nAudLoopLen - nSDLPlayPos, (Uint8*)SDLAudBuffer, end);
		nSDLPlayPos = end;
	}
	else
	{
		CopyWithVolume(stream, (Uint8*)SDLAudBuffer + nSDLPlayPos, len);
		nSDLPlayPos = end;

		if (nSDLPlayPos == nAudLoopLen)
		{
			nSDLPlayPos = 0;
		}
	}
}

static int SDLSoundGetNextSoundFiller(int)                          // int bDraw
{
	if (nAudNextSound == NULL)
	{
		return 1;
	}
	memset(nAudNextSound, 0, nAudSegLen << 2);                  // Write silence into the buffer

	return 0;
}

static int SDLSoundBlankSound()
{
	if (nAudNextSound)
	{
		memset(nAudNextSound, 0, nAudSegLen << 2);
	}
	return 0;
}

#define WRAP_INC(x) { x++; if (x >= nAudSegCount) x = 0; }

static int SDLSoundCheck()
{
	int nPlaySeg, nFollowingSeg;

	if (!bAudPlaying)
		return 1;

	// Since the SDL buffer is smaller than a segment, only fill the buffer up to the start of the currently playing segment
	nPlaySeg = nSDLPlayPos / (nAudSegLen << 2) - 1;

	if (nPlaySeg >= nAudSegCount)
	{
		nPlaySeg -= nAudSegCount;
	}
	if (nPlaySeg < 0)
	{
		nPlaySeg = nAudSegCount - 1;
	}

	if (nSDLFillSeg == nPlaySeg) {
		//	delay_ticks(1);
			return 0;
		}

	// work out which seg we will fill next
	nFollowingSeg = nSDLFillSeg;
	WRAP_INC(nFollowingSeg);

	while (nSDLFillSeg != nPlaySeg)
	{
		int bDraw;

		bDraw = (nFollowingSeg == nPlaySeg);//	|| bAlwaysDrawFrames;	// If this is the last seg of sound, flag bDraw (to draw the graphics)
		GetNextSound(bDraw);                                // get more sound into nAudNextSound

		if (nAudDSPModule[0])
		{
			DspDo(nAudNextSound, nAudSegLen);
		}

		memcpy((char*)SDLAudBuffer + nSDLFillSeg * (nAudSegLen << 2), nAudNextSound, nAudSegLen << 2);

		nSDLFillSeg = nFollowingSeg;
		WRAP_INC(nFollowingSeg);
	}

	return 0;
}

static int SDLSoundExit()
{
	DspExit();
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (nAudDevice)
	{
		SDL_CloseAudioDevice(nAudDevice);
		nAudDevice = 0;
	}
#else
	SDL_CloseAudio();
#endif

	free(SDLAudBuffer);
	SDLAudBuffer = NULL;

	free(nAudNextSound);
	nAudNextSound = NULL;

	return 0;
}

static int SDLSetCallback(int (*pCallback)(int))
{
	if (pCallback == NULL)
	{
		GetNextSound = SDLSoundGetNextSoundFiller;
	}
	else
	{
		GetNextSound = pCallback;
	}
	return 0;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
// SDL_AudioInit() is the way to use another driver than the one SDL picked,
// the interface calls it too when the driver changes with no game running
void AudSDLPickDriver()
{
	const char* pszCurrent = SDL_GetCurrentAudioDriver();

	if (szAudSDLDriver[0])
	{
		if (pszCurrent == NULL || strcmp(pszCurrent, szAudSDLDriver))
		{
			SDL_AudioQuit();
			if (SDL_AudioInit(szAudSDLDriver))
			{
				fprintf(stderr, "Couldn't use the audio driver %s: %s\n", szAudSDLDriver, SDL_GetError());
				SDL_AudioInit(NULL);
			}
			bDriverPicked = true;
		}
	}
	else if (bDriverPicked || pszCurrent == NULL)
	{
		SDL_AudioQuit();
		SDL_AudioInit(NULL);
		bDriverPicked = false;
	}
}
#endif

static int SDLSoundInit()
{
	SDL_AudioSpec audiospec_req;
	int nSDLBufferSize;

	printf("SDLSoundInit (%dHz) (%dFPS)\n", nAudSampleRate[0], nAppVirtualFps);

	if (nAudSampleRate[0] <= 0)
	{
		return 1;
	}

	nSoundFps = nAppVirtualFps;
	nAudSegLen = (nAudSampleRate[0] * 100 + (nSoundFps >> 1)) / nSoundFps;
	nAudLoopLen = (nAudSegLen * nAudSegCount) << 2;
	for (nSDLBufferSize = 64; nSDLBufferSize < (nAudSegLen >> 1); nSDLBufferSize <<= 1)
	{

	}

	memset(&audiospec_req, 0, sizeof(audiospec_req));
	audiospec_req.freq = nAudSampleRate[0];
	audiospec_req.format = AUDIO_S16;
	audiospec_req.channels = 2;
	audiospec_req.samples = nSDLBufferSize;
	audiospec_req.callback = audiospec_callback;

	SDLAudBuffer = (short*)malloc(nAudLoopLen);
	if (SDLAudBuffer == NULL)
	{
		printf("Couldn't malloc SDLAudBuffer\n");
		SDLSoundExit();
		return 1;
	}
	memset(SDLAudBuffer, 0, nAudLoopLen);

	nAudNextSound = (short*)malloc(nAudSegLen << 2);
	if (nAudNextSound == NULL)
	{
		SDLSoundExit();
		return 1;
	}

	nSDLPlayPos = 0;
	nSDLFillSeg = nAudSegCount - 1;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	AudSDLPickDriver();

	// no format changes allowed: SDL converts to what the device wants
	nAudDevice = SDL_OpenAudioDevice(szAudSDLDevice[0] ? szAudSDLDevice : NULL, 0, &audiospec_req, &audiospec, 0);
	if (nAudDevice == 0 && szAudSDLDevice[0])
	{
		fprintf(stderr, "Couldn't open the audio device %s: %s\n", szAudSDLDevice, SDL_GetError());
		nAudDevice = SDL_OpenAudioDevice(NULL, 0, &audiospec_req, &audiospec, 0);
	}
	if (nAudDevice == 0)
#else
	if (SDL_OpenAudio(&audiospec_req, &audiospec))
#endif
	{
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
		SDLSoundExit();
		return 1;
	}
	DspInit();
	SDLSetCallback(NULL);

	return 0;
}

static int SDLSoundPlay()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(nAudDevice, 0);
#else
	SDL_PauseAudio(0);
#endif
	bAudPlaying = 1;

	return 0;
}

static int SDLSoundStop()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (nAudDevice)
	{
		SDL_PauseAudioDevice(nAudDevice, 1);
	}
#else
	SDL_PauseAudio(1);
#endif
	bAudPlaying = 0;

	return 0;
}

static int SDLSoundSetVolume()
{
	nSDLGain = int(256 * nAudVolume / 10000);
	if (nSDLGain > 512) nSDLGain = 512;
	if (nSDLGain < 0) nSDLGain = 0;
	return 1;
}

static int SDLGetSettings(InterfaceInfo* /* pInfo */)
{
	return 0;
}

struct AudOut AudOutSDL = { SDLSoundBlankSound, SDLSoundCheck, SDLSoundInit, SDLSetCallback, SDLSoundPlay, SDLSoundStop, SDLSoundExit, SDLSoundSetVolume, SDLGetSettings, _T("SDL audio output") };
