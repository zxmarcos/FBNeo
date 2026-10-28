// Run module
#include "burner.h"
#ifdef BUILD_SDL2
#include "sdl2_gui_common.h"
#endif
#ifdef BUILD_SDL2_IMGUI
#include "ui.h"
#endif
#ifndef _MSC_VER
#include <sys/time.h>
#endif

static unsigned int nDoFPS = 0;
bool bAltPause = 0;

int bAlwaysDrawFrames = 0;

int counter;                                // General purpose variable used when debugging

static unsigned int nNormalLast = 0;        // Last value of GetTime()
static int          nNormalFrac = 0;        // Extra fraction we did

bool        bAppDoStep = 0;
bool        bAppDoFast = 0;
bool        bAppDoRewind = 0;
bool        bAppShowFPS = 0;
static int  nFastSpeed = 6;
static bool bscreenshot = 0;

// SlowMo T.A. feature
int nSlowMo = 0;
static int flippy = 0; // free running RunFrame() counter

UINT32 messageFrames = 0;
char lastMessage[MESSAGE_MAX_LENGTH];

#ifdef BUILD_SDL2
static Uint32 starting_stick;
/// Ingame gui
extern SDL_Renderer* sdlRenderer;
extern void ingame_gui_start(SDL_Renderer* renderer);
/// Save States
static char Windowtitle[512];

extern void AdjustImageSize();		// vid_sdl2.cpp
#endif

int bDrvSaveAll = 0;

// The automatic save
int StatedAuto(int bSave)
{
	static TCHAR szName[MAX_PATH] = _T("");
	int nRet;

#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	_stprintf(szName, _T("%s%s.fs"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
#else
	_stprintf(szName, _T("config/games/%s.fs"), BurnDrvGetText(DRV_NAME));
#endif

	if (bSave == 0)
	{
		printf("loading state %i %s\n", bDrvSaveAll, szName);
		nRet = BurnStateLoad(szName, bDrvSaveAll, NULL);		// Load ram
		if (nRet && bDrvSaveAll)
		{
			nRet = BurnStateLoad(szName, 0, NULL);				// Couldn't get all - okay just try the nvram
		}
	}
	else
	{
		printf("saving state %i %s\n", bDrvSaveAll, szName);
		nRet = BurnStateSave(szName, bDrvSaveAll);				// Save ram
	}

	return nRet;
}


/// End Save States

char fpsstring[20];

static time_t fpstimer;
static unsigned int nPreviousFrames;

static void DisplayFPSInit()
{
	nDoFPS = 0;
	fpstimer = 0;
	nPreviousFrames = nFramesRendered;
}

static void DisplayFPS()
{
	time_t temptime = clock();
	double fps = (double)(nFramesRendered - nPreviousFrames) * CLOCKS_PER_SEC / (temptime - fpstimer);
	if (bAppDoFast) {
		fps *= nFastSpeed + 1;
	}
	if (fpstimer && temptime - fpstimer > 0) { // avoid strange fps values
		sprintf(fpsstring, "%2.2lf", fps);
	}

	fpstimer = temptime;
	nPreviousFrames = nFramesRendered;
}


//crappy message system
void UpdateMessage(char* message)
{
	snprintf(lastMessage, MESSAGE_MAX_LENGTH, "%s", message);
	messageFrames = MESSAGE_MAX_FRAMES;
}

// the messages of the core (rewind)
INT32 VidSNewShortMsg(const TCHAR* pText, INT32, INT32, INT32)
{
	UpdateMessage((char*)pText);
	return 0;
}

// The system macros of the input mapping (pause, FFWD, frame advance,
// slow-mo, the state slots), as CheckSystemMacros() of the Win32 build.
// The state actions wait for the end of the frame.
static int nStateAction = 0;
enum { STATE_ACTION_NONE, STATE_ACTION_LOAD, STATE_ACTION_SAVE, STATE_ACTION_NEXT, STATE_ACTION_PREV, STATE_ACTION_UNDO };

