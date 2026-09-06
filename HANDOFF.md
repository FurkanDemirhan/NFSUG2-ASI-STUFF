# Need For Speed Underground 2 - Code Restoration Project

## 1. Overview & Objective
This project focuses on identifying, reversing, and restoring cut and unused code, gameplay mechanics, debugging features, and assets in the PC release of **Need For Speed: Underground 2 (v1.2 US / NTSC)**. 

To achieve high-accuracy restorations, we cross-reference the PC executable with development and early builds:
- **PS2 Alpha 10 Build** (`SLUS_210.65__(Alpha10R_Bin).ELF` / `./GAME/PS2/Alpha10/`)
- **PS2 Demo Build** (`SLUS_291.18_(PS2DEMO).ELF` / `./GAME/PS2/Demo/`)
- **GameCube NTSC Build** (IDA 9.0 DB: `NFSUnderground2-Gamecube-NTSC.i64`)
- **PC v1.2 NTSC Target** (`SPEED2.EXE`, 4,800,512 bytes / IDA 9.0 DB: `NFSUnderground2-v1.2-US.i64`)
- **Reference Material**: NFSU2 Extra Options (`./Reference-Code/NFSU2ExOpts/`)

---

## 2. Technical Stack & Execution Model
- **Platform / Host OS**: NixOS (Linux).
- **Environment**: Managed via `shell.nix` in the project root. All build, analysis, and execution tasks are performed inside `nix-shell`.
- **Target Architecture**: 32-bit x86 Windows Dynamic Link Library (`.asi`).
- **Toolchain**: `i686-w64-mingw32-gcc` / `g++`, CMake, Make.
- **Hooking Strategy**: Memory injection, function detouring, vtable patching, and code caves via modern C++ and header-only hooking utilities (e.g. `injector`).
- **Plugin Delivery**: Compiled `.asi` placed in `./GAME/PC/scripts/`.
- **Wine Configuration**: `WINEDLLOVERRIDES="dinput8=n,b"` (loading Ultimate ASI Loader via `dinput8.dll`).

---

## 3. Directory Layout & Git Policy
Only project code, scripts, configuration, and documentation are tracked by Git:
- `src/` — C++ source code for the restoration `.asi` plugin.
- `scripts/` — Python and shell analysis/reverse engineering scripts.
- `shell.nix` — Reproducible Nix environment specification.
- `HANDOFF.md` — Project roadmap, architecture notes, and progress handoff.
- `.gitignore` — Ignores game installations (`GAME/`), binaries (`Bin-idb-i64/`), and reference code (`Reference-Code/`).

---

## 4. Current Status
- [x] Initialized `shell.nix` with MinGW 32-bit cross-toolchain, Wine staging, and Python analysis tools.
- [x] Configured `.gitignore` to prevent tracking huge binaries and game directories.
- [x] Initialized `HANDOFF.md`.
- [ ] Verify `nix-shell` toolchain execution (MinGW, Wine, Python).
- [ ] Inspect IDA `.i64` databases and PS2 ELFs to build extraction and comparison tooling.
- [ ] Set up build system (CMake/Makefile) in `src/` producing a test `.asi`.
- [ ] Catalog unused functions, debug symbols, and strings between PS2 Alpha 10 / Demo and PC v1.2.

---

## 5. Next Steps
1. Complete Nix environment verification.
2. Develop Python scripts under `scripts/` to inspect IDA 9.0 `.i64` files and extract symbols from PS2 ELFs (`readelf`/`pyelftools`).
3. Set up CMake / Makefile for building the ASI plugin into `GAME/PC/scripts/NFSU2CodeRestoration.asi`.
4. Validate loading the ASI plugin with Wine in `GAME/PC/`.
