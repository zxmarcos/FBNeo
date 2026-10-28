// Window, renderer and Dear ImGui setup of the SDL2 frontend
#include "burner.h"
#include "ui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <wchar.h>

extern SDL_Renderer* sdlRenderer;			// vid_sdl2.cpp
extern int vsync;
void VidSDL2DrawGame(const SDL_Rect* pArea);

float fUiScale = 1.0f;

static char szImGuiIni[MAX_PATH];

static bool FileExists(const char* pszName)
{
	FILE* f = fopen(pszName, "rb");
	if (f) {
		fclose(f);
		return true;
	}
	return false;
}

// The original titles of the drivers (Japanese, Chinese, Korean) need fonts
// of the system, only with the characters those titles use
static void MergeTitleFonts(float fSize)
{
	ImFontGlyphRangesBuilder builder;
	bool bAny = false;
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		const wchar_t* pszTitles = BurnDrvGetFullNameW(i);
		// all the titles, each one ends with a '\0'
		for (const wchar_t* p = pszTitles; p && *p; p += wcslen(p) + 1) {
			for (const wchar_t* s = p; *s; s++) {
				UINT32 c = *s;
				if (c >= 0x100 && c <= 0xFFFF && (c < 0xD800 || c >= 0xE000)) {	// ImWchar is 16 bits
					builder.AddChar((ImWchar)c);
					bAny = true;
				}
			}
		}
	}
	if (!bAny) {
		return;
	}

	static ImVector<ImWchar> Ranges;		// used until the atlas is built
	builder.BuildRanges(&Ranges);

	// one font per language, the first one found
#ifdef _WIN32
	static const char* pszFonts[][3] = {
		{ "YuGothM.ttc", "meiryo.ttc", "msgothic.ttc" },
		{ "msyh.ttc", "simsun.ttc", NULL },
		{ "malgun.ttf", "gulim.ttc", NULL },
	};
	const char* pszWinDir = getenv("WINDIR");
#else
	static const char* pszFonts[][3] = {
		{ "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc" },
		{ "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf", "/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc", "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc" },
		{ "/System/Library/Fonts/Hiragino Sans GB.ttc", "/System/Library/Fonts/AppleSDGothicNeo.ttc", NULL },
	};
#endif

	for (unsigned int i = 0; i < sizeof(pszFonts) / sizeof(pszFonts[0]); i++) {
		for (int j = 0; j < 3 && pszFonts[i][j]; j++) {
			char szFont[MAX_PATH];
#ifdef _WIN32
			snprintf(szFont, sizeof(szFont), "%s\\Fonts\\%s", pszWinDir ? pszWinDir : "C:\\Windows", pszFonts[i][j]);
#else
			snprintf(szFont, sizeof(szFont), "%s", pszFonts[i][j]);
#endif
			if (FileExists(szFont)) {
				ImFontConfig cfg;
				cfg.MergeMode = true;			// the glyphs already there are kept
				cfg.OversampleH = 1;
				ImGui::GetIO().Fonts->AddFontFromFileTTF(szFont, fSize, &cfg, Ranges.Data);
				break;
			}
		}
	}
}

static void LoadFonts()
{
	ImGuiIO& io = ImGui::GetIO();
	char szFont[MAX_PATH] = "";

#ifdef _WIN32
	const char* pszWinDir = getenv("WINDIR");
	snprintf(szFont, sizeof(szFont), "%s\\Fonts\\segoeui.ttf", pszWinDir ? pszWinDir : "C:\\Windows");
	if (!FileExists(szFont)) {
		szFont[0] = '\0';
	}
#else
	static const char* pszFonts[] = {
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
		"/System/Library/Fonts/Supplemental/Arial.ttf",
	};
	for (unsigned int i = 0; i < sizeof(pszFonts) / sizeof(pszFonts[0]); i++) {
		if (FileExists(pszFonts[i])) {
			snprintf(szFont, sizeof(szFont), "%s", pszFonts[i]);
			break;
		}
	}
#endif

	// latin and its extensions cover the driver names, the built-in font is the fallback
	static const ImWchar LatinRanges[] = { 0x0020, 0x024F, 0x2000, 0x206F, 0x25A0, 0x25FF, 0 };
	float fSize = 16.0f * fUiScale;
	if (szFont[0] == '\0' || io.Fonts->AddFontFromFileTTF(szFont, fSize, NULL, LatinRanges) == NULL) {
		ImFontConfig cfg;
		fSize = 13.0f * fUiScale;
		cfg.SizePixels = fSize;
		io.Fonts->AddFontDefault(&cfg);
	}

	MergeTitleFonts(fSize);
}

static void SetStyle()
{
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.FrameRounding = 3.0f;
	style.PopupRounding = 3.0f;
	style.GrabRounding = 3.0f;
	style.ScrollbarRounding = 3.0f;
	style.WindowBorderSize = 1.0f;
	style.ScaleAllSizes(fUiScale);
}