static void CheckSystemMacros()
{
	static UINT8 prevPause = 0, prevFrame = 0, prevFFWD = 0;
	static UINT8 prevLoad = 0, prevSave = 0, prevNext = 0, prevPrev = 0, prevUndo = 0;

	if (macroSystemPause && !prevPause) {
#ifdef BUILD_SDL2_IMGUI
		UiTogglePause();
#else
		bRunPause = !bRunPause;
#endif
	}
	prevPause = macroSystemPause;

	if (macroSystemFFWD) {
		bAppDoFast = 1;
		prevFFWD = 1;
	} else if (prevFFWD) {
		bAppDoFast = 0;
		prevFFWD = 0;
	}

	if (macroSystemFrame && !prevFrame) {
		if (bRunPause) {
			bAppDoStep = 1;
		} else {
#ifdef BUILD_SDL2_IMGUI
			UiTogglePause();
#else
			bRunPause = 1;
#endif
		}
	}
	prevFrame = macroSystemFrame;

	int nSlow = macroSystemSlowMo[0] * 1 + macroSystemSlowMo[1] * 2 + macroSystemSlowMo[2] * 3 + macroSystemSlowMo[3] * 4 + macroSystemSlowMo[4] * 5;
	if (nSlow >= 1 && nSlow <= 5) {
		nSlowMo = nSlow - 1;
	}

	if (macroSystemLoadState && !prevLoad) nStateAction = STATE_ACTION_LOAD;
	if (macroSystemSaveState && !prevSave) nStateAction = STATE_ACTION_SAVE;
	if (macroSystemNextState && !prevNext) nStateAction = STATE_ACTION_NEXT;
	if (macroSystemPreviousState && !prevPrev) nStateAction = STATE_ACTION_PREV;
	if (macroSystemUNDOState && !prevUndo) nStateAction = STATE_ACTION_UNDO;
	prevLoad = macroSystemLoadState;
	prevSave = macroSystemSaveState;
	prevNext = macroSystemNextState;
	prevPrev = macroSystemPreviousState;
	prevUndo = macroSystemUNDOState;
}

static void DoStateAction()
{
	char szText[32];

	switch (nStateAction) {
		case STATE_ACTION_LOAD: StatedLoad(nSavestateSlot); break;
		case STATE_ACTION_SAVE: StatedSave(nSavestateSlot); break;
		case STATE_ACTION_UNDO: StatedUNDO(nSavestateSlot); break;
		case STATE_ACTION_NEXT:
		case STATE_ACTION_PREV:
			nSavestateSlot = (nStateAction == STATE_ACTION_NEXT) ? (nSavestateSlot % 8) + 1 : ((nSavestateSlot + 6) % 8) + 1;
			snprintf(szText, sizeof(szText), "State slot %d", nSavestateSlot);
			UpdateMessage(szText);
			break;
	}
	nStateAction = STATE_ACTION_NONE;
}

// define this function somewhere above RunMessageLoop()
void ToggleLayer(unsigned char thisLayer)
{
	nBurnLayer ^= thisLayer;                         // xor with thisLayer
	VidRedraw();
	VidPaint(0);
}


#ifdef _MSC_VER
// no gettimeofday() on MSVC, use the SDL performance counter instead
static Uint64 start;

static void ResetTime(void)
{
	start = SDL_GetPerformanceCounter();
}

unsigned int GetTime(void)
{
	return (unsigned int)((SDL_GetPerformanceCounter() - start) * 1000000 / SDL_GetPerformanceFrequency());
}
#else
struct timeval start;

static void ResetTime(void)
{
	gettimeofday(&start, NULL);
}

unsigned int GetTime(void)
{
	unsigned int ticks;
	struct timeval now;
	gettimeofday(&now, NULL);
	ticks = (now.tv_sec - start.tv_sec) * 1000000 + now.tv_usec - start.tv_usec;
	return ticks;
}
#endif

