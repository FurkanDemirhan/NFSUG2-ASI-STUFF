# NFSUG2-ASI-STUFF - Need For Speed Underground 2 ASI Plugins & Restorations

## 1. Overview & Objective
This repository (**NFSUG2-ASI-STUFF**) contains custom ASI plugins, reverse-engineered restorations, cut gameplay mechanics, and new features for the PC release of **Need For Speed: Underground 2 (v1.2 US / NTSC)**. 

To achieve high-accuracy restorations and feature ports, we cross-reference the PC executable with development and early builds:
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
- **Hooking Framework**: Header-only `injector` library under `includes/injector/`.
- **Configuration**: INI file parser under `includes/IniReader.h`.
- **Plugin Deployments**:
  - `GAME/PC/scripts/NFSU2CodeRestoration.asi` & `NFSU2CodeRestoration.ini`
  - `GAME/PC/scripts/NFSU2VehicleHealth.asi` & `NFSU2VehicleHealth.ini`
- **Wine Launch Overrides**: `WINEDLLOVERRIDES="dinput8=n,b"` (loading Ultimate ASI Loader via `dinput8.dll`).

---

## 3. Directory Layout & Git Policy
Only project source, scripts, environment, and documentation are tracked by Git:
- `includes/` — Shared hooking (`injector`) and configuration (`IniReader.h`) headers.
- `src-CodeRestoreTest/` — C++ source code for the restoration plugin (`NFSU2CodeRestoration.asi`):
  - [`src-CodeRestoreTest/dllmain.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/dllmain.cpp): Compatibility check against v1.2 NTSC entry point (`0x75BCC7`), orchestrates restorations.
  - [`src-CodeRestoreTest/Config.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/Config.h) / [`src-CodeRestoreTest/Config.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/Config.cpp): Reads and validates `NFSU2CodeRestoration.ini` settings and hotkeys.
  - [`src-CodeRestoreTest/Logger.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/Logger.h) / [`src-CodeRestoreTest/Logger.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/Logger.cpp): Runtime logging to `NFSU2CodeRestoration.log` with crash handler.
  - [`src-CodeRestoreTest/restorations/DebugCarCustomize.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/DebugCarCustomize.cpp): Restores cut `UI_DebugCarCustomize.fng` into car customize menu.
  - [`src-CodeRestoreTest/restorations/CameraRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/CameraRestorations.cpp): Restores all 5 camera POV modes (Driver, Bumper, Hood, Drift, Chase).
  - [`src-CodeRestoreTest/restorations/RaceModeRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/RaceModeRestorations.cpp): Restores Outrun in Quick Race, Free Run/Outrun track select, AI opponents in Sprint Drift, URL lap count modifier, Pause Menu "Restart Race", Track 4000 barrier crash fix, Career locked area barrier removal, Any Track in Any Mode hook, and Lap Knockout / GT mode in Quick Race.
  - [`src-CodeRestoreTest/restorations/BurnoutRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/BurnoutRestorations.cpp) / [`src-CodeRestoreTest/restorations/BurnoutRestorations.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/BurnoutRestorations.h): Restores Burnout / Smokeshow mode, trick judging engine, combo multipliers, and on-screen HUD popups (`SplitTimeText` / `RaceOverMessage`).
  - [`src-CodeRestoreTest/restorations/FngData_HUD_CarShow.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/FngData_HUD_CarShow.cpp): Restored and fixed binary package data for `HUD_CarShow.fng` (backing plates, minimap tracking, tachometer cleanups).
  - [`src-CodeRestoreTest/restorations/VehicleDamageRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/VehicleDamageRestorations.cpp) / [`src-CodeRestoreTest/restorations/VehicleDamageRestorations.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/VehicleDamageRestorations.h): Restores cut vehicle part damage, 3-stage wobbling animations (`WINDOW_DAMAGE0..2`), dynamic flapping hinges (doors/trunk), collision impulse delivery, and severe crash detachment (`4.0f` threshold).
  - [`src-CodeRestoreTest/restorations/CustomizationRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/CustomizationRestorations.cpp): Restores hidden Special Vinyls category (`0x1C`).
  - [`src-CodeRestoreTest/restorations/RegionalCarsRestoration.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/RegionalCarsRestoration.cpp): Restores cut regional cars (Peugeot 106 / Opel Corsa) into the Car Lot, Quick Race car select and AI opponent pool (adapted from NFSU2 Extra Options "Remove Region Car Lock", verified against the v1.2 US IDA database).
  - [`src-CodeRestoreTest/restorations/DemoCareerRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/DemoCareerRestorations.cpp): Restores the PS2 Demo's cut `RandomEncounterManager` `DEACTIVATE_OPPONENT` state (encounter opponent cleanup after outrun engagements), driven by `TheRace` transitions and the `Player + 0x250` encounter-car field.
  - [`src-CodeRestoreTest/restorations/EngineFixes.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/EngineFixes.cpp) / [`src-CodeRestoreTest/restorations/EngineFixes.h`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/EngineFixes.h): Disappearing wheels fix in `CarPartCuller`, internal `DebugWorldCameraMover`, and `PATH_status` audio bank null crash fix (`0x0073927D`).
  - [`src-CodeRestoreTest/restorations/GameplayRestorations.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-CodeRestoreTest/restorations/GameplayRestorations.cpp): Main loop tick via dedicated unshared hook at `0x005814DB` (`MainLoopTickHook`, executing every frame in every game state/flow), F6 Autopilot (`Player_AutoPilotOn/Off`), F5 Unlock All, F7 Cycle Damage Stages — Wine-compatible `GetAsyncKeyState & 0x8000` with software edge detection.
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
- `Makefile` — Statically links runtime libraries (`-static`), builds both `NFSU2CodeRestoration.asi` and `NFSU2VehicleHealth.asi` together or separately, and deploys to `GAME/PC/scripts/`.
- [`README.md`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/README.md) — Public repository documentation, build instructions, and credits.
- [`HANDOFF.md`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/HANDOFF.md) — Project roadmap, architecture notes, and progress handoff.
- [Walkthrough](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/WALKTHROUGH.md) — Detailed step-by-step documentation of all implemented features, bugfixes, and reverse-engineering findings.
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
- [x] **Main Loop & Hotkeys**: Per-frame tick hook installed at `0x005814DB` (`MainLoopTickHook` — dedicated unconditional `call __return` slot inside `MainLoop`'s core while-loop). Unlike `0x5811C0` (which is only the function prologue executed once at boot) or `0x581470` (trapped behind state branches), `0x005814DB` executes every frame across all game states and flows. F6 Autopilot (`Player_AutoPilotOn`), F5 Unlock All, and F7 Cycle Damage Stages, using Wine-compatible `GetAsyncKeyState & 0x8000` with software edge detection. Periodic logs `[+] Main loop tick #%d alive (Flow=%d)` confirm continuous execution.

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
- [x] **Re-Reverse & Fix Pass (WALKTHROUGH section 26)**:
  - Full system map with all symbols resolved: `CarDamage::AddDamage` (`0x637C70`), `CarPartDamage::AddDamage` (`0x61CDD0`), `CarPartDamage::FindDamageSector` (`0x610D30` — 4×3×2 grid at `0x8A0388`, 6 weights per cell), `CarDamage::InitSectorPositions` (`0x610E00`), `CarPartDamage::IsPartDamaged` (`0x615240`), `CarPartDamage::SetDamageAmount` (`0x61CFE0`), ctors `0x633E70`/`0x63A360`.
  - Root cause of "parts never damaged": `InitSectorPositions` fills the damage grid bounds only for **15 hashed car models** (`0x8036FC`); all other cars degenerate to grid cell 0. Fixed with a generic full-car grid fallback hooked at the call site (`0x63892A`), plus zero-weight cell fill.
  - Root cause of "wobbling doesn't work": previous wobble angles were 0.5–0.8° symmetric jitter (visually imperceptible); the render-side mechanism itself was verified sound end-to-end. Angles rewritten to demo-scale progressive droop + rattle (doors ~28° ajar at stage 3).
  - Damage delivery now uses the authentic engine chain exclusively (force amplification ×2.5 at the `0x593C44` call site, engine grid distribution) instead of a parallel quadrant heuristic.
  - PS2 Demo comparison: demo wobble = per-part animation channels (table `0x4461A0 = {45..49}`); retail's orphaned copy `0x803944 = {47..51}` and threshold tables `0x803934 = {0.25, 1.0, 2.0}` are identical in role — the animation dispatch was cut with no remaining retail references.
  - **Fix pass 2 — per-part render blocks**: `CarRenderInfo::Render` (`0x620AE0`, unrolled per part) has two generic part blocks feeding the shared `eMesh::Render` call at `0x624499` with matrices in *different spaces*: `0x624441` passes the car-world matrix (`ebx`), `0x624404` passes a per-part matrix from an array (index `[esp+0x6C]`). Wobble is now evaluated at both block entries (register-preserving `pushad` caves, result smuggled through the saved-EAX slot) and composed at the shared call against whichever matrix the block passes. Detached parts are skipped inside the hook (vanilla's damaged-part flow mallocs `0x40` and skips the render — parts vanish + leak). `[VehicleDamage]` INI section: `WobbleScale` + per-part `PivotMirrorL/R`, `PivotExhaust`, `PivotTrunk`, `PivotDoorL/R` hinges.
  - **Fix pass 3 — centred oscillation**: wobble angles rewritten from static-droop+ripple to continuous flapping (doors ~1.4 Hz open/close swing, trunk ~1.75 Hz pop/slam, mirrors/exhaust vibrate in place) matching the PS2 Demo gameplay video.
  - **Fix pass 4 — dual-space pivots**: a zero pivot is only safe on the per-part-matrix path (part-local vertices); on the car-matrix path it orbits the part around the car origin (~2 m × sin θ — reads as "flying off"). Block caves declare their matrix space (`s_WobbleUseCarSpace`); the car-matrix path uses the configured car-space hinge (doors `0.68, ±0.82, 0.22` verified in-game, trunk `-1.05, 0, 0.85`), the per-part path forces zero.
  - **Fix pass 5 — dead main-loop tick (root cause of dead F7 + static wobble)**: `0x581470` sits behind eleven state-dispatch early returns in `MainLoop` and was never reached in gameplay. Tick moved to a `MainLoop` entry hook (`0x5811C0`, naked cave + 5-byte prologue re-execution), and hotkeys moved to Wine-compatible `& 0x8000` + debounce with the `IsLostFocus` gate removed. Verified live: tick reached 12,000+ frames, hotkeys active.
  - **Fix pass 7 — Manual Driving Hook (`0x00624404`), Beta Kinematics (`PARTS_ANIMATIONS.bin`), & Door-Fixed Mirrors**:
    - **Manual Driving Hook (`0x00624404`)**: Fixed critical discrepancy where parts wobbled on auto-drive (F6 autopilot) but remained static during manual player driving. Disassembly revealed manual gameplay branches at `0x6234ba..0x62351b` into `0x624404` (`PartBlock_PerPartMatrix_Cave`), bypassing the previously hooked `0x624441` car-matrix block. Hooked `0x624404` with a register-preserving naked cave that reads `partIndex` from `[esp + 0x64]` and `CarDamage*` from `[esp + 0x180]`.
    - **Beta Kinematics & Flap Gap Scaling (`PARTS_ANIMATIONS.bin`)**: Reverse-engineered `CARS/RX8/PARTS_ANIMATIONS.bin` extracted animations for trunk bounce and door swing.
    - **Door-Fixed Mirrors with Pure Rotation**: Mirrors mounted to parent doors via matrix composition; mirror damage vibration strictly uses `{0, 0, 0}` origin.
  - **Fix pass 8 — Manual Driving Speed (`Car + 0x430`)**:
    - Disassembled `Tachometer::Update` at `0x004C68D0` (`0x004C6940: flds 0x430(%esi); fabs`). Confirmed `Car + 0x430` is the engine's canonical forward speed float across all driving modes. Previous `GetPlayerVehicleSpeed()` navigated `Car + 0x2C` (`RigidBody`), which was unpopulated in manual driving and returned `0.0f` (freezing wobble). Updated `GetPlayerVehicleSpeed()` to read `std::abs(*(float*)((uintptr_t)car + 0x430))` with fallbacks. Wobble now functions reliably in manual driving.
  - **Fix pass 9 — Door Symmetry & Trunk Vertical Pitch ($Y$-Axis)**:
    - **Door Swing Symmetry Fix**: In vehicle coordinates ($X$: front, $Y$: left, $Z$: up), the latch lies behind the front A-pillar hinge ($d_x < 0$). Under yaw ($Z$), lateral displacement is $\Delta Y = d_x \sin\theta_z$. For Left Door ($P_y = +0.82\,\text{m}$), positive $\theta_z$ caused negative $\Delta Y$, swinging inward into the cabin. Setting `outAngles->z = -doorAngle` for Left Door yields $\Delta Y = +0.233\,\text{m}$ (outward left into the street), while Right Door with `outAngles->z = doorAngle` yields $\Delta Y = -0.233\,\text{m}$ (outward right into the street), ensuring complete physical symmetry.
    - **Trunk Vertical Pitch Axis ($Y$-Axis)**: Longitudinal $X$-axis rotation caused transverse roll (tilting the trunk lid sideways like a seesaw over the rear quarter panel). Transverse $Y$-axis rotation produces pure vertical pitch in the $X-Z$ plane with **identically zero lateral shift** ($Y' \equiv Y$, $\Delta Y = 0.00\,\text{m}$). Setting `outAngles->y` lifts both rear corners vertically by 26 cm while keeping the lid aligned.
    - **Hinge Pivot Update**: Updated `PivotTrunk` default from `-1.51, 0.0, 0.89` to `[-1.05, 0.0, 0.85]` across `Config.cpp` and all `.ini` files, anchoring the rotation to the rear window glass seam to prevent the front edge from dipping into the cabin.
  - **Current status: VERIFIED & COMPLETED** — Parts wobble identically during manual and auto driving, speed query reads `Car + 0x430` directly, trunk opens vertically and slams shut along the rear window hinge line without horizontal yaw or lateral dislocation, left and right doors flap outward away from the car symmetrically, mirrors remain fixed to doors, and hotkeys operate smoothly.

### F. Regional Cars Restoration
- [x] **Cut Regional Cars Unlock (Peugeot 106 / Opel Corsa)**:
  - Reverse-engineered `IsCarAvailableInCurrentBuildRegion` (`0x5022C0`): excludes car types by scanning a per-region table of 32-bit car-type IDs and returning false on any match.
    - US build (`GetBuildRegion() == 0`): scans `EUExclusiveCars` (`0x7F7C48` = `{ 7 CORSA, 6 PEUGOT106 }`).
    - EU/ROW build: scans `USExclusiveCars` (`0x7F7C50` = `{ 5 CIVIC, 18 RSX }`).
    - Its only callers are `UICareerCarLot::BuildCarList` (call site `0x4DFA83`) and `UIQRCarSelect::BuildCarList` (call site `0x4E2374`).
  - Neuters the exclusion entries (low byte $\to$ `0xFF`, making the entry `0x0000FFxx` and unmatchable) for cars whose model data exists on disc (`CARS\PEUGOT` still ships on US retail discs; `CARS\CORSA` was cut entirely).
  - `CIVIC`/`RSX` entries (`0x7F7C50`/`0x7F7C54`) written as in the original (no-op on US builds, fixes EU/ROW copies).
  - Region-check bypass hooks (`bool __cdecl RegionLockHook(int) { return 1; }`) installed only when every car excluded by the current region's table has model data, matching Extra Options' file gate.
  - Unlocked car types appended to `RandomlyChooseableCarConfigsNorthAmerica` (table `0x7F6DA4`, free slots `0x7F6DBE`/`0x7F6DBF`) with the element count extended (`mov ebp, 0x1A` immediate at `0x4FEB9D` in `PickRandomCarsOfSomeSort`), letting AI opponents drive them.
  - Adapted from NFSU2 Extra Options `UnlockRegionals` (`Reference-Code/NFSU2ExOpts`); configurable via `[Cars] UnlockRegionalCars` in `NFSU2CodeRestoration.ini`. Idempotent if Extra Options with `UnlockRegionals` is also installed.

### G. PS2 Demo Career Behavior Restoration
- [x] **`DEACTIVATE_OPPONENT` Encounter Cleanup (PS2 Demo `RandomEncounterManager` state cut from retail)**:
  - Research (see WALKTHROUGH section 25): the demo's encounter-manager enum table (demo `.data 0x3B8DB0`) has six states ending in `DEACTIVATE_OPPONENT`; retail kept five. The demo used the sixth state to return the engaged outrun opponent to the world pool after the encounter; retail tears the race down without it.
  - Retail-side anchors verified via IDA: `RandomEncounterManager::TryToEngagePlayer` (`0x413030`) stores the polled encounter opponent in `Player + 0x250` (player array at `0x8900B4`, count `0x8900BC`); a promoted opponent has a live `RacingCar*` at `Car + 0x1C`; deactivation = clearing `Car + 0x550` (`mIsActive`), matching the engine's own traffic despawn store (`Car::SetRenderingEnabled` `0x5ED5C0`, called from `TrafficController` `0x42C92A`/`0x4099FD`).
  - Implementation: main-loop tick watches `TheRace` (`0x890118`) transitions, captures promoted encounter opponents at race start, deactivates them (pool reclaim) at race end, with pointer plausibility and world-membership checks.
  - Configurable via `[DemoCareer] RestoreEncounterOpponentCleanup` (default `1`). **In-game testing pending.**
- [ ] **Further demo career findings (documented, not yet portable — asset-dependent)**:
  - Cut career screens `MU_UG_ChooseCareerCar.fng` (two preset 350Z builds), `MU_UG_TradeCareerCar.fng` (car trade-in), `MU_UG_SamanthaGiveth.fng` — absent from retail code and assets; require demo disc FNG extraction.
  - `EVENT_REWARD_EVENT_%d_UNIQUE` per-event unique reward scheme (demo `.rodata 0x388080`).
  - Split-screen career remnants: `GarageMain_2P.fng`, `2PHudBot/Top.fng`, `GarageMainScreen2P`.

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
  - Validates `Car + 0x550` (`mIsActive == 1`), $Z \ne -123456.0\text{f}$, and `RigidBody + 0x77D == 0` (`Car + 0x2C`).
  - Instantly removes despawned or pooled traffic cars from `s_HealthMap` and omits them from rendering, eliminating frozen health bars over empty road locations.
  - Periodic cleanup timer purges stale vehicle references every 30 frames.
- [x] **Multi-Layer Complete Vehicle Death Immobilization (`ImmobilizeCar`)**:
  - **Full Brakes**: Applies 100% full foot brake (`driver + 0x210 = 1.0f`) and 100% locked handbrake (`driver + 0x214 = 1.0f`).
  - **Drivetrain Kill**: Sets throttle to 0 (`driver + 0x20C = 0.0f`), centers steering (`driver + 0x208 = 0.0f`), locks neutral gear on Car (`car + 0x4D0 = 0`), locks transmission neutral (`mover + 0x4C + 0x50/54 = 0`), zeroes engine throttle (`mover + 0x48 + 0x78 = 0.0f`), and stops wheel rotation (`wheel + 0x28 = 0.0f`).
  - **RigidBody Physical Velocity & Momentum Arrest**: In `SimVehicle + 0x2C` (`RigidBody`), hard-clamps all linear velocity ($V_x, V_y, V_z$), angular velocity ($\omega_x, \omega_y, \omega_z$), and momentum vectors to `0.0f` when speed drops below $3.0\text{ m/s}$ ($\sim 11\text{ km/h}$), and aggressively decelerates at higher speeds.
  - Enforced continuously in `OnCollisionForce`, `DamagePlayerCar`, `Update`, and `DelegateDriverInput` (`0x005ABBF0`), completely stopping dead AI racers from creeping forward or rolling down hills.
- [x] **Race Intro Hand-off & PhysicsMover Pointer Stability Fix (`0x0040F697`)**:
  - Fixed fatal `0xC0000005` access violation occurring the exact moment the race intro/countdown ended.
  - Eliminated erroneous `*(uintptr_t*)(car + 0x2C) = mover` overwrite in `VehicleHealthManager_OnDelegateInput`.
  - Restored proper `Car` memory map: `Car + 0x2C` remains native `mRigidBody*` (required by `Player::UpdateGameState` at `0x006027A0` / `0x0040F690`), and `PhysicsMover*` is resolved at `Car + 0x34`.
- [x] **Wine-Compatible Hotkeys**:
  - **F8**: Full repair on all active vehicles.
  - **F9**: Damages player car by 25% for testing.
  - Software debouncing using `GetAsyncKeyState(...) & 0x8000` for reliable single-trigger execution on Linux / Wine.

---

## 5. Next Steps & Research Vectors
1. **Regional Cars & Traffic Car Unlocking**:
   - [x] Peugeot 106 unlock implemented (`RegionalCarsRestoration.cpp`, see section 4.F). Opel Corsa auto-activates once `CARS\CORSA` model data is provided.
   - **Traffic vehicle meshes for Quick Race**: the car select screens read one of two `FEPlayerCarDB` instances selected by `UIQRCarSelect + 0x5C`: showroom DB at `0x83AD90`, second DB at `0x8458DC` (stride `0xAB4C`, 0x5B car record slots at `+0x9BD8`, per-record unlock-filter bitmask at `+0x14`, unique-car unlocked byte at `+0x7F5`, car type at `+0x8`). Investigate which game modes set the DB slot to 1 and whether the second DB holds drivable traffic configurations.
2. **Cut HUD / UI Screens**:
   - `LS_LangSelect.fng` (string at `0x7999A8`): the `LanguageSelectScreen` class is fully present in the PC binary (`CreateLanguageSelectScreen` at `0x4F48A0`, vtable `_vt.20LanguageSelectScreen` at `0x797CF8`, `LanguageChangerCallback` at `0x527520`) and registered in the FE screen constructor table (~`0x7F8C14`), but the `LS_LangSelect.fng` data file does **not** ship on the PC disc — restoration requires importing the console FNG asset.
   - **PS2 Demo career screens** (from WALKTHROUGH section 25): `MU_UG_ChooseCareerCar.fng`, `MU_UG_TradeCareerCar.fng`, `MU_UG_SamanthaGiveth.fng` exist only in the demo build (code + assets) — porting needs FNG extraction from the demo disc data (`ZZDATA0-3` / `Unpacked`).
   - Investigate split-screen 2P HUD remnants (`2PHudBot.fng`, `2PHudTop.fng`) identified in PS2 Alpha 10 — the PS2 Demo still contains `GarageMain_2P.fng` and `GarageMainScreen2P`.
3. **Cut Audio & Radio Chatter**:
   - Explore unused voice lines and SMS message triggers in GameCube/PS2 builds.
4. **PS2 Demo research tooling**:
   - The demo ELF is stripped (no symtab; `.mdebug` empty). The i64-mcp demo DB has no names/xrefs. The working method: capstone `lui/addiu|ori` pair scans + `.data` pointer scans against the ELF directly (see `/tmp/demo_xref.py` approach in WALKTHROUGH 25) and string-set diffs against retail. Consider importing a proper IDA MIPS analysis of the demo into the MCP workspace for future ports.