int UiInit()
{
	float fDpi = 0.0f;
	if (SDL_GetDisplayDPI(0, &fDpi, NULL, NULL) == 0 && fDpi > 0.0f) {
		fUiScale = fDpi / 96.0f;
	}
	if (fUiScale < 1.0f) fUiScale = 1.0f;
	if (fUiScale > 3.0f) fUiScale = 3.0f;

	Uint32 nFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	if (bAppFullscreen) {
		nFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	sdlWindow = SDL_CreateWindow(_T(APP_TITLE), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)(1024 * fUiScale), (int)(640 * fUiScale), nFlags);
	if (sdlWindow == NULL) {
		printf("Could not create window: %s\n", SDL_GetError());
		return 1;
	}

	sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
	if (sdlRenderer == NULL) {
		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_SOFTWARE);
		if (sdlRenderer == NULL) {
			printf("Could not create renderer: %s\n", SDL_GetError());
			return 1;
		}
	}

	SDL_RendererInfo info;
	if (SDL_GetRendererInfo(sdlRenderer, &info) == 0) {
		printf("SDL renderer: %s\n", info.name);
	}

	// the game is drawn with SDL_Renderer under the interface, the OpenGL output can't share the window
	nVidSelect = 0;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	char* pszConfigPath = SDL_GetPrefPath("fbneo", "config");
	snprintf(szImGuiIni, sizeof(szImGuiIni), "%simgui.ini", pszConfigPath);
	SDL_free(pszConfigPath);
#else
	snprintf(szImGuiIni, sizeof(szImGuiIni), "config/imgui.ini");
#endif
	io.IniFilename = szImGuiIni;

	SetStyle();
	LoadFonts();

	ImGui_ImplSDL2_InitForSDLRenderer(sdlWindow, sdlRenderer);
	ImGui_ImplSDLRenderer2_Init(sdlRenderer);

	UiSetNavigation(true);

	return 0;
}

void UiExit()
{
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplSDLRenderer2_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
	}

	if (sdlRenderer) {
		SDL_DestroyRenderer(sdlRenderer);
		sdlRenderer = NULL;
	}
	if (sdlWindow) {
		SDL_DestroyWindow(sdlWindow);
		sdlWindow = NULL;
	}
}

void UiProcessEvent(const SDL_Event* e)
{
	ImGui_ImplSDL2_ProcessEvent(e);
}

void UiNewFrame()
{
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void UiPresent(bool bDrawGame)
{
	ImGui::Render();

	ImGuiIO& io = ImGui::GetIO();
	SDL_RenderSetScale(sdlRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(sdlRenderer);

	if (bDrawGame) {
		int w, h;
		SDL_GetWindowSize(sdlWindow, &w, &h);
		int nTop = (int)UiMenuBarHeight();
		SDL_Rect area = { 0, nTop, w, h - nTop };
		VidSDL2DrawGame(&area);
	}

	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), sdlRenderer);
	SDL_RenderPresent(sdlRenderer);
}

bool UiWantsKeyboard()
{
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
}

bool UiWantsMouse()
{
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
}

void UiSetNavigation(bool bEnable)
{
	ImGuiIO& io = ImGui::GetIO();
	if (bEnable) {
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
	} else {
		io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
	}
}

// Closing a popup gives the focus back to its parent in the same frame, which
// would see the same Escape and close too
bool UiEscapePressed()
{
	static int nFrame = -1;
	if (!ImGui::IsKeyPressed(ImGuiKey_Escape, false) || nFrame == ImGui::GetFrameCount()) {
		return false;
	}
	nFrame = ImGui::GetFrameCount();
	return true;
}

static bool IsUtf8(const unsigned char* s)
{
	while (*s) {
		int n = 0;
		if (*s < 0x80) n = 0;
		else if ((*s & 0xE0) == 0xC0) n = 1;
		else if ((*s & 0xF0) == 0xE0) n = 2;
		else if ((*s & 0xF8) == 0xF0) n = 3;
		else return false;
		s++;
		while (n--) {
			if ((*s & 0xC0) != 0x80) return false;
			s++;
		}
	}
	return true;
}

const char* UiText(const char* pszText)
{
	static char szBuffer[8][1024];
	static int nNext = 0;

	if (pszText == NULL) {
		return "";
	}
	if (IsUtf8((const unsigned char*)pszText)) {
		return pszText;
	}

	char* pszOut = szBuffer[nNext];
	nNext = (nNext + 1) & 7;

	// not valid UTF-8, so take it as latin-1
	int n = 0;
	for (const unsigned char* s = (const unsigned char*)pszText; *s && n < 1020; s++) {
		if (*s < 0x80) {
			pszOut[n++] = *s;
		} else {
			pszOut[n++] = 0xC0 | (*s >> 6);
			pszOut[n++] = 0x80 | (*s & 0x3F);
		}
	}
	pszOut[n] = '\0';

	return pszOut;
}