// With or without sound, run one frame.
// If bDraw is true, it's the last frame before we are up to date, and so we should draw the screen
static int RunFrame(int bDraw, int bPause)
{
	if (!bDrvOkay)
	{
		return 1;
	}

	if (nStateAction) {
		DoStateAction();
	}

	// SlowMo T.A. (the values of the Win32 build: 1 = 75%, then 50%, 25%...)
	flippy++;
	if (nSlowMo) {
		if (nSlowMo == 1) { if ((flippy % 4) == 0) return 0; }
		else if ((flippy % ((nSlowMo - 1) * 2)) < (((nSlowMo - 1) * 2) - 1)) return 0;
	}

	if (bPause)
	{
		InputMake(false);
		CheckSystemMacros();
		VidPaint(0);
		return 0;			// keep the picture, don't run the driver
	}
	else
	{
		nFramesEmulated++;
		nCurrentFrame++;
		if (nReplayStatus == 2)
		{
			InputMake(false);				// the game inputs come from the recording
			CheckSystemMacros();
			if (ReplayInput())				// the end of the recording
			{
				bAppDoFast = 0;
#ifdef BUILD_SDL2_IMGUI
				UiSetPause(true);
#else
				bRunPause = 1;
#endif
				return 0;
			}
		}
		else
		{
			InputMake(true);
			CheckSystemMacros();
		}
		if (nReplayStatus == 1)
		{
			RecordInput();					// the inputs to the file
		}
	}

	if (bDraw)
	{
		nFramesRendered++;

		if (!bRunAhead || (BurnDrvGetFlags() & BDF_RUNAHEAD_DISABLED) || bAppDoFast) {     // Run-Ahead feature 				-dink aug 02, 2021
			if (VidFrame()) {				// Do one frame w/o RunAhead or if FFWD is pressed.
				// VidFrame() failed, but we must run a driver frame because we have
            	// a clocked input.  Possibly from recording or netplay(!)
            	// Note: VidFrame() calls BurnDrvFrame() on success.
				pBurnDraw = NULL;			// Make sure no image is drawn
            	BurnDrvFrame();

				AudBlankSound();
			}
		} else {
				pBurnDraw = (BurnDrvGetFlags() & BDF_RUNAHEAD_DRAWSYNC) ? pVidImage : NULL;
				BurnDrvFrame();
				StateRunAheadSave();
				INT16 *pBurnSoundOut_temp = pBurnSoundOut;
				pBurnSoundOut = NULL;
				nCurrentFrame++;
				bBurnRunAheadFrame = 1;

				if (VidFrame()) {
					// VidFrame() failed, but we must run a driver frame because we have
					// an input.  Possibly from recording or netplay(!)
					pBurnDraw = NULL;			// Make sure no image is drawn, since video failed this time 'round.
					BurnDrvFrame();
				}

				bBurnRunAheadFrame = 0;
				nCurrentFrame--;
				StateRunAheadLoad();
				pBurnSoundOut = pBurnSoundOut_temp; // restore pointer, for wav & avi writer
		}

		VidPaint(0);                                              // paint the screen (no need to validate)
	} else {                                       // frame skipping
		pBurnDraw = NULL;                    // Make sure no image is drawn
		BurnDrvFrame();
	}

#ifdef BUILD_SDL2
	StateRewindDoFrame(macroSystemRewind || bAppDoRewind, macroSystemRewindCancel, bRunPause);
#endif

	if (bAppShowFPS) {
		if (nDoFPS < nFramesRendered) {
			DisplayFPS();
			nDoFPS = nFramesRendered + 30;
		}
	}

	return 0;
}

// Callback used when DSound needs more sound
static int RunGetNextSound(int bDraw)
{
	if (nAudNextSound == NULL)
	{
		return 1;
	}

	if (bRunPause)
	{
		if (bAppDoStep)
		{
			RunFrame(bDraw, 0);
			memset(nAudNextSound, 0, nAudSegLen << 2);                                        // Write silence into the buffer
		}
		else
		{
			RunFrame(bDraw, 1);
			memset(nAudNextSound, 0, nAudSegLen << 2);                                        // Write silence into the buffer
		}

		bAppDoStep = 0;                                                   // done one step
		return 0;
	}

	if (bAppDoFast)
	{                                            // do more frames
		for (int i = 0; i < nFastSpeed; i++)
		{
			RunFrame(0, 0);
		}
	}

	// Render frame with sound
	pBurnSoundOut = nAudNextSound;
	RunFrame(bDraw, 0);
	if (WaveLog != NULL && pBurnSoundOut != NULL)				// Audio > Record Sound
	{
		fwrite(pBurnSoundOut, 1, nBurnSoundLen << 2, WaveLog);
	}
	if (bAppDoStep)
	{
		memset(nAudNextSound, 0, nAudSegLen << 2);                // Write silence into the buffer
	}
	bAppDoStep = 0;                                              // done one step

	return 0;
}

