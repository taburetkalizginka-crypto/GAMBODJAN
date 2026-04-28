cmake_minimum_required(VERSION 3.15)
project(GAMBODJAN LANGUAGES CXX C)

if(WIN32)
    set(CMAKE_CXX_STANDARD 20)
    enable_language(ASM_MASM)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Platform detection
# ============================================================
if(WIN32)
    message(STATUS "Building for Windows (EXE injector + Cheat DLL)")
    add_definitions(-DGAMBODJAN_WINDOWS)
elseif(UNIX AND NOT APPLE)
    message(STATUS "Building for Linux (shared-object injection)")
    add_definitions(-DGAMBODJAN_LINUX)
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ============================================================
# Compiler flags
# ============================================================
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Wno-unused-parameter -fPIC -pipe")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -fvisibility=hidden")
elseif(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/W1> $<$<COMPILE_LANGUAGE:C,CXX>:/MP> $<$<COMPILE_LANGUAGE:C,CXX>:/EHsc> $<$<COMPILE_LANGUAGE:C,CXX>:/GL>)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/wd4244> $<$<COMPILE_LANGUAGE:C,CXX>:/wd4267>)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
endif()

# ============================================================
# Include directories
# ============================================================
include_directories(
    src
    src/core
    src/core/lib
    src/core/lib/protobuf/protobuf
    src/gui
)

if(UNIX)
    include_directories(
        src/core/sdk_game
        src/sdk
        sdk_dumps/hpp
    )
endif()

# ============================================================
# ImGui sources
# ============================================================
set(IMGUI_SOURCES
    src/core/lib/imgui/imgui.cpp
    src/core/lib/imgui/imgui_demo.cpp
    src/core/lib/imgui/imgui_draw.cpp
    src/core/lib/imgui/imgui_impl_dx11.cpp
    src/core/lib/imgui/imgui_impl_win32.cpp
    src/core/lib/imgui/imgui_stdlib.cpp
    src/core/lib/imgui/imgui_tables.cpp
    src/core/lib/imgui/imgui_widgets.cpp
)

# MinHook sources
set(MINHOOK_SOURCES
    src/core/lib/minhook/src/buffer.c
    src/core/lib/minhook/src/hook.c
    src/core/lib/minhook/src/trampoline.c
    src/core/lib/minhook/src/hde/hde32.c
    src/core/lib/minhook/src/hde/hde64.c
)

