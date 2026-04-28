# GAMBODJAN

Unified Dota 2 project combining six open-source repositories into a single codebase with a clean menu.

## Sources

| Project | Description | Platform |
|---------|-------------|----------|
| **McDota** | Cheat/tinker tool with Panorama UI, ESP, Zoom, Fog, protobuf interception | Linux |
| **dota_new_hack** | Camera hack, Hero bar, Inventory changer, Tree changer, ImGui GUI | Windows |
| **dota2dumped** | SDK dumps (hpp, json, cs, go, rs) | Reference |
| **GDRVMapper** | Kernel driver mapper via gdrv.sys (EAC/BE bypass) | Windows |
| **64KernelDriverCleaner** | Clean driver traces (PiDDB, HashBucket, MMU/MML) | Windows (WDK) |
| **dataptr-injector** | Dataptr COM injector with kernel driver + usermode | Windows |

## Project Structure

```
GAMBODJAN/
|-- CMakeLists.txt             # Cross-platform build (CMake)
|-- src/
|   |-- main.cpp               # Windows entry (DllMain)
|   |-- main.hpp               # Windows main header
|   |-- McDota.cpp             # Linux entry
|   |-- Interfaces.cpp/h       # Linux interface grabbing
|   |-- Netvars.cpp/h          # Linux netvars
|   |-- core/
|   |   |-- features/          # All game features
|   |   |   |-- camera_hack    # Camera distance & fog
|   |   |   |-- hero_bar       # Custom HP/mana bars
|   |   |   |-- inventory_changer  # Fake items
|   |   |   |-- overwolf       # Overwolf integration
|   |   |   |-- tree_changer   # Custom tree models
|   |   |   |-- Esp            # ESP (from McDota)
|   |   |   |-- Zoom           # Zoom (from McDota)
|   |   |-- hook/              # Hooking system
|   |   |-- sdk_game/          # Game SDK headers
|   |   |-- lib/               # Libraries (ImGui, MinHook, etc.)
|   |   |-- util/              # Utilities
|   |-- gui/
|   |   |-- menu.cpp/h         # Unified ImGui menu (NEW)
|   |   |-- gui.cpp/hpp        # Original ImGui GUI
|   |   |-- panorama_gui       # Panorama GUI
|   |   |-- Gui.cpp/h          # McDota Panorama GUI
|   |   |-- Callbacks.cpp/h    # McDota UI callbacks
|   |-- hooks/                 # McDota hooks (Linux)
|   |-- sdk/                   # McDota SDK headers
|   |-- Utils/                 # subhook library
|-- tools/
|   |-- gdrv_mapper/           # GDRVMapper (driver loader)
|   |   |-- main.cpp           # Entry point
|   |   |-- load.cpp           # Driver loading logic
|   |   |-- pe.cpp             # PE parser
|   |   |-- sys.cpp            # System driver operations
|   |   |-- hde/               # HDE64 disassembler
|   |   |-- gdrv.sys           # Signed driver (for C:\Windows\System32\drivers)
|   |-- driver_cleaner/        # 64KernelDriverCleaner
|   |   |-- entry/main.cpp     # Driver entry point
|   |   |-- kernel.cpp         # Kernel operations
|   |   |-- HashBucket/        # HashBucket cleaning
|   |   |-- MMU_MML/           # MMU/MML cleaning
|   |   |-- PiDDB/             # PiDDB cache cleaning
|   |-- dataptr_injector/      # dataptr-injector
|       |-- entry.cpp          # Kernel driver entry
|       |-- hook/              # Hook functions
|       |-- memory/            # Memory operations
|       |-- pe/                # PE operations
|       |-- pte/               # Page table operations
|       |-- security/          # Process hiding/protection
|       |-- usermode/          # Usermode injector app
|           |-- mapper/        # KDMapper / Intel driver
|           |-- DllMapper/     # DLL mapping
|           |-- interface/     # Communication interface
|           |-- logger/        # Logging
|           |-- util/          # Utilities
|-- sdk_dumps/                 # Dota 2 SDK dumps
|   |-- hpp/                   # C++ headers
|   |-- json/                  # JSON format
|   |-- cs/                    # C# format
|   |-- dota2/                 # Updated Dota 2 dumps
|       |-- hpp/ json/ cs/ go/ rs/
|-- scripts/                   # Linux load/unload scripts
|-- injector/                  # Windows DLL injector (simple)
|-- dumper/                    # SDK dumper tool
```

## Menu (8 tabs)

The unified menu (`src/gui/menu.cpp`) includes:

- **Visuals** -- ESP, hero bars, creep coloring
- **Camera** -- Distance control, fog toggle, zoom
- **World** -- Weather, river type, tree changer
- **Inventory** -- Item changer, Overwolf
- **Auto Accept** -- Match auto-accept with delay
- **Panorama** -- Symbol tools, MMR display
- **Tools** -- GDRVMapper, Driver Cleaner, Dataptr Injector
- **Settings** -- Info & hotkeys

### Hotkeys

| Key | Action |
|-----|--------|
| `INSERT` | Toggle menu |
| `Mouse Wheel` | Camera zoom (when enabled) |

## Build

### Main cheat DLL (Windows)

Requirements: Visual Studio 2022 with C++ workload (includes CMake and Ninja).

All dependencies (protobuf, ImGui, MinHook, json) are bundled in the repo —
no vcpkg, NuGet, or internet connection required.

**Option A — Open folder in Visual Studio 2022:**

1. Open Visual Studio 2022
2. File → Open → Folder → select the project root
3. Visual Studio auto-detects `CMakePresets.json`
4. Select preset **x64-Release** (or x64-Debug)
5. Build → Build All

**Option B — Command line (Developer Command Prompt):**

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `release/GAMBODJAN_dll.dll`, `release/GAMBODJAN.exe`

### Main cheat SO (Linux)

Prerequisites: `cmake`, `gcc >= 7.1`, `protobuf 3.15.3`

```bash
./scripts/rebuildprotos.sh
cmake -B build
cmake --build build -j$(nproc)
```

Output: `build/libGAMBODJAN.so`

### 64KernelDriverCleaner (Windows, WDK)

This is a kernel-mode driver. Build via Visual Studio with WDK installed:
1. Install WDK from https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
2. Open `tools/driver_cleaner/` as a WDK project
3. Build for x64 Release

### GDRVMapper

Built automatically with the main project (CMake). Uses the `gdrv.sys` from `tools/gdrv_mapper/`.

Setup: Copy `gdrv.sys` to `C:\Windows\System32\drivers\`

### Dataptr Injector

Usermode component built automatically with CMake. Kernel driver requires WDK.

## Usage

### Windows
1. Build `GAMBODJAN.dll`
2. Copy `gdrv.sys` to `C:\Windows\System32\drivers\` (for driver loading)
3. Inject DLL into `dota2.exe` using any injector
4. Press `INSERT` to open the menu

### Linux
1. Build `libGAMBODJAN.so`
2. Use load scripts: `./scripts/load` or `./scripts/load-stealth`
3. Press `INSERT` to open the menu

## Credits

- **McDota** by LWSS
- **dota_new_hack** by F0RQU1N
- **dota2dumped** -- SDK dumps
- **GDRVMapper** by DErDYAST1R
- **64KernelDriverCleaner** by DErDYAST1R
- **dataptr-injector** by DErDYAST1R

## License

GPL-3.0
