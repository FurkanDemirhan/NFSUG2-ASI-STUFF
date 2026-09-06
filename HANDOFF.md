# Need For Speed Underground 2 - Code Restoration Project

## 1. Overview & Objective
This project reverse-engineers and restores cut and unused code, gameplay mechanics, debugging features, and assets in the PC release of **Need For Speed: Underground 2 (v1.2 US / NTSC)**. 

To achieve high-accuracy restorations, we cross-reference the PC executable with development and early builds:
- **PS2 Alpha 10 Build** (`SLUS_210.65__(Alpha10R_Bin).ELF` / `./GAME/PS2/Alpha10/`)
- **PS2 Demo Build** (`SLUS_291.18_(PS2DEMO).ELF` / `./GAME/PS2/Demo/`)
- **GameCube NTSC Build** (IDA 9.0 DB: `NFSUnderground2-Gamecube-NTSC.i64` — 14,598 named functions)
- **PC v1.2 NTSC Target** (`SPEED2.EXE`, 4,800,512 bytes / IDA 9.0 DB: `NFSUnderground2-v1.2-US.i64` — 4,065 named functions)
- **Reference Material**: NFSU2 Extra Options (`./Reference-Code/NFSU2ExOpts/`)

---

## 2. Technical Stack & Execution Model
- **Host Platform**: NixOS (Linux). All commands must run inside `nix-shell` and only within the workspace directory.
- **Nix Environment (`shell.nix`)**:
  - 32-bit MinGW toolchain: `i686-w64-mingw32-gcc` / `g++` (GCC 15.3.0).
  - Wine: `pkgs.wineWow64Packages.stagingFull`.
  - Python: `python3` with `pyelftools`, `pefile`, `capstone`.
  - Local Python venv (`.venv`): `python-idb` and `capstone` for reading IDA Pro 9.0 `.i64` databases and disassembling.
- **Target Architecture**: 32-bit x86 Windows Dynamic Link Library (`.asi`).
- **Hooking Framework**: Header-only `injector` library under `src/includes/injector/`.
- **Configuration**: INI file parser under `src/includes/IniReader.h`.
- **Plugin Deployment**: `GAME/PC/scripts/NFSU2CodeRestoration.asi` & `NFSU2CodeRestoration.ini`.
- **Wine Launch Overrides**: `WINEDLLOVERRIDES="dinput8=n,b"` (loading Ultimate ASI Loader via `dinput8.dll`).

---

## 3. Directory Layout & Git Policy
Only project source, scripts, environment, and documentation are tracked by Git:
- `src/` — C++ source code for the restoration `.asi` plugin:
  - `src/dllmain.cpp`: Compatibility check against v1.2 NTSC entry point (`0x75BCC7`), orchestrates restorations.
  - `src/Config.h` / `Config.cpp`: Reads `NFSU2CodeRestoration.ini`.
  - `src/Logger.h` / `Logger.cpp`: Runtime logging to `NFSU2CodeRestoration.log`.
  - `src/restorations/DebugCarCustomize.cpp`: Restores cut `UI_DebugCarCustomize.fng` into car customize menu.
  - `src/restorations/CameraRestorations.cpp`: Restores all 5 camera POV modes (Driver, Bumper, Hood, Drift, Chase).
  - `src/restorations/RaceModeRestorations.cpp`: Restores Outrun in Quick Race, Free Run/Outrun track select, AI opponents in Sprint Drift, URL lap count modifier, and "Restart Race" in Pause Menu across all modes.
  - `src/restorations/CustomizationRestorations.cpp`: Restores hidden Special Vinyls category (`0x1C`).
  - `src/restorations/EngineFixes.cpp`: Fixes disappearing wheels in `CarPartCuller` and enables internal `DebugWorldCameraMover`.
  - `src/restorations/GameplayRestorations.cpp`: Main loop tick at `0x581470`, F6 Autopilot (`Player_AutoPilotOn/Off`), F5 Unlock All.
- `scripts/` — Python reverse engineering utilities:
  - `scripts/dump_ida_symbols.py`: Exports functions and names from IDA `.i64` files.
  - `scripts/extract_ps2_symbols.py`: Extracts vtables, source paths, assertions, and strings from PS2 ELFs.
  - `scripts/query_symbols.py`: Instant CLI search across PC, GameCube, and PS2 symbol caches.
- `shell.nix` — Reproducible Nix environment specification.
- `Makefile` — Statically links runtime libraries (`-static`) and deploys to `GAME/PC/scripts/`.
- `HANDOFF.md` — Project roadmap, architecture notes, and progress handoff.
- `.gitignore` — Strictly ignores `GAME/`, `Bin-idb-i64/`, `Reference-Code/`, `build/`, `.venv/`, and `cache/`.

---

## 4. Current Status & Verified Restorations
- [x] Initialized and verified `shell.nix` with MinGW 32-bit compiler and Wine staging.
- [x] Extracted PC v1.2 symbols (4,065 named functions, 15,312 total).
- [x] Extracted GameCube symbols (14,598 named functions — 99.9% coverage).
- [x] Extracted PS2 Alpha 10 and Demo vtables and source tree paths (`/indep/src/...`).
- [x] Cross-platform query tool (`scripts/query_symbols.py`).
- [x] Implemented and verified Restorations:
  - **Debug Car Customization**: Re-enabled cut `UI_DebugCarCustomize.fng` screen (StateID 4) in Customize Car menu.
  - **Camera Modes**: Re-enabled all 5 camera POVs (Driver, Bumper, Hood, Drift, Chase) and scroll limit.
  - **Quick Race Modes**: Restored Outrun in Quick Race menu, unlocked Free Run and Outrun track selection.
  - **Sprint Drift AI**: Restored AI opponents and leaderboard scoring in Sprint Drift races.
  - **URL Lap Controller**: Unlocked the lap modifier for URL races.
  - **Pause Menu Restart**: Restored "Restart Race" button for URL tournaments, Outruns, and all game modes.
  - **Special Vinyls**: Restored hidden vinyl category (`0x1C`).
  - **Engine Fixes**: Disappearing wheels fix in `CarPartCuller`.
  - **Main Loop & Hotkeys**: Per-frame tick hook at `0x581470` for F6 Autopilot (`Player_AutoPilotOn`) and F5 Unlock All.
- [x] Statically linked plugin eliminating `libmcfgthread-2.dll` dependency.
- [x] Successfully verified live loading and execution under Wine with `WINEDLLOVERRIDES="dinput8=n,b"`.
- [x] Runtime log generated and validated: `GAME/PC/NFSU2CodeRestoration.log`.

---

## 5. Next Steps & Research Vectors
1. **Regional Cars & Traffic Car Unlocking**:
   - Investigate unlocking Peugeot 106, Opel Corsa, and traffic vehicle meshes for Quick Race.
2. **Cut HUD / UI Screens**:
   - Restore `LS_LangSelect.fng` (language selection bootflow screen).
   - Investigate split-screen 2P HUD remnants (`2PHudBot.fng`, `2PHudTop.fng`) identified in PS2 Alpha 10.
3. **Cut Audio & Radio Chatter**:
   - Explore unused voice lines and SMS message triggers in GameCube/PS2 builds.