int delay_ticks(int ticks)
{
//sdl_delay can take up to 10 - 15 ticks it doesnt guarentee below this
   int startTicks = 0;
   int endTicks = 0;
   int checkTicks = 0;

   startTicks=SDL_GetTicks();

   while (checkTicks <= ticks)
   {
      endTicks=SDL_GetTicks();
      checkTicks = endTicks - startTicks;
   }

   return ticks;
}
int RunIdle()
{
	int nTime, nCount;

	if (bAudPlaying)
	{
		// Run with sound
		AudSoundCheck();
		return 0;
	}

	// Run without sound
	nTime = GetTime() - nNormalLast;
	nCount = (nTime * nAppVirtualFps - nNormalFrac) / 100000;
	if (nCount <= 0) {						// No need to do anything for a bit
		//delay_ticks(2);
		return 0;
	}

	nNormalFrac += nCount * 100000;
	nNormalLast += nNormalFrac / nAppVirtualFps;
	nNormalFrac %= nAppVirtualFps;

	if (nCount > 100) {						// Limit frame skipping
		nCount = 100;
	}
	if (bRunPause) {
		if (bAppDoStep) {					// Step one frame
			nCount = 0;
		}
		else {
			RunFrame(1, 1);					// Paused
			return 0;
		}
	}
	bAppDoStep = 0;


	if (bAppDoFast)
	{									// do more frames
		for (int i = 0; i < nFastSpeed; i++)
		{
			RunFrame(0, 0);
		}
	}

	if (!bAlwaysDrawFrames)
	{
		for (int i = nCount / 10; i > 0; i--)
		{              // Mid-frames
			RunFrame(0, 0);
		}
	}
	RunFrame(1, 0);                                  // End-frame
	// temp added for SDLFBA
	//VidPaint(0);
	return 0;
}

int RunReset()
{
	// Reset the speed throttling code
	nNormalLast = 0; nNormalFrac = 0;
	if (!bAudPlaying)
	{
		// run without sound
		nNormalLast = GetTime();
	}
	return 0;
}

int RunInit()
{
	ResetTime();
	DisplayFPSInit();
	// Try to run with sound
	AudSetCallback(RunGetNextSound);
	AudSoundPlay();

	RunReset();
	// a recording from power-on starts without the automatic state, or with the NVRAM only
	if (nRunInitAutoState < 0)
	{
		StatedAuto(0);
	}
	else if (nRunInitAutoState == 1)
	{
		int bOldSaveAll = bDrvSaveAll;
		bDrvSaveAll = 0;
		StatedAuto(0);
		bDrvSaveAll = bOldSaveAll;
	}
	nRunInitAutoState = -1;
	return 0;
}

int RunExit()
{
	nNormalLast = 0;
	StatedAuto(1);
	return 0;
}

#ifdef BUILD_SDL2_IMGUI
// Opens the sound output again (another driver, device or buffer size) at the
// same sample rate, the game goes on without sound if that fails
int RunAudioReInit()
{
	bool bPlaying = bAudPlaying;

	pBurnSoundOut = NULL;
	AudSoundStop();
	AudSoundExit();
	if (AudSoundInit()) {
		RunReset();
		return 1;
	}
	AudSetCallback(RunGetNextSound);
	AudSoundSetVolume();
	if (bPlaying) {
		AudSoundPlay();
	}
	RunReset();
	return 0;
}
#endif

