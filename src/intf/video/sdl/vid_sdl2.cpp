// blitter effects via SDL2
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#ifdef INCLUDE_SWITCHRES
#include <switchres_wrapper.h>
#endif

#include <SDL.h>
#include <SDL_image.h>

extern int vsync;
extern char videofiltering[3];

static unsigned char* VidMem = NULL;
extern SDL_Window* sdlWindow;
SDL_Renderer* sdlRenderer = NULL;
static SDL_Texture* sdlTexture = NULL;
static int  nRotateGame = 0;
static bool bFlipped = false;
static SDL_Rect dstrect;
static char Windowtitle[512];
static int display_w = 400, display_h = 300;
Uint32 screenFlags;

extern UINT16 maxLinesMenu;	// sdl2_gui_ingame.cpp: number of lines to show in ingame menus
extern bool didReinitialise;

void RenderMessage()
{
	// First render anything that is key-held based
	if (bAppDoFast)
	{
		inprint_shadowed(sdlRenderer, "FFWD", 10, 10);
	}
	if (bAppShowFPS)
	{
		if (bAppFullscreen)
		{
			inprint_shadowed(sdlRenderer, fpsstring, 10, 50);
		} 
		else 
		{
			sprintf(Windowtitle, "FBNeo - FPS: %s - %s - %s", fpsstring, BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));
			SDL_SetWindowTitle(sdlWindow, Windowtitle);
		}
	}

	if (messageFrames > 1)
	{
		inprint_shadowed(sdlRenderer, lastMessage, 10, 30);
		messageFrames--;
	}
}

#ifdef BUILD_SDL2_IMGUI
// With the ImGui interface the window and the renderer belong to ui_core.cpp,
// this module keeps the game texture, which the interface draws every frame.

void VidSDL2SetFiltering()
{
	if (sdlTexture) {
		SDL_ScaleMode nMode = SDL_ScaleModeNearest;
		if (videofiltering[0] == '1') nMode = SDL_ScaleModeLinear;
		if (videofiltering[0] == '2') nMode = SDL_ScaleModeBest;
		SDL_SetTextureScaleMode(sdlTexture, nMode);
	}
}

// Size of the game picture on screen, before scaling
void VidSDL2GetScreenSize(int* pnWidth, int* pnHeight)
{
	*pnWidth = nRotateGame ? display_h : display_w;
	*pnHeight = nRotateGame ? display_w : display_h;
}

// Draws the game in pArea (window coordinates), keeping the aspect ratio
void VidSDL2DrawGame(const SDL_Rect* pArea)
{
	if (sdlTexture == NULL || pArea->w <= 0 || pArea->h <= 0) {
		return;
	}

	int w, h;
	VidSDL2GetScreenSize(&w, &h);

	int nOutW = pArea->w, nOutH = pArea->h;
	if (!bVidFullStretch) {
		double dScale = SDL_min((double)pArea->w / w, (double)pArea->h / h);
		if (bIntegerScale && dScale >= 1.0) {
			dScale = floor(dScale);
		}
		nOutW = (int)(w * dScale + 0.5);
		nOutH = (int)(h * dScale + 0.5);
	}

	// SDL_RenderCopyEx() rotates around the centre, so the rectangle is given unrotated
	SDL_Rect dst;
	dst.w = nRotateGame ? nOutH : nOutW;
	dst.h = nRotateGame ? nOutW : nOutH;
	dst.x = pArea->x + (pArea->w - dst.w) / 2;
	dst.y = pArea->y + (pArea->h - dst.h) / 2;

	if (nRotateGame) {
		SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, &dst, (bFlipped ? 90 : 270), NULL, SDL_FLIP_NONE);
	} else if (bFlipped) {
		SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, &dst, 180, NULL, SDL_FLIP_NONE);
	} else {
		SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &dst);
	}
}
#endif

static int Exit()
{
#ifdef INCLUDE_SWITCHRES
	sr_deinit();
#endif
#ifndef BUILD_SDL2_IMGUI
	kill_inline_font(); //TODO: This is not supposed to be here
#endif
	SDL_DestroyTexture(sdlTexture);
	sdlTexture = NULL;
#ifndef BUILD_SDL2_IMGUI
	SDL_DestroyRenderer(sdlRenderer);
	sdlRenderer = NULL;
	SDL_DestroyWindow(sdlWindow);
	sdlWindow = NULL;
#endif

	if (VidMem)
	{
		free(VidMem);
		VidMem = NULL;
	}
	return 0;
}

