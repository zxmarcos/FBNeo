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
    dependency('opengl'),
]

if get_option('sdl_gui') == 'imgui'
    frontend_sources += files(
        'ui/ui_app.cpp',
        'ui/ui_core.cpp',
        'ui/ui_gamesel.cpp',
        'ui/ui_menubar.cpp',
        'ui/ui_popup.cpp',
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
    frontend_inc += ['ui', imgui_dir, imgui_dir / 'backends']
    frontend_deps += declare_dependency(link_with: imgui_lib)
endif

frontend_dep = declare_dependency(
    include_directories: include_directories(frontend_inc),
    dependencies: frontend_deps,
    compile_args: sdl2_defs,
)

frontend_link_args = []