# Protobuf generated message sources
file(GLOB PROTOBUF_PB_SOURCES src/core/lib/protobuf/files/*.pb.cc)

# ============================================================
# Windows targets
# ============================================================
if(WIN32)
    # --- GAMBODJAN.exe (Injector / Launcher — NO full menu) ---
    add_executable(GAMBODJAN_Launcher WIN32
        src/main_exe.cpp
        src/gui/menu.cpp
        ${IMGUI_SOURCES}
    )
    target_link_libraries(GAMBODJAN_Launcher PRIVATE d3d11 dxgi dwmapi comdlg32)
    set_target_properties(GAMBODJAN_Launcher PROPERTIES OUTPUT_NAME "GAMBODJAN")

    # --- External dependencies for the full DLL ---
    # protobuf: required for .pb.cc generated message files
    # Try vcpkg / system protobuf; fall back to header-only stub
    find_package(Protobuf QUIET)
    if(NOT Protobuf_FOUND)
        # If protobuf is not installed, try pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PROTOBUF QUIET protobuf)
        endif()
    endif()

    # curl: required for overwolf.cc (player lookup)
    find_package(CURL QUIET)

    # --- GAMBODJAN_dll.dll (REAL cheat DLL — hooks game D3D, full features) ---
    set(CHEAT_SOURCES
        src/main.cpp
        src/gui/gui.cpp
        src/gui/menu.cpp
        src/gui/panorama_gui.cc
        src/core/hook/hook.cpp
        src/core/features/camera_hack.cc
        src/core/features/hero_bar.cc
        src/core/features/inventory_changer.cc
        src/core/features/overwolf.cc
        src/core/features/tree_changer.cc
        src/core/sdk_game/CEconItem.cpp
        src/core/sdk_game/CDOTA_ParticleManager.cpp
        src/core/util/drawing.cc
        src/core/util/fnv1a.cc
        src/core/util/request.cc
        src/core/util/util.cpp
        src/core/util/spoof_call.asm
    )

    add_library(GAMBODJAN_DLL SHARED
        ${CHEAT_SOURCES}
        ${IMGUI_SOURCES}
        ${MINHOOK_SOURCES}
        ${PROTOBUF_PB_SOURCES}
    )
    target_compile_definitions(GAMBODJAN_DLL PRIVATE GAMBODJAN_CHEAT_DLL)
    target_include_directories(GAMBODJAN_DLL PRIVATE
        src/core/lib/protobuf/protobuf
    )
    target_link_libraries(GAMBODJAN_DLL PRIVATE
        d3d11 dxgi dwmapi wininet ws2_32
    )

    # Link protobuf if found
    if(Protobuf_FOUND)
        target_link_libraries(GAMBODJAN_DLL PRIVATE protobuf::libprotobuf)
    elseif(PROTOBUF_FOUND)
        target_include_directories(GAMBODJAN_DLL PRIVATE ${PROTOBUF_INCLUDE_DIRS})
        target_link_libraries(GAMBODJAN_DLL PRIVATE ${PROTOBUF_LIBRARIES})
    else()
        message(WARNING
            "Protobuf NOT found. GAMBODJAN_DLL will have linker errors.\n"
            "Install protobuf via vcpkg: vcpkg install protobuf:x64-windows\n"
            "Or use the overlay DLL (GAMBODJAN_Overlay) which works without protobuf.")
    endif()

    # Link curl if found
    if(CURL_FOUND)
        target_link_libraries(GAMBODJAN_DLL PRIVATE CURL::libcurl)
    else()
        message(WARNING
            "CURL NOT found. Overwolf feature will have linker errors.\n"
            "Install curl via vcpkg: vcpkg install curl:x64-windows\n"
            "Or use the overlay DLL (GAMBODJAN_Overlay) which works without curl.")
    endif()

    if(MSVC)
        target_link_options(GAMBODJAN_DLL PRIVATE
            /LTCG
            /IGNORE:4217
            /IGNORE:4049
        )
    endif()
    set_target_properties(GAMBODJAN_DLL PROPERTIES OUTPUT_NAME "GAMBODJAN_dll")

    # --- GAMBODJAN_overlay.dll (Overlay DLL — standalone, no game hooks) ---
    # This is the MINIMAL WORKING version: creates a transparent D3D11
    # overlay window on top of the game, renders the full ImGui menu,
    # and implements camera hack via writing CFG files.
    # No pattern scanning, no MinHook, no protobuf, no curl needed.
    add_library(GAMBODJAN_Overlay SHARED
        src/dll_main.cpp
        src/gui/menu.cpp
        ${IMGUI_SOURCES}
    )
    target_link_libraries(GAMBODJAN_Overlay PRIVATE d3d11 dxgi dwmapi)
    set_target_properties(GAMBODJAN_Overlay PROPERTIES OUTPUT_NAME "GAMBODJAN_overlay")

    # --- Loader.exe (kdmapper-based loader) ---
    set(LOADER_SOURCES
        tools/dataptr_injector/usermode/eac-usermode.cpp
        tools/dataptr_injector/usermode/interface/interface.cpp
        tools/dataptr_injector/usermode/logger/logger.cpp
        tools/dataptr_injector/usermode/mapper/intel_driver.cpp
        tools/dataptr_injector/usermode/mapper/kdmapper.cpp
        tools/dataptr_injector/usermode/mapper/portable_executable.cpp
        tools/dataptr_injector/usermode/mapper/service.cpp
        tools/dataptr_injector/usermode/mapper/utils.cpp
        tools/dataptr_injector/usermode/util/sigscan.cpp
        tools/dataptr_injector/usermode/DllMapper/DllMapper.cpp
    )

    add_executable(GAMBODJAN_Loader
        ${LOADER_SOURCES}
    )
    target_include_directories(GAMBODJAN_Loader PRIVATE
        tools/dataptr_injector
        tools/dataptr_injector/usermode
        tools/dataptr_injector/usermode/mapper
        tools/dataptr_injector/usermode/util
    )
    target_link_libraries(GAMBODJAN_Loader PRIVATE ntdll version advapi32 setupapi newdev)
    set_target_properties(GAMBODJAN_Loader PROPERTIES OUTPUT_NAME "loader")

    # ============================================================
    # Post-build: copy everything to release/ folder
    # ============================================================
    set(RELEASE_DIR ${CMAKE_SOURCE_DIR}/release)

    add_custom_command(TARGET GAMBODJAN_Launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GAMBODJAN_Launcher> ${RELEASE_DIR}/GAMBODJAN.exe
    )
    add_custom_command(TARGET GAMBODJAN_DLL POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GAMBODJAN_DLL> ${RELEASE_DIR}/GAMBODJAN_dll.dll
    )
    add_custom_command(TARGET GAMBODJAN_Loader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GAMBODJAN_Loader> ${RELEASE_DIR}/loader.exe
    )
    add_custom_command(TARGET GAMBODJAN_Overlay POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GAMBODJAN_Overlay> ${RELEASE_DIR}/GAMBODJAN_overlay.dll
    )
    # gdrv.sys is already in release/ folder (pre-built binary)

# ============================================================
# Linux target — Cross-platform Launcher (GLFW + OpenGL3)
# ============================================================
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW3 REQUIRED glfw3)
    find_package(OpenGL REQUIRED)

    set(IMGUI_GLFW_SOURCES
        src/core/lib/imgui/imgui.cpp
        src/core/lib/imgui/imgui_demo.cpp
        src/core/lib/imgui/imgui_draw.cpp
        src/core/lib/imgui/imgui_tables.cpp
        src/core/lib/imgui/imgui_widgets.cpp
        src/core/lib/imgui/imgui_stdlib.cpp
        src/core/lib/imgui/imgui_impl_glfw.cpp
        src/core/lib/imgui/imgui_impl_opengl3.cpp
    )

    # --- GAMBODJAN Launcher (cross-platform, GLFW + OpenGL3) ---
    add_executable(GAMBODJAN_Launcher
        src/main_glfw.cpp
        src/gui/menu.cpp
        ${IMGUI_GLFW_SOURCES}
    )
    target_include_directories(GAMBODJAN_Launcher PRIVATE
        src
        src/core
        src/core/lib
        src/gui
        ${GLFW3_INCLUDE_DIRS}
    )
    target_link_libraries(GAMBODJAN_Launcher PRIVATE
        ${GLFW3_LIBRARIES}
        OpenGL::GL
        dl
        pthread
    )
    set_target_properties(GAMBODJAN_Launcher PROPERTIES OUTPUT_NAME "gambodjan")
endif()

# ============================================================
# Install
# ============================================================
if(WIN32)
    install(TARGETS GAMBODJAN_Launcher RUNTIME DESTINATION bin)
    install(TARGETS GAMBODJAN_DLL LIBRARY DESTINATION bin RUNTIME DESTINATION bin)
    install(TARGETS GAMBODJAN_Overlay LIBRARY DESTINATION bin RUNTIME DESTINATION bin)
    install(TARGETS GAMBODJAN_Loader RUNTIME DESTINATION bin)
    install(FILES tools/gdrv_mapper/gdrv.sys DESTINATION bin)
else()
    install(TARGETS GAMBODJAN_Launcher RUNTIME DESTINATION bin)
endif()