void AdjustImageSize()
{
	screenFlags = SDL_GetWindowFlags(sdlWindow);

	// Scale governed by -windowscale command line option.
	// Screen center fix thanks to Woises
	if (nRotateGame) {
		if (screenFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
			int w;
			int h;
			SDL_GetWindowSize(sdlWindow, &w, &h);
			SDL_SetWindowSize(sdlWindow, (display_w * w / h) * nWindowScale, display_w * nWindowScale);
			SDL_RenderSetLogicalSize(sdlRenderer, (display_w * w / h), display_w);
			dstrect.x = ((display_w * w / h) - display_w) / 2;
		} else {
			SDL_RestoreWindow(sdlWindow);		// If started fullscreen, switching to window can get maximized
			SDL_SetWindowSize(sdlWindow, display_h * nWindowScale, display_w * nWindowScale);
			SDL_RenderSetLogicalSize(sdlRenderer, display_h, display_w);
			dstrect.x = (display_h - display_w) / 2;
		}
		dstrect.y = (display_w - display_h) / 2;
		maxLinesMenu = display_w / 10 - 6;		// Get number of lines to show in ingame menus
	} else {
		if (screenFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
			int w;
			int h;
			SDL_GetWindowSize(sdlWindow, &w, &h);
			SDL_SetWindowSize(sdlWindow, (display_h * w / h) * nWindowScale, display_h * nWindowScale);

			if (display_w < (display_h * w / h)) {
				SDL_RenderSetLogicalSize(sdlRenderer, (display_h * w / h), display_h);
				dstrect.x = ((display_h * w / h) - display_w) / 2;
				dstrect.y = 0;
			} else {
				SDL_RenderSetLogicalSize(sdlRenderer, display_w, (display_w * h / w));
				dstrect.x = 0;
				dstrect.y = ((display_w * h / w) - display_h) / 2;
			}
		} else {
			SDL_RestoreWindow(sdlWindow);
			SDL_SetWindowSize(sdlWindow, display_w * nWindowScale, display_h * nWindowScale);
			SDL_RenderSetLogicalSize(sdlRenderer, display_w, display_h);
			dstrect.x = 0;
			dstrect.y = 0;
		}
		maxLinesMenu = display_h / 10 - 6;		// Get number of lines to show in ingame menus
	}
	SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	dstrect.w = display_w;
	dstrect.h = display_h;
}

static int Init()
{
	int nMemLen = 0;
	int GameAspectX = 4, GameAspectY = 3;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("vid init error\n");
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
		return 3;
	}

	nRotateGame = 0;

	if (bDrvOkay)
	{
		// Get the game screen size
		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)
		{
#ifdef FBNEO_DEBUG
			printf("Vertical\n");
#endif
			BurnDrvGetVisibleSize(&nVidImageHeight, &nVidImageWidth);
			BurnDrvGetAspect(&GameAspectY, &GameAspectX);
			nRotateGame = 1;
		}
		else
		{
			BurnDrvGetVisibleSize(&nVidImageWidth, &nVidImageHeight);
			BurnDrvGetAspect(&GameAspectX, &GameAspectY);
		}

#ifdef INCLUDE_SWITCHRES
		// Don't force 4:3 aspect-ratio, until there is a command-line switch
		display_w = nVidImageWidth;
		display_h = nVidImageHeight;
		sr_init();
		if (nRotateGame) sr_set_rotation(1);
#else
		if (nRotateGame) {
			display_w = nVidImageHeight * GameAspectX / GameAspectY;
			display_h = nVidImageHeight;
		} else {
			display_w = nVidImageWidth;
			display_h = nVidImageWidth * GameAspectY / GameAspectX;
		}
#endif

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED)
		{
#ifdef FBNEO_DEBUG
			printf("Flipped\n");
#endif
			bFlipped = 1;
		} else {
			bFlipped = 0;
		}
	}

	sprintf(Windowtitle, "FBNeo - %s - %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));

	if (bAppFullscreen) screenFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP;
	else screenFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

	//Test refresh rate availability
#ifdef FBNEO_DEBUG
	printf("Game resolution: %dx%d@%f\n", nVidImageWidth, nVidImageHeight, nBurnFPS/100.0);
#endif

#ifdef INCLUDE_SWITCHRES
	sr_mode srm;
	unsigned char interlace = 0; // FBN doesn't handle interlace yet, force it to disabled
	double rr = nBurnFPS / 100.0;
	sr_init_disp();
	sr_add_mode(display_w, display_h, rr, interlace, &srm);
	sr_switch_to_mode(display_w, display_h, rr, interlace, &srm);
#endif

#ifndef BUILD_SDL2_IMGUI
	if (nRotateGame)
	{
		sdlWindow = SDL_CreateWindow(
			Windowtitle,
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			display_h,
			display_w,
			screenFlags
		);
	}
	else
	{
		sdlWindow = SDL_CreateWindow(
			Windowtitle,
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			display_w,
			display_h,
			screenFlags
		);
	}

	// Check that the window was successfully created
	if (sdlWindow == NULL)
	{
		// In the case that the window could not be made...
		printf("Could not create window: %s\n", SDL_GetError());
		return 1;
	}

	Uint32 renderflags = SDL_RENDERER_ACCELERATED;

	if (vsync)
	{
		renderflags = renderflags | SDL_RENDERER_PRESENTVSYNC;
	}

	sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, renderflags);
	if (sdlRenderer == NULL)
	{
		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_SOFTWARE);
		if (sdlRenderer == NULL)
		{	
			// In the case that the window could not be made...
			printf("Could not create renderer: %s\n", SDL_GetError());
			return 1;
		}
	}

	SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_NONE);
