# FinalBurn Neo for SDL1.2/2 

## Differences between SDL1.2 and SDL2 versions

The SDL2 port is the recommended version as it has the following features over the SDL1.2 port:

* A better renderer
* OSD  - shows when fast-forwarding and when FPS display is enabled via F12
* Game selection menu which also replicates the rom scanning functionality of the Windows version of FBNeo. Handy for cabinets!
* A lot more testing

## Compiling
### SDL1.2

Assuming you have a working GCC (Mingw and GCC under ubuntu 18.04 have been tested) you can just install libsdl 1.2 and type:

`make sdl`

And out will pop an fbneo executable.

### SDL2

Assuming you have a working GCC (Mingw and GCC under ubuntu 18.04 have been tested) you can just install libsdl 2 and libsdl2-image and type:

'make sdl2'

and out will pop an fbneo executable.

### SDL2 on Windows with MSVC (meson)

The meson build only needs Python (the code generators in `src/dep/scripts/*.py`), no perl.

* Install Visual Studio 2022 (C++ workload) and `pip install meson ninja`
* Get the SDL2 and SDL2_image `-devel-*-VC.zip` packages from https://github.com/libsdl-org and write `sdl2.pc` / `SDL2_image.pc` files pointing at them (`Cflags: -I<sdl>/include`, `Libs: -L<sdl>/lib/x64 -lSDL2main -lSDL2 -lshell32`, and `-lSDL2_image` for SDL2_image); a `pkg-config`/`pkgconf` executable must be available
* `meson setup build-sdl -Dfrontend=sdl2 --vsenv --buildtype=release --pkg-config-path=<dir with the .pc files>`
* `meson compile -C build-sdl`
* copy `SDL2.dll` and `SDL2_image.dll` next to `build-sdl/fbneo.exe`

### Dear ImGui interface

The meson build uses the new Dear ImGui interface by default (`-Dsdl_gui=imgui`): one window for the game selector and the game, a menu bar, message boxes for the loading errors and a game menu on Tab. `-Dsdl_gui=legacy` builds the old menus.
With make it is opt-in for now: `make INCLUDE_IMGUI=1 sdl2`.

Video menu: fullscreen (desktop, or exclusive with a resolution and a display for the horizontal and the vertical games, auto switch after loading), window size, stretch (normal, full, correct aspect ratio, integer scaling), monitor aspect ratio, rotation of the vertical games, gamma, scanlines, the software effects (Scale2x, hq2xS, xBR, CRT...), filtering and vsync. Audio menu: volume up to 200% and mute, the SDL output driver and device, sample rate, buffer size, low-pass filter and interpolation. Everything is kept in `fbneo.ini`; the video options of the command line are used for that run only.

Input menu: map the game inputs (F5: keyboard, gamepads and mouse, presets, macros with auto-fire), DIP switches (Ctrl+F5), cheats (Shift+F5), auto-fire rate, SOCD per player, rewind and the hotkeys editor (Input > Hotkeys, with a "Windows Layout" button for the keys of the Windows build).

The game selector has the filters of the Windows build (board type, family, genre, hardware, favorites), the parent/clone tree, the title and preview images (`support/titles`, `support/previews` or their `titles.zip`/`previews.zip`), the driver icons (Misc > Selector Icons, from `support/icons` or `icons.zip`) and the Neo Geo MVS cartridge slots (play "Neo Geo MVS system"). The ROM and support paths are set in Misc > ROM Paths and Misc > Support Paths. The favorites are kept in `favorites.dat` of the config folder, in the format of the Windows build.

Game menu: recent games, quick load of a ROM archive or of a CD image (Neo Geo CD, PC Engine CD), pause and frame advance, fast forward, slow motion, CPU speed, save states (8 slots with undo, or files), memory cards. Keys: F9/F10 load/save the active slot, F8/F7 previous/next slot, Shift+F8 undo the last save, Ctrl+F9/F10 state files, F1 fast forward (frame advance when paused), Shift+F1 fast forward on/off, Page Up rewind (Input > Rewind).

The original titles of the games (Japanese, Chinese, Korean) use the fonts of the system: on Linux install the Noto CJK fonts, or tick "Latin text only" in the selector.

### SDL2 + CRT mode switching

Linux now supports modeswitching for CRT monitors using https://github.com/antonioginer/switchres (which is the switchres code of GroovyMAME extracted by its author as a library for emulators that wish to implement resolution switching). In order to get this to work, you need to:
  * `git clone https://github.com/antonioginer/switchres.git` + `make` then `make install`. You'll also need to copy the repos `switchres.ini` to `/etc` and customize it. Monitors presets can be found at http://forum.arcadecontrols.com/index.php/topic,116023.msg1230485.html#msg1230485
  * build fbneo with `make INCLUDE_SWITCHRES=1 sdl2`

Current known limitations:
  * the SDL2 backend doesn't support rom interlace. Although switchres can compute an interlace resolution, FBNeo isn't yet passing that flag.
  * Vertical screens are not detected, so vertical games will always be rendered as horizontal
  * in-game resolution switching is not yet supported
  * if the monitor's refresh rate is too different from the game refresh rate, no mechanism comes to smooth the video output, so tearing can happen.

## Running

### SDL1.2

* First run

run the fbneo executable and an fbneo.ini will be created. You will need to edit this to point to your rom directories. You can also edit any of the other otions in the ini file as needed

* subsiquent runs

type 

'fbneo <romname>' 

where <romname> is the name of a supported rom. For example

'fbneo sf2'

will run sf2. 

### SDL2

'-cd' used when running a neocd game. You'll have to look in the code to work out where the the CD images should be placed

'-joy' enable joystick

'-menu' load the menu

'-novsync' disable vsync

'-integerscale' only scale to the closest integer value. This will cause a border around the games unless you are running at a resolution that is a whole multiple of the games original resolution

'-fullscreen' enable fullscreen mode

'-dat' generate dat files

'-autosave' autosave/autoload a save state when starting or exiting a game

'-nearest' enbable nearest neighbour filtering (e.g. just scale the pixels)

'-linear' enable linear filter (or is it a bilinear filter) to smooth out pixels

'-best' enable sdl2 'best' filtering, which actually makes the games look the worst
 

recommend command line options:

'fbneo -menu -integerscale -fullscreen -joy'

The above will give you a nicely scalend game screen and the menu for launching games. 

## In-game controls

With the ImGui interface: 'TAB' opens the game menu, 'Pause' pauses, F12 ends the game (back to the selector if it was open, quits otherwise), and the menu bar has the rest. The other keys below are the same.

'TAB' - brings up the in game menu (or game PAUSE)
'F12' - quit game.
'F1' - fast forward game.
'F11' - show FPS counter
'F6' - Screenshot
'ALT-ENTER' - Switch window/fullscreen
'+'/'-' - Volume controls

## SDL2 in menu controls

'F1' - Rescan current roms
'F2' - enable/disable filtering
'F3' - Swap current system
'F12' - quit menu. This will return you to the game select menu if run with '-menu'. Press 'f12' again to quit 
'q'/'w' - Skip to next letter
'ALT-ENTER' - Switch window/fullscreen