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
- **Host Platform**: NixOS (Linux). All build commands run inside `nix-shell` and only within the workspace directory.
- **Nix Environment (`shell.nix`)**:
  - 32-bit MinGW toolchain: `i686-w64-mingw32-gcc` / `g++` (GCC 15.3.0).
  - Wine: `pkgs.wineWow64Packages.stagingFull`.
  - Python: `python3` with `pyelftools`, `pefile`, `capstone`.
  - Local Python venv (`.venv`): `python-idb` and `capstone` for reading IDA Pro 9.0 `.i64` databases and disassembling.
- **Target Architecture**: 32-bit x86 Windows Dynamic Link Libraries (`.asi`).
- **Hooking Framework**: Header-only `injector` library under `src/includes/injector/`.
- **Configuration**: INI file parser under `src/includes/IniReader.h`.
- **Plugin Deployments**:
  - `GAME/PC/scripts/NFSU2CodeRestoration.asi` & `NFSU2CodeRestoration.ini`
  - `GAME/PC/scripts/NFSU2VehicleHealth.asi` & `NFSU2VehicleHealth.ini`
- **Wine Launch Overrides**: `WINEDLLOVERRIDES="dinput8=n,b"` (loading Ultimate ASI Loader via `dinput8.dll`).

---

## 3. Directory Layout & Git Policy
Only project source, scripts, environment, and documentation are tracked by Git:
- `src/` — C++ source code for the restoration plugin (`NFSU2CodeRestoration.asi`):
  - [`src/dllmain.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/dllmain.cpp): Compatibility check against v1.2 NTSC entry point (`0x75BCC7`), orchestrates restorations.
  - [`src/Config.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/Config.h) / [`src/Config.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/Config.cpp): Reads and validates `NFSU2CodeRestoration.ini` settings and hotkeys.
  - [`src/Logger.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/Logger.h) / [`src/Logger.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/Logger.cpp): Runtime logging to `NFSU2CodeRestoration.log` with crash handler.
  - [`src/restorations/DebugCarCustomize.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/DebugCarCustomize.cpp): Restores cut `UI_DebugCarCustomize.fng` into car customize menu.
  - [`src/restorations/CameraRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/CameraRestorations.cpp): Restores all 5 camera POV modes (Driver, Bumper, Hood, Drift, Chase).
  - [`src/restorations/RaceModeRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/RaceModeRestorations.cpp): Restores Outrun in Quick Race, Free Run/Outrun track select, AI opponents in Sprint Drift, URL lap count modifier, Pause Menu "Restart Race", Track 4000 barrier crash fix, Career locked area barrier removal, Any Track in Any Mode hook, and Lap Knockout / GT mode in Quick Race.
  - [`src/restorations/BurnoutRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/BurnoutRestorations.cpp) / [`src/restorations/BurnoutRestorations.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/BurnoutRestorations.h): Restores Burnout / Smokeshow mode, trick judging engine, combo multipliers, and on-screen HUD popups (`SplitTimeText` / `RaceOverMessage`).
  - [`src/restorations/FngData_HUD_CarShow.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/FngData_HUD_CarShow.cpp): Restored and fixed binary package data for `HUD_CarShow.fng` (backing plates, minimap tracking, tachometer cleanups).
  - [`src/restorations/VehicleDamageRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/VehicleDamageRestorations.cpp) / [`src/restorations/VehicleDamageRestorations.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/VehicleDamageRestorations.h): Restores cut vehicle part damage, 3-stage wobbling animations (`WINDOW_DAMAGE0..2`), dynamic flapping hinges (doors/trunk), collision impulse delivery, and severe crash detachment (`4.0f` threshold).
  - [`src/restorations/CustomizationRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/CustomizationRestorations.cpp): Restores hidden Special Vinyls category (`0x1C`).
  - [`src/restorations/EngineFixes.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/EngineFixes.cpp) / [`src/restorations/EngineFixes.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/EngineFixes.h): Disappearing wheels fix in `CarPartCuller`, internal `DebugWorldCameraMover`, and `PATH_status` audio bank null crash fix (`0x0073927D`).
  - [`src/restorations/GameplayRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src/restorations/GameplayRestorations.cpp): Main loop tick at `0x581470`, F6 Autopilot (`Player_AutoPilotOn/Off`), F5 Unlock All, F7 Cycle Damage Stages.
- `src-vehicle-health/` — C++ source code for the vehicle health and disqualification plugin (`NFSU2VehicleHealth.asi`):
  - [`src-vehicle-health/dllmain.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/dllmain.cpp): ASI entry point, game version verification, lifecycle hook installation.
  - [`src-vehicle-health/Config.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/Config.h) / [`src-vehicle-health/Config.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/Config.cpp): Reads and validates `NFSU2VehicleHealth.ini` settings (HP values, damage scaling, HUD/floating toggles, hotkeys).
  - [`src-vehicle-health/Logger.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/Logger.h) / [`src-vehicle-health/Logger.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/Logger.cpp): Thread-safe file logging to `scripts/NFSU2VehicleHealth.log`.
  - [`src-vehicle-health/VehicleHealthManager.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.h) / [`src-vehicle-health/VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp): Physics impulse collision damage hook (`0x005A0BC4`), active/spawned vehicle lifecycle tracking (`Car + 0x550`, `TrafficAI + 0x77D`), Free Roam invulnerability mode, race disqualification / DNF last-place ranking (`0x00602000`), and multi-layer physical death immobilization (`ImmobilizeCar`).
  - [`src-vehicle-health/HealthBarRenderer.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.h) / [`src-vehicle-health/HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp): Direct3D 9 `EndScene` hooking, screen-space 2D local player HUD health bar, 3D in-world floating health bars with perspective distance scaling and behind-camera near-plane/Clip-W culling.
