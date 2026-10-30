frontend_sources = files(
    'bzip.cpp',
    'config.cpp',
    'drv.cpp',
    'inpdipsw.cpp',
    'input_sdl2.cpp',
    'ips_manager.cpp',
    'localise_gamelist.cpp',
    'main.cpp',
    'media.cpp',
    'memcard.cpp',
    '../cdlist.cpp',
    'replay.cpp',
    'romdata.cpp',
    'run.cpp',
    'scrn.cpp',
    'sdl2_gui.cpp',
    'sdl2_gui_common.cpp',
    'sdl2_gui_ingame.cpp',
    'sdl2_inprint.cpp',
    'stated.cpp',
    'stringset.cpp',
    'support_paths.cpp',
    'textfile.cpp',
    'wave.cpp',
    '../../intf/audio/sdl/aud_sdl.cpp',
    '../../intf/cd/cd_img.cpp',
    '../../intf/input/sdl/inp_sdl2.cpp',
    '../../intf/video/sdl/vid_sdl2.cpp',
    '../../intf/video/sdl/vid_sdl2opengl.cpp',
)

sdl2_defs = ['-DBUILD_SDL2', '-DUSE_FILE32API']
if cpp.get_id() != 'msvc'
    sdl2_defs += ['-DHAVE_UNISTD_H']
endif
if host_machine.system() == 'windows'
    sdl2_defs += ['-DSDL_WINDOWS']
endif

frontend_inc = [
    '.',
    '../../intf/input/sdl',
    '../../intf/audio/sdl',
    '../../intf/video/sdl',
]
frontend_deps = [
    dependency('SDL2_image'),
    # libGL.so.1 on Linux, which Mesa installs: the libOpenGL.so.0 of 'opengl'
    # (glvnd) is missing on many systems, the Raspberry Pi OS among them
    dependency(host_machine.system() == 'windows' ? 'opengl' : 'gl'),
]

if get_option('sdl_gui') == 'imgui'
    frontend_sources += files(
        '../gameinfo_text.cpp',
        'cdlibrary.cpp',
        '../gamelist_model.cpp',
        'ui/ui_app.cpp',
        'ui/ui_cdlib.cpp',
        'ui/ui_core.cpp',
        'ui/ui_filebrowser.cpp',
        'ui/ui_game.cpp',
        'ui/ui_gameinfo.cpp',
        'ui/ui_gamesel.cpp',
        'ui/ui_hotkeys.cpp',
        'ui/ui_image.cpp',
        'ui/ui_input.cpp',
        'ui/ui_ips.cpp',
        'ui/ui_menubar.cpp',
        'ui/ui_misc.cpp',
        'ui/ui_paths.cpp',
        'ui/ui_popup.cpp',
        'ui/ui_record.cpp',
        'ui/ui_romdata.cpp',
        'ui/ui_themes.cpp',
        'ui/ui_video.cpp',
        # the software effects of Video > Software Effect
        '../../intf/video/vid_softfx.cpp',
        '../../intf/video/scalers/2xpm.cpp',
        '../../intf/video/scalers/2xsai.cpp',
        '../../intf/video/scalers/crt.cpp',
        '../../intf/video/scalers/ddt3x.cpp',
        '../../intf/video/scalers/epx.cpp',
        '../../intf/video/scalers/hq2xs.cpp',
        '../../intf/video/scalers/hq2xs_16.cpp',
        '../../intf/video/scalers/xbr.cpp',
        '../../intf/video/scalers/xbr32.cpp',
    )

    # the license text of Help > About
    frontend_sources += custom_target(
        'license_text_h',
        input: meson.project_source_root() / 'src/license.txt',
        output: 'license_text.h',
        command: [
            python,
            meson.project_source_root() / 'src/dep/scripts/license2h.py',
            '@INPUT@',
            '-o', '@OUTPUT@',
        ],
        depend_files: files('../../dep/scripts/license2h.py'),
    )

    # Dear ImGui with the SDL2 + SDL_Renderer backends, built on its own so
    # the emulator warning flags don't apply to it
    imgui_dir = '../../dep/libs/imgui'
    imgui_lib = static_library(
        'imgui',
        files(
            imgui_dir / 'imgui.cpp',
            imgui_dir / 'imgui_draw.cpp',
            imgui_dir / 'imgui_tables.cpp',
            imgui_dir / 'imgui_widgets.cpp',
            imgui_dir / 'backends/imgui_impl_sdl2.cpp',
            imgui_dir / 'backends/imgui_impl_sdlrenderer2.cpp',
        ),
        include_directories: include_directories(imgui_dir, imgui_dir / 'backends'),
        dependencies: dependency('sdl2'),
    )
    # zip.h for the preview and icon archives (zip.c is in the burner lib)
    frontend_inc += ['ui', imgui_dir, imgui_dir / 'backends', '../../dep/libs/zip']
    if cpp.get_id() == 'msvc'
        frontend_inc += '../../dep/vc/include'		# dirent.h, for the file browser
    endif
    frontend_deps += declare_dependency(link_with: imgui_lib)
endif

frontend_dep = declare_dependency(
    include_directories: include_directories(frontend_inc),
    dependencies: frontend_deps,
    compile_args: sdl2_defs,
)

frontend_link_args = []
