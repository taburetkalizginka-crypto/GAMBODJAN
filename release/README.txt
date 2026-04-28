GAMBODJAN Release Folder
========================

After building the project, this folder should contain:

  GAMBODJAN.exe         - Launcher with DLL injector (standalone ImGui window)
  GAMBODJAN_overlay.dll - Overlay DLL (minimal working version, no external deps)
  GAMBODJAN_dll.dll     - Full cheat DLL (hooks game D3D, all features, needs protobuf+curl)
  loader.exe            - Mapper/injector (kernel-level, needs gdrv.sys + WDK driver)
  gdrv.sys              - GIGABYTE vulnerable kernel driver (pre-built)

================================================================
  QUICK START (Minimal Working Version)
================================================================

  1. Build GAMBODJAN_Launcher + GAMBODJAN_Overlay targets (see BUILD below)
  2. Launch Dota 2 via Steam
  3. Run GAMBODJAN.exe as Administrator
  4. Click Browse, select GAMBODJAN_overlay.dll
  5. Click INJECT INTO DOTA 2
  6. Press INSERT in-game to open the cheat menu

  The overlay DLL creates a transparent window on top of Dota 2 with the
  full ImGui menu. It does NOT require protobuf, curl, or any external
  libraries. Camera hack works by writing CFG files.

================================================================
  BUILD INSTRUCTIONS
================================================================

  Requirements:
    - Windows 10/11 x64
    - Visual Studio 2022 (with C++ Desktop workload)
    - CMake 3.15+

  Step 1: Open Developer Command Prompt for VS 2022

  Step 2: Generate and build (minimal targets only):

    cd GAMBODJAN
    mkdir build && cd build
    cmake .. -G "Visual Studio 17 2022" -A x64
    cmake --build . --config Release --target GAMBODJAN_Launcher GAMBODJAN_Overlay

  This builds:
    - GAMBODJAN.exe         (Launcher/Injector)
    - GAMBODJAN_overlay.dll (Overlay DLL with full menu)

  Step 3 (optional): Build the full DLL (requires extra libraries):

    Install via vcpkg:
      vcpkg install protobuf:x64-windows curl:x64-windows

    Then build with vcpkg toolchain:
      cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
      cmake --build . --config Release --target GAMBODJAN_DLL

================================================================
  USAGE
================================================================

  Simple (GAMBODJAN.exe + overlay DLL):
    1. Launch Dota 2
    2. Run GAMBODJAN.exe as Administrator
    3. Browse and select GAMBODJAN_overlay.dll (or GAMBODJAN_dll.dll)
    4. Click INJECT INTO DOTA 2
    5. Press INSERT in-game to open the cheat menu

  Advanced (loader.exe + kernel driver):
    1. Copy gdrv.sys to C:\Windows\System32\drivers\
    2. Build driver.sys with WDK (from tools/dataptr_injector/)
    3. Put loader.exe + gdrv.sys + driver.sys + GAMBODJAN_dll.dll in one folder
    4. Run loader.exe as Administrator
    5. Press INSERT in-game to open the cheat menu

================================================================
  IN-GAME HOTKEYS
================================================================

  INSERT     - Open/close menu
  END        - Unload overlay DLL
  HOME       - Unload full cheat DLL
  PgUp/PgDn  - Camera zoom in/out (when enabled in menu)
  F3         - Show Panorama GUI overlay (full DLL only)

================================================================
  TARGETS OVERVIEW
================================================================

  GAMBODJAN_Launcher (GAMBODJAN.exe):
    - Standalone Windows app with D3D11/ImGui injector UI
    - No external dependencies beyond Windows SDK
    - Browse DLL, find dota2.exe, inject via CreateRemoteThread

  GAMBODJAN_Overlay (GAMBODJAN_overlay.dll):
    - Creates transparent D3D11 overlay window on top of the game
    - Full ImGui menu with all settings tabs (ESP, Camera, World, etc.)
    - Camera hack via writing CFG files (safe, no memory scanning)
    - No game-specific pattern scanning, no MinHook, no protobuf, no curl
    - Works independently of game version updates

  GAMBODJAN_DLL (GAMBODJAN_dll.dll):
    - Hooks into game's D3D11 pipeline via GameOverlayRenderer
    - MinHook-based function hooking
    - Pattern scanning for game functions (may need updates per game patch)
    - Full features: ESP hero bars, abilities, items, camera hack, inventory changer
    - Requires protobuf and curl libraries

  GAMBODJAN_Loader (loader.exe):
    - Kernel-mode mapper via gdrv.sys vulnerability
    - Loads driver.sys (must be compiled separately with WDK)

================================================================
  MENU FEATURES
================================================================

  Visuals:
    - ESP (health bars, mana bars, names, boxes, lines)
    - Abilities & items with cooldown timers
    - Colored creep bars

  Camera:
    - Camera distance override (unlimited zoom)
    - PgUp/PgDn zoom control
    - Fog control / fog of war removal
    - FOV override

  World:
    - Weather changer (10 weather types)
    - River changer (8 river types)
    - Tree changer (12 tree models)
    - Sky changer (10 sky types)
    - Particle effects

  Inventory:
    - Inventory changer (item spoofing)
    - Overwolf mode
    - Profile spoof (fake MMR, rank)

  Auto Accept:
    - Auto-accept matchmaking
    - Accept at last moment
    - Auto pause / unpause

  Tools:
    - GDRV mapper interface
    - Dataptr injector launcher
    - Driver cleaner (PiDDB, HashBucket, MMU)

  Settings:
    - Config save/load (preset files)
    - Theme selection
    - Menu opacity control
    - Quick presets (Default, Competitive, Casual)
