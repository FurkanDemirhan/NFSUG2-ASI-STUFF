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
  - 32-bit MinGW toolchain: `i686-w64-mingw32-gcc` / `g++`.
  - Wine: `pkgs.wineWow64Packages.stagingFull`.
  - Python: `python3` with `pyelftools`, `pefile`, `capstone`.
  - Local Python venv (`.venv`): `python-idb` for reading IDA Pro 9.0 `.i64` databases.
- **Target Architecture**: 32-bit x86 Windows Dynamic Link Library (`.asi`).
- **Hooking Framework**: Header-only `injector` library under `src/includes/injector/`.
- **Configuration**: INI file parser under `src/includes/IniReader.h`.
- **Plugin Deployment**: `GAME/PC/scripts/NFSU2CodeRestoration.asi`.
- **Wine Launch Overrides**: `WINEDLLOVERRIDES="dinput8=n,b"` (loading Ultimate ASI Loader via `dinput8.dll`).

---

## 3. Directory Layout & Git Policy
Only project source, scripts, environment, and documentation are tracked by Git:
- `src/` — C++ source code for the restoration `.asi` plugin.
- `scripts/` — Python and shell reverse engineering utilities:
  - `scripts/dump_ida_symbols.py`: Exports functions and names from IDA `.i64` files.
  - `scripts/extract_ps2_symbols.py`: Extracts vtables, source paths, assertions, and strings from PS2 ELFs.
  - `scripts/query_symbols.py`: Instant CLI search across PC, GameCube, and PS2 symbol caches.
- `shell.nix` — Reproducible Nix environment specification.
- `Makefile` — Builds 32-bit `.asi` via MinGW and deploys to `GAME/PC/scripts/`.
- `HANDOFF.md` — Project roadmap, architecture notes, and progress handoff.
- `.gitignore` — Strictly ignores `GAME/`, `Bin-idb-i64/`, `Reference-Code/`, `build/`, `.venv/`, and `cache/`.

---

## 4. Current Accomplishments
- [x] Initialized and verified `shell.nix` with MinGW 32-bit compiler and `wineWow64Packages.stagingFull`.
- [x] Initialized Git repository with strict `.gitignore`.
- [x] Verified pure-Python IDA 9.0 `.i64` database parsing via `python-idb`.
- [x] Extracted and cached PC v1.2 symbols (4,065 named functions, 15,312 total).
- [x] Extracted and cached GameCube symbols (14,598 named functions — 99.9% coverage of engine symbols).
- [x] Extracted PS2 Alpha 10 and Demo vtables (`.gnu.linkonce.d._vt$*`) and internal Black Box source tree paths (`/indep/src/...`).
- [x] Built cross-platform symbol search tool (`scripts/query_symbols.py`).
- [x] Configured MinGW 32-bit build system (`Makefile`) in `src/`.
- [x] Successfully compiled and deployed `NFSU2CodeRestoration.asi` into `GAME/PC/scripts/`.

---

## 5. Next Steps
1. Commit current working foundation (Makefile, scripts, src, updated gitignore and HANDOFF.md).
2. Begin implementing initial feature restorations in `src/restorations/`:
   - Debug car customization menu (`UI_DebugCarCustomize.fng` / `CreateDebugCarCustomize` at `0x00554A00`).
   - Debug camera movers (bumper, hood, orbit, debug world camera).
   - Unused race coordinator track selectors (Free Run & Outrun track selection).
   - Cut vinyl category (`0x1C`) and parts restorations.
3. Test running `GAME/PC/SPEED2.EXE` under Wine with `WINEDLLOVERRIDES="dinput8=n,b"`.