- `scripts/` — Python reverse engineering utilities:
  - `scripts/dump_ida_symbols.py`: Exports functions and names from IDA `.i64` files.
  - `scripts/extract_ps2_symbols.py`: Extracts vtables, source paths, assertions, and strings from PS2 ELFs.
  - `scripts/query_symbols.py`: Instant CLI search across PC, GameCube, and PS2 symbol caches.
- `shell.nix` — Reproducible Nix environment specification.
- `Makefile` — Statically links runtime libraries (`-static`) and builds both `NFSU2CodeRestoration.asi` and `NFSU2VehicleHealth.asi`, deploying to `GAME/PC/scripts/`.
- [`HANDOFF.md`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/HANDOFF.md) — Project roadmap, architecture notes, and progress handoff.
- [Walkthrough](file:///home/nebulafdv2/.gemini/antigravity-ide/brain/a7050381-50e9-484a-9e54-26ba7f42b0e0/walkthrough.md) — Detailed step-by-step documentation of all implemented features, bugfixes, and reverse-engineering findings.
- `.gitignore` — Strictly ignores `GAME/`, `Bin-idb-i64/`, `Reference-Code/`, `build/`, `.venv/`, and `cache/`.

---

## 4. Current Status & Verified Restorations

### A. Core UI & Menu Restorations
- [x] **Debug Car Customization**: Re-enabled cut `UI_DebugCarCustomize.fng` screen (StateID 4) in Customize Car menu.
- [x] **Camera Modes**: Re-enabled all 5 camera POVs (Driver, Bumper, Hood, Drift, Chase) and scroll limit.
- [x] **Quick Race Modes**: Restored Outrun mode, Free Run track selection, and Lap Knockout / GT mode (`Mode 9`).
- [x] **Sprint Drift AI**: Restored AI opponents and leaderboard scoring in Sprint Drift races.
- [x] **URL Lap Controller**: Unlocked lap modifier for URL races.
- [x] **Pause Menu Restart**: Restored "Restart Race" button across URL tournaments, Outruns, and all modes.
- [x] **Special Vinyls**: Restored hidden vinyl category (`0x1C`).
- [x] **Main Loop & Hotkeys**: Per-frame tick hook at `0x581470` for F6 Autopilot (`Player_AutoPilotOn`), F5 Unlock All, and F7 Cycle Damage Stages.

### B. Stability & Engine Crash Fixes
- [x] **Audio `PATH_status` Fatal Crash Fix (`0x0073927D`)**:
  - Fixed random `0xC0000005` access violation caused by multi-threaded audio cleanup zeroing `ds:0x8B7BB0`.
  - Implemented recovery routine `GetOrRecoverCurrentPathBank()` and code cave at `0x00739274` that gracefully falls back if no audio bank is loaded.
- [x] **Missing Barrier Crash Fix (Track 4000 & Custom Tracks)**:
  - Naked hook at `0x578070` inside `sub_578060` redirecting `"BARRIERS_%d"` to `"BARRMERS_%d"`, bypassing `TrackStreamer` registration and eliminating race transition crashes.
- [x] **Career Locked Area Barriers**:
  - Patched `0x7A073C` (`"BARRIERS_CAREER%d"`) to `'M'`, eliminating neon barriers in Career mode.
- [x] **Any Track in Any Mode Hook**:
  - Hooked `UIQRTrackSelect::BuildPresetTrackList` at `0x4CDEF5` to allow all valid tracks while safely filtering unpopulated dummy stubs.
- [x] **Disappearing Wheels Fix**:
  - Patched `CarPartCuller::CullParts` at `0x60C5A9`.

### C. Burnout / Smokeshow Mode & Trick Engine
- [x] Restored `Mode 8` (`Smokeshow / Burnout`) in Quick Race menu and fixed Track 4097 override crash (`0x52481A`).
- [x] Fixed HUD visual artifacts in `HUD_CarShow.fng`:
  - Restored opaque dark backing boxes for lap counters, timer, and leaderboard rows (`0xFF000000` and `0xC0000000`).
  - Remapped minimap track map to resource 15 (`Track4000_map.tga`), fixed player car blip hash (`0xC1347E2F`), and restored opponent car indicators.
  - Relocated orphan PS2 Demo turbo text and gauge objects off-screen.
- [x] Reverse-engineered and ported the PS2 Demo Burnout trick judging & scoring engine:
  - Fixed player car pointer dereference bug (`0x008900AC` array indexing).
  - Decoupled trick evaluation from tire slip score, allowing high-speed tricks (S-Curves, S-Mirrors, 360 Spins) to trigger.
  - Implemented combo multipliers (`1.0x` up to `2.5x`) and chain window timeouts.
  - Center-screen HUD trick popup banners (`SplitTimeText` & `RaceOverMessage` with `ZoomInGreen` animation).
  - Restored tricks: `DONUT (CW/CCW)`, `FIGURE 8`, `S CURVE`, `S MIRROR`, `360 SPIN`, `J TURN`.

### D. Vehicle Part Damage, Wobbling & Detachment Restoration
- [x] **Global Engine Damage Activation**:
  - Patched `0x00540033` (`0F 95 C0` -> `B0 01 90`) so race initialization writes `1` to `0x0089E7D2`.
  - Set `g_CarDamageEnable` (`0x00870CCC`), `0x008764E4`, and NOPed graphics preset override at `0x005BE6F7`.
  - Forced `0x0089E7D2 = 1` in per-frame tick.
- [x] **Enabled All 6 Damage Parts**:
  - Enabled all 6 slots in `0x00803698` (Left Mirror, Right Mirror, Exhaust, Trunk, Left Door, Right Door).
- [x] **PS2 Demo Cross-Reference & Gutted PC Engine Discovery**:
  - PS2 Demo evaluated `CarPartDamage::UpdateDamage` (`0x2d4928`) to dispatch animation channels (`0x2d`..`0x31` / `0x4461a0`) with animation IDs 30..34 (`0x445320`).
  - Retail PC gutted this dispatch at `0x00610CD0` into an empty loop, leaving table `0x00803944` orphaned.
  - Retail PC replaced `GetDamagedPartRenderingMatrix` in `CarRenderInfo::Render` (`0x00624441`) with `jmp 0x0062449E`, causing any part with damage $\ge 0.25\text{f}$ to bypass `eMesh::Render` and leak 64 bytes (`malloc(0x40)`).
- [x] **Pivot-Invariant Physical Wobble Engine**:
  - Implemented `BuildPivotWobbleMatrix` using exact 3D invariant pivot mathematics:
    $$T_{offset} = P - P \cdot R$$
    ensuring that part hinges and mounts ($P$) remain 100% attached to the car frame with 0.0 error while the unlatched ends flutter and rattle:
    - **Left Door (Part 4)**: Hinge pinned at A-pillar $(0.68, 0.82, 0.22)$; rear latch rattles open outward on yaw axis ($Z$).
    - **Right Door (Part 5)**: Hinge pinned at passenger A-pillar $(0.68, -0.82, 0.22)$; rattles open outward.
    - **Trunk Lid (Part 3)**: Top hinge pinned near rear window $(-0.75, 0.00, 0.58)$; rear edge bounces upward on pitch ($Y$).
    - **Exhaust Pipe (Part 2)**: Pipe pinned at underbody hanger $(-1.65, -0.38, 0.08)$; tip rattles and vibrates.
    - **Left & Right Mirrors (Parts 0 & 1)**: Base pinned at window triangle $(0.50, \pm 0.88, 0.55)$; housing buzzes and jiggles.
- [x] **Safe Mesh Rendering Without Register Corruption**:
  - Intercepted `0x00624441` with `PartDamageWobbleCodeCave`:
    - Evaluates detachment: jumps to `0x0062449E` when $\text{dmg} \ge 4.0\text{f}$ (detaches cleanly without memory leaks).
    - Preserves register `ebx` completely, preventing any distortion or matrix corruption of the car body, wheels, or other parts.
  - Hooked part mesh rendering at `0x00624499` with `Hooked_BreakablePartMeshRender`:
    - Applies `s_ActivePartWobbleMatrix` only to the active breakable part model draw and resets it immediately.
  - Hooked Left Mirror (`0x006264F6` threshold, `0x006269F0` render call via `Hooked_RenderLeftMirrorMesh`).
  - Hooked Right Mirror (`0x00626A67` threshold, `0x00626FED` render call via `Hooked_RenderRightMirrorMesh`).
- [x] **Collision Force Delivery & Quad Mapping**:
  - Hooked `CarCollisionBody::AddDamageForce` (`0x00593C44`) with responsive force scaling and local impact quadrant mapping (Front, Rear, Left, Right).
  - Populated zeroed collision weights table `0x008A0388`, ensuring crashes anywhere on the car deliver damage.
- [x] **In-Game Testing Hotkey (F7)**:
  - Pressing **F7** cycles player damage stages directly: `0.0f` (Repaired) -> `0.6f` (Stage 1) -> `1.5f` (Stage 2) -> `2.8f` (Stage 3) -> `4.5f` (Detached).

### E. Vehicle Health & Damage System (`NFSU2VehicleHealth.asi`)
- [x] **Collision Physics Impulse Hooking (`0x005A0BC4`)**:
  - Hooked `CarCollisionBody::AddDamage` inside `ApplyImpulse` at `0x005A0BC4` with a naked assembly trampoline.
  - Intercepts real physical impact forces between all vehicles (Player, AI Opponents, Traffic) and world geometry.
  - Scales damage using configurable `DamageMultiplier` with `MinForceThreshold` filtering to ignore normal tire friction, bumps, and curb contact.
- [x] **Direct3D 9 `EndScene` Hooking & Wine Compatibility**:
  - Intercepts Direct3D 9 `EndScene` (`vtable[42]`) from the device pointer at `0x00870974`.
  - Relaxed device pointer verification to `pDevice >= 0x00010000`, resolving Wine/WineD3D compatibility where the COM device object resides on the low process heap (`0x00100000`–`0x003FFFFF`).
  - Thread-safe hook installation using Win32 `CRITICAL_SECTION`.
  - Comprehensive Direct3D state preservation and restoration: saves and restores `D3DRS_FOGENABLE`, `D3DRS_SCISSORTESTENABLE`, `D3DRS_ALPHABLENDENABLE`, `D3DRS_ZENABLE`, and texture stage states.
- [x] **In-World 3D Floating Health Bars**:
  - Projects 3D car coordinates to screen space using native `eView::WorldToScreen` (`s_GetView(0)` / `s_GetView(1)` with matrix fallback).
  - **Behind-Camera Projection Culling**: Dot product with camera forward vector and homogeneous Clip-W plane check ($W_{\text{clip}} \le 0.5\text{m}$) completely eliminates phantom inverted health bars for vehicles behind the player.
  - Perspective distance scaling: dynamically scales bar dimensions based on camera depth (`min(1.4, max(0.5, 30.0 / depth))`).
  - Smooth animated damage lag bar trailing behind current health.
  - HP color gradient: transitions smoothly from Green ($>50\%$) to Yellow ($20\%-50\%$) to Red ($<20\%$).
- [x] **2D Screen-Space Local Player HUD Health Bar**:
  - Top-center screen-space HUD bar decoupled from 3D world view matrices.
  - Smooth animated damage lag drop, HP percentage readout, and styling matching the game's aesthetic.
- [x] **Race Disqualification (DNF) & Last-Place Standings**:
  - When a car's health reaches 0 HP in a race, calculates `lastPlace = numRacers` (or next lowest available rank).
  - Overwrites race standings (`racingCar + 0x0C`, `0x0E`, `0x14`) and calls native `RaceStatus::FinishRacer` (`0x00602000`) with DNF reason.
  - Results screen accurately lists the wrecked car at the bottom in last place with `--:--.--` DNF status instead of awarding 1st place.
- [x] **Free Roam Player Invulnerability Mode**:
  - Hooked `StartCareerFreeRoam` (`0x005404A0`), `StartRace` (`0x0053FC20`), and checks Track `4000` (`0xFA0`) with `numRacers <= 1`.
  - In Quick Race Free Roam and Career Free Roam, the player car is completely immune to damage, health remains locked at 100%, and race disqualification is bypassed.
- [x] **Active / Despawned / Pooled Traffic Car Lifecycle Tracking**:
  - Validates `Car + 0x550` (`mIsActive == 1`), $Z \ne -123456.0\text{f}$, and `TrafficAI + 0x77D == 0`.
  - Instantly removes despawned or pooled traffic cars from `s_HealthMap` and omits them from rendering, eliminating frozen health bars over empty road locations.
  - Periodic cleanup timer purges stale vehicle references every 30 frames.
- [x] **Multi-Layer Complete Vehicle Death Immobilization (`ImmobilizeCar`)**:
  - **Full Brakes**: Applies 100% full foot brake (`driver + 0x210 = 1.0f`) and 100% locked handbrake (`driver + 0x214 = 1.0f`).
  - **Drivetrain Kill**: Sets throttle to 0 (`driver + 0x20C = 0.0f`), centers steering (`driver + 0x208 = 0.0f`), locks neutral gear on Car (`car + 0x4D0 = 0`), locks transmission neutral (`mover + 0x4C + 0x50/54 = 0`), zeroes engine throttle (`mover + 0x48 + 0x78 = 0.0f`), and stops wheel rotation (`wheel + 0x28 = 0.0f`).
  - **RigidBody Physical Velocity & Momentum Arrest**: In `SimVehicle + 0x2C` (`RigidBody`), hard-clamps all linear velocity ($V_x, V_y, V_z$), angular velocity ($\omega_x, \omega_y, \omega_z$), and momentum vectors to `0.0f` when speed drops below $3.0\text{ m/s}$ ($\sim 11\text{ km/h}$), and aggressively decelerates at higher speeds.
  - Enforced continuously in `OnCollisionForce`, `DamagePlayerCar`, `Update`, and `DelegateDriverInput` (`0x005ABBF0`), completely stopping dead AI racers from creeping forward or rolling down hills.
- [x] **Wine-Compatible Hotkeys**:
  - **F8**: Full repair on all active vehicles.
  - **F9**: Damages player car by 25% for testing.
  - Software debouncing using `GetAsyncKeyState(...) & 0x8000` for reliable single-trigger execution on Linux / Wine.

---

## 5. Next Steps & Research Vectors
1. **Regional Cars & Traffic Car Unlocking**:
   - Investigate unlocking Peugeot 106, Opel Corsa, and traffic vehicle meshes for Quick Race.
2. **Cut HUD / UI Screens**:
   - Restore `LS_LangSelect.fng` (language selection bootflow screen).
   - Investigate split-screen 2P HUD remnants (`2PHudBot.fng`, `2PHudTop.fng`) identified in PS2 Alpha 10.
3. **Cut Audio & Radio Chatter**:
   - Explore unused voice lines and SMS message triggers in GameCube/PS2 builds.


