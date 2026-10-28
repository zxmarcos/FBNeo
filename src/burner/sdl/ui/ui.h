// Dear ImGui interface for the SDL2 frontend (BUILD_SDL2_IMGUI)
#pragma once

#include <SDL.h>
#include "imgui.h"

// ui_core.cpp
extern float fUiScale;						// HiDPI scale of the fonts and the style

int UiInit();								// creates the window, the renderer and the imgui context
void UiExit();
void UiProcessEvent(const SDL_Event* e);
void UiNewFrame();
void UiPresent(bool bDrawGame);				// draws the game image under the interface and presents
bool UiWantsKeyboard();
bool UiWantsMouse();
void UiSetNavigation(bool bEnable);			// keyboard/gamepad navigation, off while playing
const char* UiText(const char* pszText);	// driver text (latin-1) to UTF-8, valid until a few more calls
ImVec2 UiDialogSize(float fWidth, float fHeight);	// fWidth (scaled) x fHeight, at most 95% of the window
bool UiEscapePressed();						// once per frame: the popup drawn first (the innermost) takes it

// ui_app.cpp
extern bool bUiMenuPinned;					// menu bar always shown in a window
extern bool bUiPauseOnMenu;					// pause the game while a menu or a dialog is open
extern bool bUiPauseOnFocusLoss;

int UiMain(int nDrvSelect, bool bMenu);		// the main loop, nDrvSelect < 0 starts on the game selector
void UiLaunchGame(int nDrv);				// the requests are carried out between two frames
void UiEndGame();
void UiQuit();
void UiToggleFullscreen();
void UiSetWindowScale(int nScale);
void UiShowOverlay(bool bShow);			// the game menu (Tab)
bool UiOverlayShown();
void UiTogglePause();
bool UiGameRunning();
bool UiGamePaused();						// paused by the user (Game > Pause)
bool UiMenuMode();							// the selector comes back when the game ends

// ui_menubar.cpp
void UiMenuBarDraw();
float UiMenuBarHeight();					// height reserved for the menu bar, 0 when it floats over the game

// ui_gamesel.cpp
extern char szSelSearch[256];				// the search of the selector, kept in the ini
extern bool bEnableIcons;					// driver icons in the list
extern int nIconsSize;						// 0, 1, 2: 16, 24, 32 pixels
extern bool bIconsOnlyParents;
extern bool bIconsByHardwares;
extern char szSelMvsCarts[6][64];				// zip names of the Neo Geo MVS cartridges, kept in the ini

void UiGameSelInit();
void UiGameSelExit();
void UiGameSelDraw(bool bOverGame);
void UiGameSelOpen();						// opens the selector over the running game
bool UiGameSelIsOpen();						// open over the running game
void UiGameSelClose();
void UiGameSelRescan();
void UiGameSelStopScan();					// before anything else uses nBurnDrvActive
void UiGameSelReloadImages();				// the image paths changed
void UiGameSelPlay(int nDrv);				// with the MVS slots and the not working confirmation

// ui_game.cpp
extern char szPrevGames[10][32];			// recent games, kept in the ini
extern int bUiFastToggle;					// fast forward on until toggled off
void UiGameMenu();
void UiGameDialogsDraw();
void UiGameOpenStateDialog(bool bSave);
void UiGamePrepareLaunch();					// StartGame(), once the rom scan is stopped
void UiGameStarted();

// ui_hotkeys.cpp
enum {
	UI_HK_PAUSE, UI_HK_FFWD, UI_HK_FFWD_TOGGLE, UI_HK_REWIND, UI_HK_GAME_MENU, UI_HK_LOAD_GAME, UI_HK_END_GAME,
	UI_HK_STATE_LOAD, UI_HK_STATE_SAVE, UI_HK_STATE_PREV, UI_HK_STATE_NEXT, UI_HK_STATE_UNDO, UI_HK_STATE_LOAD_DIALOG, UI_HK_STATE_SAVE_DIALOG,
	UI_HK_MEMCARD_TOGGLE, UI_HK_MAP_INPUTS, UI_HK_DIPS, UI_HK_CHEATS,
	UI_HK_FULLSCREEN, UI_HK_TOGGLE_MENU, UI_HK_FPS, UI_HK_SCREENSHOT, UI_HK_MUTE, UI_HK_VOLUME_UP, UI_HK_VOLUME_DOWN,
	UI_HK_COUNT
};
int UiHotkeyFind(const SDL_KeyboardEvent& key);		// the UI_HK_ action of a key press, -1 if none
bool UiHotkeyUsesKey(int nAction, SDL_Keycode nKey);
const char* UiHotkeyName(int nAction);		// "Ctrl+F9", for the menus
void UiHotkeysSave(FILE* fp);
bool UiHotkeysLoadLine(const char* pszLine);
void UiHotkeysOpen();
void UiHotkeysDraw();
bool UiHotkeysCapturing();					// the key events go to UiHotkeysCaptureKey() then
void UiHotkeysCaptureKey(const SDL_KeyboardEvent& key);
void UiHotkeysTable();

// ui_input.cpp
void UiInputMappingOpen();
void UiDipsOpen();
void UiCheatsOpen();
void UiInputDialogsDraw();
bool UiInputCapturing();					// a control is being mapped: no keyboard/gamepad navigation
void UiInputRedetect();						// gamepads plugged or unplugged

// ui_paths.cpp
void UiRomPathsOpen();
void UiSupportPathsOpen();
void UiPathsDraw();

// ui_filebrowser.cpp, a modal popup drawn inside the caller's window or popup
enum { UI_BROWSE_FOLDER, UI_BROWSE_OPEN, UI_BROWSE_SAVE };
void UiFileBrowserOpen(const char* pszPopup, const char* pszStart, int nMode, const char* pszExtensions);	// ".fs;.fr" or NULL
bool UiFileBrowserDraw(const char* pszPopup, char* pszResult, int nSize);	// true once something is picked

// ui_image.cpp
struct UiZip;
SDL_Texture* UiImageLoad(const char* pszFile, int* pnWidth, int* pnHeight);
UiZip* UiZipOpen(const char* pszFile);
void UiZipClose(UiZip* pZip);
SDL_Texture* UiImageLoadZip(UiZip* pZip, const char* pszEntry, int* pnWidth, int* pnHeight);
SDL_Texture* UiImageLoadFromZipFile(const char* pszZip, const char* pszEntry, int* pnWidth, int* pnHeight);

// ui_popup.cpp
void UiPopupsDraw();
void UiPopupFlush();						// shows the text queued by AppError() and friends
bool UiPopupActive();
void UiProgressStart(const char* pszTitle);	// progress window while a game loads
void UiProgressEnd();