#endif

	nVidImageDepth = 32;

	if (BurnDrvGetFlags() & BDF_16BIT_ONLY)
	{
		nVidImageDepth = 16;
#ifdef FBNEO_DEBUG
		printf("Forcing 16bit color\n");
#endif
	}
#ifdef FBNEO_DEBUG
	printf("bbp: %d\n", nVidImageDepth);
#endif
#ifndef BUILD_SDL2_IMGUI
	if (bIntegerScale)
	{
		SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, videofiltering);

	AdjustImageSize();

#ifdef FBNEO_DEBUG
	printf("setting logical size w: %d h: %d\n", display_w, display_h);
#endif
	
	inrenderer(sdlRenderer); //TODO: this is not supposed to be here
	prepare_inline_font();   // TODO: BAD
	incolor(0xFFF000, 0);
#endif

	if (nVidImageDepth == 32)
	{
		sdlTexture = SDL_CreateTexture(sdlRenderer,
			SDL_PIXELFORMAT_RGB888,
			SDL_TEXTUREACCESS_STREAMING,
			nVidImageWidth, nVidImageHeight);
	}
	else
	{
		sdlTexture = SDL_CreateTexture(sdlRenderer,
			SDL_PIXELFORMAT_RGB565,
			SDL_TEXTUREACCESS_STREAMING,
			nVidImageWidth, nVidImageHeight);
	}
	if (!sdlTexture)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create sdlTexture from surface: %s", SDL_GetError());
		return 3;
	}
#ifdef BUILD_SDL2_IMGUI
	VidSDL2SetFiltering();
#endif

	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;

	SetBurnHighCol(nVidImageDepth);

	nVidImagePitch = nVidImageWidth * nVidImageBPP;
	nBurnPitch = nVidImagePitch;

	nMemLen = nVidImageWidth * nVidImageHeight * nVidImageBPP;
#ifdef FBNEO_DEBUG
	printf("nVidImageWidth=%d nVidImageHeight=%d nVidImagePitch=%d\n", nVidImageWidth, nVidImageHeight, nVidImagePitch);
#endif
	VidMem = (unsigned char*)malloc(nMemLen);
	if (VidMem)
	{
		memset(VidMem, 0, nMemLen);
		pVidImage = VidMem;
#ifdef FBNEO_DEBUG
		printf("Malloc for video Ok %d\n", nMemLen);
#endif
		return 0;
	}
	else
	{
		pVidImage = NULL;
		return 1;
	}

#ifdef FBNEO_DEBUG
	printf("done vid init");
#endif
	return 0;
}

static int vidScale(RECT*, int, int)
{
	return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw)                                          // bRedraw = 0
{
	if ((didReinitialise) && (pVidImage == NULL)) {
		didReinitialise = false;
		Exit();
		Init();
	}
	if (pVidImage == NULL)
	{
		return 1;
	}

	VidFrameCallback(bRedraw);

	return 0;
}

// Paint the BlitFX surface onto the primary surface
static int Paint(int bValidate)
{
#ifdef BUILD_SDL2_IMGUI
	// UiPresent() draws it with the interface
	SDL_UpdateTexture(sdlTexture, NULL, pVidImage, nVidImagePitch);
	return 0;
#endif

	SDL_RenderClear(sdlRenderer);
	SDL_UpdateTexture(sdlTexture, NULL, pVidImage, nVidImagePitch);
	if (nRotateGame)
	{
		SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, &dstrect, (bFlipped ? 90 : 270), NULL, SDL_FLIP_NONE);
	}
	else
	{
		if (bFlipped)
		{
			SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, &dstrect, 180, NULL, SDL_FLIP_NONE);
		}
		else
		{
			SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &dstrect);
		}
	}

	RenderMessage();

	SDL_RenderPresent(sdlRenderer);

	return 0;
}

static int GetSettings(InterfaceInfo* pInfo)
{
	return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDL2 = { Init, Exit, Frame, Paint, vidScale, GetSettings, _T("SDL2 video output") };