#ifdef BUILD_SDL2
void pause_game()
{
	AudSoundStop();

	if(nVidSelect) {
		// no Text in OpenGL...
		SDL_GL_SwapWindow(sdlWindow);
	}else{
		inprint_shadowed(sdlRenderer, "PAUSE", 10, 10);
		SDL_RenderPresent(sdlRenderer);
	}

    int finished = 0;
	while (!finished)
  	{
		starting_stick = SDL_GetTicks();

 		SDL_Event e;

		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
			{
				finished=1;
			}
			if (e.type == SDL_KEYDOWN)
			{
			  switch (e.key.keysym.sym)
			  {
				  case SDLK_TAB:
				  case SDLK_p:
					finished=1;
					break;
				  default:
					break;
			  }
			}
			if (e.type == SDL_WINDOWEVENT)
			{ // Window Event
				switch (e.window.event)
				{
					//case SDL_WINDOWEVENT_RESTORED: // keep pause when restore window
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						finished=1;
						break;
					case SDL_WINDOWEVENT_CLOSE:
						finished=1;
						RunExit();
						break;
					default:
						break;
				}
			}
		}

		// limit 5 FPS (free CPU usage)
		if ( ( 1000 / 5 ) > SDL_GetTicks() - starting_stick) {
			SDL_Delay( 1000 / 5 - ( SDL_GetTicks() - starting_stick ) );
		}

	}

	AudSoundPlay();
}
#endif

#ifndef BUILD_MACOS
// The main message loop
int RunMessageLoop()
{
	int quit = 0;

	RunInit();
	GameInpCheckMouse();                                                                     // Hide the cursor

	while (!quit)
	{

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:                                        /* Windows was closed */
				quit = 1;
				break;

#ifdef BUILD_SDL2
			case SDL_WINDOWEVENT:  // Window Event
				switch (event.window.event)
				{
					case SDL_WINDOWEVENT_MINIMIZED:
					case SDL_WINDOWEVENT_FOCUS_LOST:
						pause_game();
						break;
				}
				break;
#endif

			case SDL_KEYDOWN:                                                // need to find a nicer way of doing this...
				switch (event.key.keysym.sym)
				{
				case SDLK_F1:
					bAppDoFast = 1;
					break;
				case SDLK_F9:
					QuickState(0);
					break;
				case SDLK_F10:
					QuickState(1);
					break;
				case SDLK_F11:
					bAppShowFPS = !bAppShowFPS;
#ifdef BUILD_SDL2
					if (!bAppShowFPS)
					{
						sprintf(Windowtitle, "FBNeo - %s - %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));
						SDL_SetWindowTitle(sdlWindow, Windowtitle);
					}
#endif
					break;
#ifdef BUILD_SDL2
				case SDLK_TAB:
					if(!nVidSelect) {
						ingame_gui_start(sdlRenderer);
					} else {
						// Pause with SDL2 OpenGL mode
						pause_game();
					}
					break;

				case SDLK_RETURN:
					if (event.key.keysym.mod & KMOD_ALT)
					{
						SetFullscreen(!GetFullscreen());
						AdjustImageSize();
					}
					break;
#endif
				case SDLK_F6: // screeenshot
					if (!bscreenshot) {
						MakeScreenShot(0);
						bscreenshot = 1;
					}
					break;
				case SDLK_KP_MINUS: // volumme -
					nAudVolume -= 500;
					if (nAudVolume < 0) {
						nAudVolume = 0;
					}
					if (AudSoundSetVolume() == 0) {
					}
					break;
				case SDLK_KP_PLUS: // volume -+
					nAudVolume += 500;
					if (nAudVolume > 10000) {
						nAudVolume = 10000;
					}
					if (AudSoundSetVolume() == 0) {
					}
					break;
				default:
					break;
				}
				break;

			case SDL_KEYUP:                                                // need to find a nicer way of doing this...
				switch (event.key.keysym.sym)
				{
				case SDLK_F1:
					bAppDoFast = 0;
					break;
				case SDLK_F6:
					bscreenshot = 0;
					break;
				case SDLK_F12:
					quit = 1;
					break;

				default:
					break;
				}
				break;
			}
		}

		RunIdle();

	}

	RunExit();

	return 0;
}

#endif
