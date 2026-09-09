# NFSUG2-ASI-STUFF

> **Custom ASI Plugins, Reverse-Engineered Restorations & Mechanics for Need for Speed: Underground 2 (PC v1.2 NTSC)**

---

> [!WARNING]
> ### ⚡ COMPLETELY VIBECODED DISCLAIMER
> **This entire repository is 100% vibecoded!**
> 
> Everything in here—from reverse-engineering memory structures to assembly caves, physics impulse hooking, Direct3D 9 matrix math, and trick judging routines—was developed, tested, and iterated through **AI pair programming, vibes, and raw intuition**. Expect bizarre hacks, deep engine hooks, unconventional solutions, and undocumented EA Black Box quirks. It runs, it does wild things, but it was forged entirely in the fires of vibe coding. **Use at your own risk!**

---

> [!CAUTION]
> ### 🧪 EXPERIMENTAL NOTICE: `CodeRestoreTest`
> The code inside [`src-CodeRestoreTest/`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest) (`NFSU2CodeRestoration.asi`) is an **experimental reverse-engineering testbed**.
> 
> **Most of the things in `CodeRestoreTest` will NOT work properly or remain incomplete.** It is an active playground for resurrecting gutted code, unused menus, and early mechanics from the PS2 Alpha 10, PS2 Demo, and GameCube builds back into the PC v1.2 executable. Some features may glitch, fail to trigger, or crash under specific circumstances. Do not expect a polished or bug-free experience from this module!

---

## 📑 Table of Contents
- [Overview](#-overview)
- [Plugins Overview](#-plugins-overview)
  - [1. NFSU2VehicleHealth (`src-vehicle-health/`)](#1-nfsu2vehiclehealth-src-vehicle-health)
  - [2. NFSU2CodeRestoration (`src-CodeRestoreTest/`)](#2-nfsu2coderestoration-src-coderestoretest)
- [Build Instructions](#-build-instructions)
  - [Prerequisites](#prerequisites)
  - [Compiling Everything](#compiling-everything)
  - [Compiling Separately](#compiling-separately)
  - [Standalone Directory Builds](#standalone-directory-builds)
- [Installation & Setup](#-installation--setup)
- [Configuration](#-configuration)
- [Thanks and Credits](#-thanks-and-credits)

---

## 🔭 Overview

**NFSUG2-ASI-STUFF** is a collection of 32-bit x86 C++ ASI plugins for **Need for Speed: Underground 2 (PC v1.2 US / NTSC)**. 

The project contains two distinct plugins:
1. **`NFSU2VehicleHealth`**: A complete, standalone vehicle destruction and health management system featuring physical collision damage, 2D HUD health bar, 3D floating world health bars with behind-camera frustum culling, race DNF disqualification with proper last-place standings, and multi-layer death immobilization.
2. **`NFSU2CodeRestoration`** (`CodeRestoreTest`): An experimental research module dedicated to reviving cut, unused, and prototype features discovered across the PS2 Alpha 10 (`SLUS_210.65`), PS2 Demo (`SLUS_291.18`), and GameCube NTSC builds.

---

## 🎮 Plugins Overview

### 1. NFSU2VehicleHealth (`src-vehicle-health/`)
*Standalone Vehicle Destruction & Health System (`NFSU2VehicleHealth.asi`)*

- **Physical Impact Collision Interception**: Hooks into `CarCollisionBody::AddDamage` inside `ApplyImpulse` (`0x005A0BC4`) to calculate damage dynamically from actual vehicle-to-vehicle and vehicle-to-world physics impulses, filtering out minor curbs and normal tire friction.
- **Direct3D 9 `EndScene` Overlay**: Hooks Direct3D 9 `EndScene` (`0x00870974`) directly with full state preservation and relaxed pointer checks for 100% compatibility with native Windows and Linux (Wine / Proton).
- **2D Screen-Space HUD Bar**: Top-center docked HUD bar for the local player car featuring smooth animated damage lag and dynamic health color gradients (Green $\to$ Yellow $\to$ Red).
- **3D Floating World Health Bars**: In-world health bars above AI opponents and ambient traffic using native `eView::WorldToScreen` projection with perspective distance scaling.
- **Behind-Camera Projection Culling**: Dot product with camera forward vector and homogeneous Clip-W plane check ($W_{\text{clip}} \le 0.5\text{m}$) completely eliminates inverted "phantom" health bars for vehicles behind the player's view.
- **Authentic Race Disqualification (DNF)**: Vehicles reduced to 0 HP are disqualified with legitimate last-place standings (`lastPlace = numRacers`) via native `RaceStatus::FinishRacer` (`0x00602000`) instead of incorrectly claiming 1st place.
- **Complete Multi-Layer Death Immobilization (`ImmobilizeCar`)**:
  - Full foot brake (`driver + 0x210 = 1.0f`) and handbrake (`driver + 0x214 = 1.0f`).
  - Drivetrain arrest: zero throttle, centered steering, locked neutral gear, and transmission neutral lock.
  - RigidBody linear and angular velocity zeroing below $3.0\text{ m/s}$ to stop dead cars from rolling or creeping forward.
- **Free Roam Player Invulnerability**: Disables player damage and disqualification during Career Free Roam and Quick Race Explore mode.
- **Active Vehicle Lifecycle Tracking**: Prevents floating ghost bars by immediately purging despawned or pooled traffic cars.
- **Hotkeys (Debounced for Wine/Linux)**:
  - **F8**: Fully repair all vehicles back to 100% HP.
  - **F9**: Inflict 25% damage on the player car for quick testing.

---

### 2. NFSU2CodeRestoration (`src-CodeRestoreTest/`)
*Experimental Cut Feature Restorations & Engine Fixes (`NFSU2CodeRestoration.asi`)*

> [!NOTE]
> As noted in the disclaimer, features in this module are **experimental testbeds** and may not work consistently.

- **PS2 Demo Burnout / Smokeshow Mode & Trick Engine**:
  - Restores `Mode 8` in the Quick Race menu and patches the Track 4097 override crash (`0x52481A`).
  - Restores the PS2 Demo trick judging engine: Donuts (CW/CCW), 360 Spins, J-Turns, S-Curves, and S-Mirrors with combo multipliers ($1.0\times$ up to $2.5\times$).
  - Restores center-screen animated HUD popups (`SplitTimeText` and `RaceOverMessage`).
  - Restores backing plates, minimap tracking, and tachometer cleanups in `HUD_CarShow.fng`.
- **Procedural 3D Vehicle Part Wobbling & Detachment**:
  - Restores cut vehicle damage tables and animations gutted from retail PC builds.
  - Invariant pivot wobble engine ($T_{\text{offset}} = P - P \cdot R$) keeping hinges anchored while doors, trunk, exhaust, and side mirrors rattle and vibrate based on collision damage.
  - Clean collision detachment threshold ($\ge 4.0\text{f}$) without register corruption or memory leaks.
  - Direct damage delivery hook into `CarCollisionBody::AddDamageForce` (`0x00593C44`) with impact quadrant mapping.
- **Menu & UI Restorations**:
  - Cut `UI_DebugCarCustomize.fng` screen (StateID 4) restored into the Customize Car menu.
  - All 5 camera POV modes (Driver, Bumper, Hood, Drift, Chase) with unlocked scroll limits.
  - Outrun mode and Free Run track selection restored in Quick Race.
  - Lap Knockout / GT mode (`Mode 9`) restored in Quick Race.
  - AI opponents and leaderboard scoring restored in Sprint Drift races.
  - Lap count modifier restored for URL races.
  - Pause Menu "Restart Race" enabled across all race modes.
  - Hidden Special Vinyls category (`0x1C`) unlocked.
- **Engine Stability & Bug Fixes**:
  - **`PATH_status` Audio Crash Fix (`0x0073927D`)**: Prevents fatal `0xC0000005` access violations caused by asynchronous sound engine cleanup zeroing bank pointers.
  - **Track 4000 Missing Barrier Crash Fix**: Bypasses missing barrier package stream crashes when starting Track 4000 in Circuit mode.
  - **Career Locked Area Barriers**: Removes neon barricades in Career mode.
  - **Disappearing Wheels Fix**: Corrects `CarPartCuller` bounding culling in custom camera views.
- **Testing Hotkeys**:
  - **F5**: Unlock all career and car parts.
  - **F6**: Toggle AI Autopilot (`Player_AutoPilotOn/Off`).
  - **F7**: Cycle vehicle damage stages (Repaired $\to$ Stage 1 $\to$ Stage 2 $\to$ Stage 3 $\to$ Detached).

---

## 🛠️ Build Instructions

### Prerequisites
- **Target**: 32-bit x86 Windows DLL (`.asi`).
- **Toolchain**: `i686-w64-mingw32-gcc` / `i686-w64-mingw32-g++` (GCC 10+).
- **Build System**: GNU Make.
- **Optional**: Nix / NixOS (a complete reproducible environment is provided via [`shell.nix`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/shell.nix)).

---

### Compiling Everything
To build both `NFSU2CodeRestoration.asi` and `NFSU2VehicleHealth.asi` at once:

```bash
# Using standard MinGW toolchain:
make all

# Or using Nix:
nix-shell --run "make all"
```

The compiled binaries will be output to:
- `build/NFSU2CodeRestoration.asi`
- `build/NFSU2VehicleHealth.asi`

---

### Compiling Separately
Both plugins are completely decoupled and can be compiled individually from the root directory:

```bash
# Build ONLY the Vehicle Health plugin:
make vehicle-health
# (alias: make health)

# Build ONLY the Code Restoration plugin:
make code-restore
# (alias: make restore)

# Clean all build outputs:
make clean
```

If you are using Nix, simply prepend `nix-shell --run`:
```bash
nix-shell --run "make vehicle-health"
nix-shell --run "make code-restore"
```

---

### Standalone Directory Builds
You can also compile each plugin directly from within its own subdirectory:

```bash
# Build Vehicle Health from its own folder:
cd src-vehicle-health
make

# Build Code Restoration from its own folder:
cd src-CodeRestoreTest
make
```

---

## 💾 Installation & Setup

1. Ensure your game is patched to **Need for Speed: Underground 2 v1.2 NTSC (US)**.
2. Install **Ultimate ASI Loader** (e.g., place `dinput8.dll` in your game root directory).
3. Copy the compiled `.asi` files from the `build/` directory into your game's `scripts/` folder:
   - `scripts/NFSU2VehicleHealth.asi`
   - `scripts/NFSU2CodeRestoration.asi`
4. Copy the default configuration `.ini` files into the `scripts/` folder:
   - [`src-vehicle-health/NFSU2VehicleHealth.ini`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/NFSU2VehicleHealth.ini) $\to$ `scripts/NFSU2VehicleHealth.ini`
   - [`src-CodeRestoreTest/NFSU2CodeRestoration.ini`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/NFSU2CodeRestoration.ini) $\to$ `scripts/NFSU2CodeRestoration.ini`
5. **Wine / Proton (Linux)**:
   Ensure your Wine configuration has the DLL override set:
   ```bash
   WINEDLLOVERRIDES="dinput8=n,b"
   ```

---

## ⚙️ Configuration

Both plugins include extensive configuration via INI files:

- **[`NFSU2VehicleHealth.ini`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/NFSU2VehicleHealth.ini)**:
  - Toggle health system globally (`EnableHealthSystem`).
  - Max HP (`MaxHealth = 100.0`).
  - Collision force scaling (`DamageMultiplier`, `MinForceThreshold`).
  - Free Roam player invulnerability (`DisablePlayerHealthInFreeRoam = 1`).
  - HUD and world bar toggles (`ShowPlayerBar`, `ShowOpponentBars`, `ShowTrafficBars`).
  - In-world bar float offset (`HealthBarHeightOffset = 1.85`).
  - Custom hotkeys (`HotkeyRepairAll`, `HotkeyDamagePlayer`).

- **[`NFSU2CodeRestoration.ini`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/NFSU2CodeRestoration.ini)**:
  - Individual toggles for every single restored feature (Debug car customize, camera POVs, race modes, burnout mode, procedural vehicle damage).
  - Engine stability toggles (audio crash fix, barrier crash fix, wheel culler fix).
  - Debugging hotkeys (`AutoDrive`, `UnlockAll`, `DamageCycle`).

---

## 💖 Thanks and Credits

### Project Lead & Development
- **[Furkan Demirhan](https://github.com/FurkanDemirhan)** — Author, project direction, reverse-engineering, and vibe coding.

### Reference Material & Special Thanks
- **[NFSU2 Extra Options](https://github.com/ExOptsTeam/NFSU2ExOpts)** by **[nlgxzef](https://github.com/nlgxzef)** — The gold standard for Need for Speed: Underground 2 modding and engine extension.
- **[nlgxzef](https://github.com/nlgxzef)** — For invaluable IDA Pro `.i64` database files, symbol naming, and monumental foundational reverse-engineering of the NFSU2 executable and GameCube debug symbols.
- **[MWHealthbars](https://github.com/trelbutate/MWHealthbars/)** by **[trelbutate](https://github.com/trelbutate)** — Foundational reference, inspiration, and math for vehicle health tracking and HUD/floating health bar rendering.
- **[GlobalLib](https://github.com/NFSTools/GlobalLib)** by **[NFSTools](https://github.com/NFSTools)** — Invaluable reference material for NFS game structures, formats, and hashing utilities.
