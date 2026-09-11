# Walkthrough: Vehicle Health System & Bug Fixes

## Executive Summary

The **Vehicle Health & Damage System** (`NFSU2VehicleHealth.asi`) introduces a complete, standalone vehicle destruction and health tracking architecture to Need for Speed: Underground 2 (PC v1.2 NTSC). The system tracks health for the player car, AI opponents, and ambient traffic, visualizes real-time damage with both a 2D screen-space HUD bar and 3D in-world floating health bars, handles fatal race disqualification (DNF) with authentic last-place standings, protects player health during Free Roam, cleans up despawned traffic cars, and enforces complete multi-layer immobilization to prevent wrecked vehicles from creeping or rolling.

### Feature Matrix

| Feature | Description | Key Mechanism |
| :--- | :--- | :--- |
| **Physical Collision Damage** | Intercepts physical impact forces between cars and world geometry | Hook at `0x005A0BC4` in `CarCollisionBody::AddDamage` / `ApplyImpulse` |
| **Direct3D 9 Hooking (Wine)** | Renders overlays directly through the game's D3D9 device | Intercepts `IDirect3DDevice9::EndScene` (`vtable[42]`) at `0x00870974` |
| **2D HUD Health Bar** | Screen-space player HP bar with trailing damage lag animation | Gradient bar (Green $\to$ Yellow $\to$ Red) anchored top-center |
| **3D Floating World Bars** | Overhead bars on opponents and traffic with distance scaling | Native `eView::WorldToScreen` projection with perspective sizing |
| **Behind-Camera Culling** | Eliminates phantom mirrored bars for cars out of camera vision | Dot product with camera forward vector + Homogeneous Clip-W check |
| **Race Disqualification** | Destructive crash (0 HP) places racer in last place as DNF | Native `RaceStatus::FinishRacer` (`0x00602000`) with `lastPlace = numRacers` |
| **Free Roam Immunity** | Player car takes 0 damage in Career Free Roam and Quick Race Explore | Hooks `StartCareerFreeRoam` (`0x005404A0`), checks Track 4000 & `numRacers <= 1` |
| **Traffic Despawn Purge** | Prevents frozen floating bars on despawned or pooled traffic | Validates `Car + 0x550` (`mIsActive`), $Z \ne -123456$, and `TrafficAI + 0x77D` |
| **Death Immobilization** | Stops 0 HP AI racers from creeping or rolling forward down slopes | 100% foot brake & handbrake, Neutral gear, zero throttle, RigidBody velocity arrest |
| **Debug Hotkeys** | Instant repair (F8) and 25% damage to player (F9) | Software debounced with `GetAsyncKeyState & 0x8000` |

---

## Table of Contents
1. [Car Table Iteration & Wine Hotkeys](#1-car-table-iteration-bug-in-vehiclehealthmanager)
2. [Collision Sensitivity & Damage Scaling](#5-collision-sensitivity--damage-debounce-overhaul)
3. [Floating 3D Overhead Health Bars](#6-floating-3d-health-bars-for-other-vehicles)
4. [AI Opponent Race Disqualification](#7-ai-opponent-disqualification-on-death)
5. [Positioning, Damage Scaling & Player DNF](#8-fine-tuning-positioning-damage-scaling-and-player-dnf)
6. [Motion Lead Compensation & Player Car Filtering](#10-restoring-floating-health-bars-on-other-vehicles--motion-lead-compensation)
7. [Direct3D Depth Range & D3D9 State Hardening](#15-direct3d-depth-range-fix--player-race-disqualification-overhaul)
8. [Native Engine World-to-Screen Projection](#16-fix-3d-world-viewport-projection--event-results-last-place-ranking)
9. [Direct3D 9 Hooking on Wine & Free Roam Immunity](#18-fix-d3d9-hooking-in-wine--disable-player-damage-in-free-roam)
10. [Behind-Camera Projection Culling](#19-eliminate-phantom-projection-of-cars-behind-camera)
11. [Despawned Traffic Car Removal](#20-immediate-removal-of-despawned--pooled-traffic-car-health-bars)
12. [Complete 0 HP Vehicle Immobilization](#21-complete-ai-racer--vehicle-immobilization-at-0-hp)
13. [Fix Race Intro Hand-off Crash (`0x0040F697`)](#22-fix-race-intro-hand-off-crash-0x0040f697-in-playerupdategamestate)

---

## Problem Summary
The user reported two issues:
1. The floating health bars were completely missing in-game.
2. Pressing the F9 hotkey to damage the vehicle did not work.

---

## Root Cause Analysis

### 1. Car Table Iteration Bug in `VehicleHealthManager`
In `TheWorld` (`*(uintptr_t*)0x00890080`):
- `+0x10`: `totalCars` (total vehicles in world).
- `+0x14`: `firstAICarIndex` (value = 1).
- `+0x18`: `trafficStartIndex` (start index of traffic cars).
- `+0x1C + i * 4`: Array of vehicle pointers (`Car*`).

**The Flaw**:
`GetActiveVehiclesForRender()` was computing:
```cpp
int startIndex = *(int*)(world + 0x14); // 1
int endIndex = *(int*)(world + 0x18);
int numCars = endIndex - startIndex;
```
- The **player car is always at index 0** (`world + 0x1C + 0 * 4`). Because the loop started at `startIndex = 1`, the player vehicle was **never included**.
- In FreeRoam or race modes without traffic, `startIndex == endIndex == 1`, resulting in `numCars = 0`. The function returned an empty list, so **no health bars were drawn for any vehicle**.
- `IsValidCar()` also used the same flawed range, causing collision and damage checks on the player car to be rejected as "invalid".

### 2. Wine Key State Handling (`GetAsyncKeyState` Bit 0)
The hotkeys previously checked:
```cpp
if (GetAsyncKeyState(g_HealthConfig.hotkeyDamagePlayer) & 1)
```
Under Wine / Proton on Linux, bit 0 (key toggle status since previous query) is notoriously unreliable or never set. The high bit (`0x8000`), representing the physical key-down state, is the only reliable indicator under Wine.

### 3. Screen Projection Depth Sign
`ProjectWorldToScreen` checked `if (cz <= 1.0f) return false;` before invoking the projection routine. In NFSU2's view matrix convention, points in front of the camera have negative Z ($cz < 0$). This caused all vehicles in front of the camera to be culled immediately.

### 4. Background Direct3D Hooking
The Direct3D `EndScene` hook previously had a 10-second timeout. If the game spent more than 10 seconds in loading screens or intro movies, the hook thread would exit before the Direct3D 9 device was created.

---

## Solutions Implemented

### 1. Fixed Car Array Iteration ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp))
- Iterates `0 <= i < totalCars` on `world + 0x1C + i * 4` where `totalCars = *(int*)(world + 0x10)`.
- Explicitly classifies vehicle at index `0` as the Player car (`isPlayer = true`).
- AI opponents and traffic cars are accurately identified and filtered according to config switches (`ShowPlayerBar`, `ShowOpponentBars`, `ShowTrafficBars`).
- Fallback lookup for player car: if `0x008900AC` is not yet dereferenced, reads directly from `world + 0x1C`.

### 2. Wine-Compatible Debounced Hotkeys ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp))
Implemented software state debouncing using `(GetAsyncKeyState(...) & 0x8000) != 0`:
```cpp
static bool s_F9WasDown = false;
bool f9Down = (GetAsyncKeyState(g_HealthConfig.hotkeyDamagePlayer) & 0x8000) != 0;
if (f9Down && !s_F9WasDown)
{
    VehicleHealthLogger::Log("[Hotkey] F9 pressed: Damaging player vehicle by 25%%.");
    DamagePlayerCar(25.0f);
}
s_F9WasDown = f9Down;
```
Also added logging so every F8 / F9 keypress produces visible verification in `scripts/NFSU2VehicleHealth.log`.

### 3. Screen Projection & Perspective Scaling ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp))
- Computes Euclidean distance $\sqrt{cx^2 + cy^2 + cz^2}$ for depth scaling.
- Invokes native `ProjectNormalized` (`0x005EAB90`) to transform 3D world coordinates directly to normalized $[-1, 1]$ screen space.
- Provides top-center HUD fallback for bumper/hood camera views on the player car.

### 4. Robust Direct3D Hook Thread ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp))
- The hook thread now polls continuously with `Sleep(50)` until `s_HookInstalled == true`, guaranteeing the hook installs regardless of how long the game spends in menus or loading screens.
- Minimal render states are saved and restored directly without per-frame COM allocations.

---

## 5. Collision Sensitivity & Damage Debounce Overhaul

### Problem
The slightest brush or contact instantly dropped vehicle health to 0 HP due to:
1. Collision force multiplier was set too high (`80.0f`).
2. Minimum force threshold was too low (`0.02f`), causing ambient micro-bumps or road scraping to register as heavy impacts.
3. Physics contact events fire multiple times in successive frames during a single impact, causing rapid compounding damage.

### Solution ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp))
- Normalized base damage calculation:
  ```cpp
  float damage = (force * 3.0f) * g_HealthConfig.damageMultiplier;
  ```
- Increased `minForceThreshold` default from `0.02f` to `0.35f`.
- Added a 120ms per-vehicle damage cooldown debounce timer (`lastDamageTick` in `VehicleHealthData`). Subsequent physics impulses within 120ms are ignored to prevent multi-hit stacking on a single collision.

---

## 6. Floating 3D Health Bars for Other Vehicles

### Problem
Floating health bars were completely missing above AI opponents and traffic cars.

### Root Cause Analysis via IDA Pro IDB
Disassembly of `Bin-idb-i64/NFSUnderground2-v1.2-US.i64` revealed:
1. `playerPtr + 0x238` is `CameraMover*`, not `Camera*`.
2. The active `Camera*` is located at `CameraMover + 0x1C` (and at `eView 0 + 0x40`).
3. Calling `0x00444530` was calling `CameraMover::MinGapTopology(bMatrix4*, Car*, bVector3*)`. Because an uninitialized matrix was passed, it invoked `eInvertMatrix` (`0x005BAF90`) on garbage stack memory, filling the matrix with NaNs.
4. The engine **already computes and updates the 4x4 View Matrix every single frame at `Camera + 0x00`**, and the FOV at `Camera + 0xC4`.

### Solution ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp))
- Directly read the active view matrix from `Camera + 0x00` and FOV from `Camera + 0xC4`.
- Implemented robust perspective projection transforming world positions directly to screen coordinates:
  ```cpp
  float cx = worldPos.x * camMtx.m[0][0] + worldPos.y * camMtx.m[1][0] + worldPos.z * camMtx.m[2][0] + camMtx.m[3][0];
  float cy = worldPos.x * camMtx.m[0][1] + worldPos.y * camMtx.m[1][1] + worldPos.z * camMtx.m[2][1] + camMtx.m[3][1];
  float cz = worldPos.x * camMtx.m[0][2] + worldPos.y * camMtx.m[1][2] + worldPos.z * camMtx.m[2][2] + camMtx.m[3][2];
  ```
- Added perspective scaling based on Euclidean depth (`std::clamp(20.0f / depth, 0.40f, 1.25f)`).
- Added downward-pointing chevrons to anchor the bars cleanly above opponent and traffic roofs.
- Kept the top-center docked HUD bar for the player vehicle.

---

## 7. AI Opponent Disqualification on Death

### Problem & Feature Request
When an AI opponent's health reaches 0 during a race, the game should officially disqualify / DNF them in race standings (e.g. "Knocked Out" or "Blown Engine").

### Reverse Engineering Race Finish Logic
From IDA database disassembly:
- `TheRace` pointer: `*(void**)0x00890118`.
- `RacingCar`: `*(uintptr_t*)(car + 0x1C)`.
- Racer index: `*(int8_t*)(racingCar + 0xA)`.
- Current finish status: `*(int16_t*)(racingCar + 0x10)` (`0` = active/unfinished).
- `Race::FinishCar` function: `0x00606CB0` (`__thiscall: void FinishCar(Race* race, int carIndex, int a2, int a3, int reason)`).
- Reason codes:
  - `3`: `RACING_CAR_KNOCKED_OUT` (Circuit, Sprint, URL, Street X).
  - `8`: `RACING_CAR_DRAG_BLOWNENGINE` (Drag races).

### Implementation ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp))
- When an AI car reaches 0 HP, `DisqualifyCarInRace(car)` is invoked:
  ```cpp
  void DisqualifyCarInRace(uintptr_t car)
  {
      void* race = *(void**)0x00890118;
      if (!race) return;
      uintptr_t racingCar = *(uintptr_t*)(car + 0x1C);
      if (!racingCar) return;
      int racerIndex = (int)(*(int8_t*)(racingCar + 0xA));
      int16_t currentStatus = *(int16_t*)(racingCar + 0x10);
      if (currentStatus != 0) return;

      uint32_t raceType = *(uint32_t*)0x0089E7B0;
      int reason = (raceType == 3) ? 8 : g_HealthConfig.disqualifyReason;

      typedef void (__thiscall* FinishCarFn)(void* thisRace, int carIndex, int a2, int a3, int reason);
      ((FinishCarFn)0x00606CB0)(race, racerIndex, 0, 0, reason);
  }
  ```
- Configurable via `DisqualifyOnDeath` (default `1`) and `DisqualifyReason` (default `3`) in [`NFSU2VehicleHealth.ini`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/GAME/PC/scripts/NFSU2VehicleHealth.ini).

---

## 8. Fine-Tuning Positioning, Damage Scaling, and Player DNF

### 1. Collision Sensitivity Further Rebalanced
The user reported that `force * 3.0f` was still too aggressive (a bump of force 24 in logs dealt 72 HP damage).
- Lowered the base multiplier from `3.0f` to `0.40f`:
  ```cpp
  float damage = (force - minForceThreshold) * g_HealthConfig.damageMultiplier * 0.40f;
  ```
- Increased `minForceThreshold` to `0.50f`.
- Result: light taps (~5 force) deal ~2 HP, medium hits (~20 force) deal ~8 HP, solid collisions (~40 force) deal ~16 HP, and extreme high-speed head-on crashes (~100 force) deal ~40 HP.

### 2. Fixing Health Bar Vertical Positioning & Lagging Behind Moving Cars
- **Why it was sitting near the road**: `HealthBarHeightOffset` was `1.35f` from the car axle/ground coordinate, placing the bar right at trunk/bumper level. Bumped to `2.25f` meters, elevating the bar cleanly into the air above the car roof.
- **Why it was "falling behind" moving cars**: `Car + 0x60` was reading the raw physics body position from the physics tick, which runs asynchronously from the rendering thread. The game's visual mesh is rendered using `Car + 0x5A0` (interpolated transform when bit 0 of `Car + 0x5B0` is set) or `Car + 0x50` (base visual transform). Switching to this rendering position eliminates the visual lag completely.
- **Integrated Native `eView::WorldToScreen` (`0x005BC4A0`)**: Used by `OnlineHUDSupport` to project floating overhead player markers onto the viewport.

### 3. Player Car Disqualification on 0 HP
- Enabled `DisqualifyCarInRace` for the player vehicle (`racerIndex = 0`) when player health drops to 0 HP during a race.
- Calls `FinishCar(race, 0, 0, 0, reason)` to cleanly mark the player as Knocked Out / DNF in race standings.

---

## 10. Restoring Floating Health Bars on Other Vehicles & Motion Lead Compensation

### Problem
In the previous build, floating health bars stopped rendering completely above AI opponents and traffic vehicles.

### Root Cause
1. **`s_WorldToScreen` (`0x005BC4A0`) Incompatibility**: Calling `0x005BC4A0` on `eView[0]` in single-player either returned negative depth or failed, and the function executed an unconditional `return false;` that prevented the working mathematical projection fallback from ever being reached.
2. **Invalid Memory Offsets in `GetActiveVehiclesForRender`**: Attempting to read `Car + 0x5A0` / `0x50` read uninitialized memory locations rather than the true car coordinates at `Car + 0x60`, `0x64`, `0x68`.

### Solution
1. **Restored Proven Camera Pose Perspective Projection** ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp)):
   - Reads the active Camera pose matrix at `Camera + 0x00` and FOV at `Camera + 0xC4`.
   - Transforms world target coordinates by subtracting Camera Eye Position (`camMtx.m[3]`) and projecting along the Camera Right, Up, and Forward basis vectors.
2. **True Car Position with Velocity Lead Compensation** ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp)):
   - Base coordinates read directly from `Car + 0x60`, `0x64`, `0x68`.
   - Bumps the vertical float height by `g_HealthConfig.healthBarHeightOffset` (`2.25f` meters) to position the bar directly above the car roof.
   - Reads linear velocity at `Car + 0x80`, `0x84`, `0x88` and applies 1 frame (16.6ms) of forward lead compensation (`cx = pos.x + vx * 0.0166f`). This eliminates any visual lag or trailing behind fast-moving vehicles.

---

## 12. Fix Player Car Identification & Accurate Overhead Bar Centering

### Problems Reported
1. **Bars Positioned on Left Sidewalk**: Floating health bars appeared shifted far to the left of vehicles over the sidewalk.
2. **F9 Damaged 1st Place Car Instead of Player**: Pressing the F9 test hotkey damaged Timmy (1st place) instead of the local player ("No Profile", 3rd place).

### Root Cause Analysis
1. **False Velocity Lead Shift**: Attempting to read `Car + 0x80` as world velocity added large negative offsets during forward motion, shifting the projected world X coordinates into the sidewalk.
2. **Flawed `i == 0` Player Assumption**: `DamagePlayerCar` and `GetActiveVehiclesForRender` assumed `i == 0` in `TheWorld` was always the player car. In races, cars in `TheWorld` are ordered by race standings; car `0` was the leader (Timmy), while the player was at index `2`.

### Solutions Implemented
1. **Bulletproof Player Car Resolution (`GetPlayerCar`)** ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp)):
   - Inspects `DriverInfo` at `Car + 0x14` for every active car.
   - Specifically checks `*(int*)(driverInfo + 4) == 1` (`DRIVER_HUMAN`). In NFSU2, this is exclusively assigned to the local human player across all race modes and Free Roam, regardless of race standing or world array index.
   - `DamagePlayerCar` and `OnCollisionForce` now accurately and exclusively target the player's car.
2. **True Car Coordinates with Forward Centering Offset** ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp)):
   - Removed the faulty velocity lead.
   - Directly reads car position coordinates `(Car + 0x60, 0x64, 0x68)`.
   - Projects a configurable forward offset (`HealthBarForwardOffset`, default `0.20m`) along the car's normalized forward orientation vector `(Car + 0x90, 0x94, 0x98)`. This centers the floating bar directly over the cabin roof rather than the rear axle/trunk.

---

## 13. Build & Deployment Verification

Built using:
```bash
nix-shell --run "make"
```

Deployment status:
- [`GAME/PC/scripts/NFSU2VehicleHealth.asi`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/GAME/PC/scripts/NFSU2VehicleHealth.asi) compiled and deployed.
- [`GAME/PC/scripts/NFSU2VehicleHealth.ini`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/GAME/PC/scripts/NFSU2VehicleHealth.ini) updated with `HealthBarForwardOffset = 0.20`.
- 0 compile errors.

---

## 14. Native Engine 3D-to-Screen Projection & Prominent Sizing Overhaul

### Problems Reported
1. Floating health bars on other cars were positioned offscreen on the left sidewalk/curb.
2. Bars were "tiny af" (shrunk down to microscopic horizontal slashes).

### Analysis of `Reference-Code/MWHealthbars` & `SPEED2.EXE` Disassembly
1. In `Reference-Code/MWHealthbars`, health bars are positioned in 3D space above the car roof:
   ```cpp
   D3DXVECTOR3 barWorldPos = D3DXVECTOR3(car->position[2], -car->position[0], car->position[1] + height);
   ```
2. In `SPEED2.EXE`, the engine already has a native function to project 3D world coordinates directly to screen pixels:
   - `View::WorldToScreen` at `0x005BC4A0` (`void __thiscall View::WorldToScreen(bVector3* outScreen, const bVector3* inWorld)`).
   - Calling convention: `this` is `View*` (obtained from `0x00832DE0` via `GetView(0)` at `0x0048B1E0`).
   - `outScreen->x`: exact pixel X on screen (0 .. viewport width).
   - `outScreen->y`: exact pixel Y on screen (0 .. viewport height).
   - `outScreen->z`: view-space clip depth $W$. If $W \le 0.5f$, the point is behind the camera.
3. **Why Manual Projection Failed**:
   - The previous manual projection in `HealthBarRenderer.cpp` made standard OpenGL/Direct3D basis assumptions that did not match the EAGL camera basis at `Camera + 0x00`. Lateral and forward axes were mismatched, causing negative X offsets (projecting to pixels 150-250 on the left edge) and huge depth calculations.
   - Because depth was inflated, `distScale` hit the minimum clamp (`0.40f`), reducing the 90px bar down to a 36px tiny bar ("tiny af").

### Solutions Implemented
1. **Engine Native `View::WorldToScreen` Integration** ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp)):
   - Directly calls `s_ViewWorldToScreen((void*)0x00832DE0, &screenPos, &worldPos)`.
   - Eliminates all synthetic camera matrix transformations, FOV calculations, and aspect ratio bugs.
   - Screen coordinates are generated by the game engine's exact `ViewProj` matrix for the current frame.
2. **Prominent, Legible Perspective Sizing**:
   - Increased default bar dimensions to `100px x 10px` (`HealthBarWidth = 100`, `HealthBarHeight = 10`).
   - Improved distance scaling curve: `distScale = std::clamp(28.0f / depth, 0.50f, 1.35f)`.
   - Added minimum size clamps: `barW = std::max(52.0f, barW)` and `barH = std::max(6.0f, barH)`. The bar will never become a microscopic dot regardless of distance.
3. **Clean Roof Height Offset** ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp)):
   - Directly anchors the health bar on `(cx, cy, cz + HealthBarHeightOffset)` where `HealthBarHeightOffset = 1.85m`.
   - Removed forward orientation skewing, placing the bar directly and squarely over the vehicle roof.

---

## 15. Direct3D Depth Range Fix & Player Race Disqualification Overhaul

### Problems Reported
1. Health bars did not show up at all (even on traffic cars).
2. When player car health depleted to 0 HP, it did not disqualify the player in race status.

### Root Cause Analysis
1. **Depth Range Misunderstanding in `ProjectWorldToScreen`**:
   - `s_ViewWorldToScreen` (`0x005BC4A0`) writes normalized device coordinate $Z_{ndc} = Z_{clip} / W$ into `screenPos.z`, not distance $W$ in meters!
   - In Direct3D, $Z_{ndc}$ is between `0.0f` and `1.0f`.
   - The previous check had: `if (screenPos.z <= 0.5f || screenPos.z > 140.0f) return false;`.
   - Any car in the near half of the view frustum has $Z_{ndc} \le 0.5f$, so **every single car in front of the camera was culled**!
   - Additionally, `s_ProjLogCount` was inside `if (projected)`, so when everything was culled, nothing was logged.
2. **Silent Failure in `DisqualifyCarInRace`**:
   - `racerIndex` was read from `*(int8_t*)(racingCar + 0xA)`. Disassembly of `SPEED2.EXE` (`0x6078CD`) confirmed offset `0xA` is NOT `racerIndex` (it is a car/driver type flag).
   - Because `racerIndex` was invalid or out of range, the function exited silently before calling `Race::FinishCar` (`0x00606CB0`).
   - Furthermore, the player's race coordinator flags (`0x83ABC4` and `0x83ABCC`) were not being set, which is required by the game engine to advance the race state machine into the post-race / DNF sequence for the local player.

### Solutions Implemented
1. **Correct Direct3D Depth & True Distance Sizing** ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp)):
   - Updated `ProjectWorldToScreen` to check the proper Direct3D depth range: `if (screenPos.z < 0.0f || screenPos.z > 1.0f) return false;`.
   - Distance for perspective scaling is now computed directly as true Euclidean 3D distance between the player vehicle and the target vehicle (`std::sqrt(dx*dx + dy*dy + dz*dz)`).
   - Added unconditional debug logging on the first 30 projection attempts so exact coordinates are always visible in `scripts/NFSU2VehicleHealth.log`.
2. **Robust Racer Matching & Game State Notification** ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp)):
   - `DisqualifyCarInRace` now scans the active `race->racingCars` array at `Race + 0xBD0` to find the exact matching `racerIndex` (`0 <= i < numRacers`).
   - For the local human player, falls back to `racerIndex = 0` (player is always racer 0 in single-player races).
   - Invokes `Race::FinishCar(race, racerIndex, 1, 0, reason)` with `a2 = 1`.
   - For the player car, also sets `*(uint32_t*)0x0083ABC4 = 1;` and `*(uint8_t*)0x0083ABCC = reason;`, cleanly transitioning the game into the Knocked Out / Totalled / DNF race end screen.
   - Every step of `DisqualifyCarInRace` is fully logged to `scripts/NFSU2VehicleHealth.log`.

---

## 16. Fix 3D World Viewport Projection & Event Results Last Place Ranking

### Problems Reported
1. Floating health bars on other cars still didn't show up.
2. When the player car health depleted to 0 HP during a race, the player was awarded **1st place** ("Event Results: 1 No Profile RX-8 --:--.--", winning the event) instead of being placed in **last place / disqualified**.

---

### Root Cause Analysis via Binary Disassembly of `SPEED2.EXE`

#### 1. Why Health Bars Did Not Project (`s_GetView(0)` vs `s_GetView(1)`)
- In `HealthBarRenderer.cpp`, projection called `s_ViewWorldToScreen` (`0x005BC4A0`) with `pView = s_GetView(0)`.
- Disassembly of `SPEED2.EXE` at `0x0048B1E0` (`GetView`):
  - View index `0` (`0x00832DE0`) is the **2D Front-End / UI view**. Its cached matrix at `*(uint32_t*)view + 0x80` is an orthographic 2D UI matrix (or identity).
  - When multiplying 3D world coordinates like `World(-1052.9, 310.1, 14.9)` by View 0's matrix, it produced huge out-of-bounds screen coordinates: `Screen(-1009780.9, -166940.2, Zndc=14.8804)`.
  - Disassembly of all 3D gameplay rendering functions in `SPEED2.EXE` (e.g. `0x005C7597`, `0x005C787C`, `0x0060097E`, `0x0061AB63`, `0x0061E0F0`) revealed that **every 3D gameplay rendering call explicitly invokes `GetView(1)`** (`push $0x1; call 0x48b1e0`).
  - View index `1` (`0x00832E50`) is the **active 3D world/gameplay view**. Its matrix at `*(uint32_t*)view + 0x80` is the actual composite 3D World-View-Projection matrix!
  - Furthermore, `*(uintptr_t*)(view + 0x40)` points directly to the active `Camera*` struct containing the camera position, basis rotation vectors, and FOV angle (`Camera + 0xC4`).

#### 2. Why Dying at 0 HP Placed the Player in 1st Place
- Disassembly of `Race::FinishCar` at `0x00606CB0`:
  ```assembly
  00606df4: mov 0xc(%esi), %al        ; al = racingCar->finish_position
  00606df7: test %al, %al             ; already non-zero?
  00606df9: jne 0x606e17             ; IF ALREADY SET, SKIP RE-CALCULATION!
  ...
  00606e06: call 0x5eefb0            ; 0x5eefb0 assigns last place (e.g. 4)
  00606e14: mov %al, 0xc(%esi)
  ...
  00606e7d: movsbl 0xc(%esi), %edx   ; edx = finish position
  00606e8e: call 0x602000            ; RaceStatus::FinishRacer(carIndex, finishPos, 0, reason)
  ```
  - During a race, `racingCar + 0x0C` continuously holds the car's **current live track position**.
  - When the player took damage while leading the race, `racingCar + 0x0C` was equal to `1`.
  - Because `0xc(%esi)` was already non-zero, `FinishCar` saw `test %al, %al != 0` and skipped calling `0x5eefb0`, preserving `1`!
  - Then in `0x00609609` (Results Calculation):
    ```assembly
    00609609: cmpb $0x1, 0xc(%eax)    ; Is finish position == 1?
    0060960d: jne 0x60961b
    0060960f: mov %cl, 0xd8(%ebx)     ; MARK CAR AS WINNER!
    ```
  - Because `0xc(%eax)` was `1`, the game marked the dead player car as the event winner and placed them at #1 on the Event Results screen!

---

### Solutions Implemented

#### 1. 3D World Projection & Robust Fallback ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp))
- Switched `s_GetView` index from `0` to `1` (`(void*)0x00832E50`) in both `ProjectWorldToScreen` and `Render`.
- Added resolution scaling: multiplies `screenPos.x` and `screenPos.y` by `vp.Width / screenW` and `vp.Height / screenH` (where `screenW = *(int*)0x00870980`, `screenH = *(int*)0x00870984`) so coordinates match the Direct3D viewport at any resolution.
- Added a direct `Camera` rotation matrix & FOV projection fallback (using `Camera*` at `pView + 0x40`, exactly matching `CarRenderInfo::Neon` at `0x0060D7F0` in `SPEED2.EXE`) ensuring projection never fails even if `s_ViewWorldToScreen` clipping encounters edge cases.

#### 2. Last Place Standings on Disqualification ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp))
- In `DisqualifyCarInRace`:
  - Dynamically computes `lastPlace = numRacers` (or the lowest unoccupied rank from `numRacers` down to 1 if other cars were previously disqualified).
  - Explicitly overrides `racingCar + 0x0C`, `racingCar + 0x0E`, and `racingCar + 0x14` with `(uint8_t)lastPlace` before calling `fnFinishCar`.
  - Calls `fnFinishCar(race, racerIndex, lastPlace, 0, reason)`:
    - `0xc(%esi)` is now `lastPlace` (e.g. `4`).
    - `FinishCar` records `4` in `RaceStatus::FinishRacer` (`0x00602000`).
    - The Event Results screen places the player at the bottom (4th of 4) with `--:--.--` DNF status, while active AI cars take 1st, 2nd, and 3rd place.

---

## 17. Comprehensive Bug Check & Hardening Audit

### Audited Components & Edge Cases Resolved

1. **Eliminated Thread Race on Direct3D Hooking (`HealthBarRenderer.cpp`)**:
   - **Issue**: Both the background installer thread `D3DHookThread` and the main game loop hook `CheckInstallHook()` were checking `!s_HookInstalled` without synchronization. A simultaneous hook attempt could have clobbered `s_OrigEndScene` with `Hooked_EndScene`, causing infinite recursion and stack overflow.
   - **Fix**: Consolidated hook installation into `InstallHookInternal()` protected by a Win32 `CRITICAL_SECTION`. Guaranteed atomic, single-execution hook installation.

2. **D3D Render State Leak & Fog/Scissor Pollution (`HealthBarRenderer.cpp`)**:
   - **Issue**: `D3DRS_FOGENABLE` and `D3DRS_SCISSORTESTENABLE` were unmanaged. In foggy weather or smoke, health bars could be faded or greyed out by atmospheric fog; leftover scissor rectangles from prior UI drawing could clip bars.
   - **Fix**: Explicitly saved, disabled, and restored `D3DRS_FOGENABLE` and `D3DRS_SCISSORTESTENABLE`.
   - **Fix**: Added viewport dimension validation (`vp.Width == 0 || vp.Height == 0`) and screen dimension fallbacks (`screenW`, `screenH`) preventing any zero-division on minimized windows or device resets.

3. **Divide-by-Zero & NaN/Inf Immunity (`VehicleHealthManager.cpp`, `HealthBarRenderer.cpp`, `Config.cpp`)**:
   - **Issue**: If physics collisions produced `NaN` or `Inf` forces (e.g. car geometry glitching into world meshes), damage calculations would propagate `NaN` into health, permanently corrupting vehicle state. Also, `entry.health.maxHealth <= 0` could cause divide-by-zero.
   - **Fix**: Added `std::isnan` and `std::isinf` checks to `OnCollisionForce` and `DamagePlayerCar`.
   - **Fix**: Clamped `maxHealth` to a minimum of `1.0f` before computing `hpPct` and `lagPct`.
   - **Fix**: Clamped camera/player distance `depth` to a minimum of `1.0f` before perspective scaling.
   - **Fix**: Added sanitization and clamping to `Config::Load` ensuring `maxHealth > 0.0f`, `damageMultiplier >= 0.0f`, `minForceThreshold >= 0.0f`, and minimum bar pixel dimensions.

4. **Pointer Bounds & Memory Safety (`VehicleHealthManager.cpp`)**:
   - **Issue**: Pointers from `TheWorld` (`0x00890080`), `TheRace` (`0x00890118`), and `PhysicsMover` could occasionally become dangling or out-of-range during level unloads or memory transitions.
   - **Fix**: Added pointer range verification (`addr >= 0x00400000 && addr < 0x7FFFFFFF`) before dereferencing `world`, `race`, `rc`, `racingCar`, `driver`, `mover`, and `car` in all hooks and methods.
   - **Fix**: In the periodic despawn purge loop, protected player health data (`!mapIt->second.isPlayer`) so local player health is never accidentally purged during scene transitions.

5. **Log File I/O Optimization**:
   - **Issue**: Continuous 60Hz logging of disqualification or periodic logging every 5 seconds could cause file bloat and disk I/O stutter over prolonged gameplay sessions.
   - **Fix**: Limited render frame logging to the first 5 frames, and restricted disqualification logging to single-shot execution.

---

## 18. Fix D3D9 Hooking in Wine & Disable Player Damage in Free Roam

### Problems Reported
1. **Health bars stopped displaying completely**: Even the player's 2D HUD health bar at the top center of the screen failed to show up.
2. **Player health in Free Roam**: When driving in Quick Race Free Roam or Career Free Roam, crashes into scenery or traffic decreased the player's health, and depleting health caused the race disqualification system to trigger as `racer #0 (rank 1/1)`. The user requested disabling player health decrease in Free Roam and Career Free Roam.

---

### Root Cause Analysis & Fixes

#### 1. Why Health Bars Did Not Display (`pDevice` address range on Wine)
- **Root Cause**: During the previous hardening pass, `InstallHookInternal` checked:
  ```cpp
  uintptr_t pDevice = *(uintptr_t*)0x00870974;
  if (pDevice && pDevice > 0x00400000 && pDevice < 0x7FFFFFFF)
  ```
  In Wine (WineD3D), the `IDirect3DDevice9` COM object is allocated on the process heap via `HeapAlloc(GetProcessHeap())`.
  In 32-bit Wine/Windows processes, the default process heap resides in the memory range `0x00100000`–`0x003FFFFF`, which is **below** the executable image base (`0x00400000`)!
  Because `pDevice` was `< 0x00400000`, the check evaluated to `false` every frame, causing `Hooked_EndScene` to **never be hooked** (`vtable[42]` was never patched).
- **Fix**: Changed pointer validation to `pDevice >= 0x00010000` (the standard Win32 boundary above the 64KB null trap page).
- **HUD Bar Decoupling**: Moved `pView` validation out of the top of `Render()` into `ProjectWorldToScreen()`. Because the local player HUD health bar is purely 2D screen-space, it is never dependent on the 3D world camera view matrix being initialized.

#### 2. Disabling Player Health Decrease in Free Roam & Career Free Roam
- **Detection Mechanism (`IsInFreeRoam()`)**:
  - Implemented dynamic state tracking:
    1. Hooked `StartCareerFreeRoam` (`0x005404A0`) to flag `s_IsInCareerFreeRoam = true`.
    2. Hooked `StartRace` (`0x0053FC20`) to clear `s_IsInCareerFreeRoam = false`.
    3. Checked `TrackID` at `0x0089E7A0`: Track `4000` (`0xFA0`) is exclusively Bayview Open World (used by Career Free Roam and Quick Race Explore Mode).
    4. Checked `TheRace` racer count (`race + 0x24`): In Free Roam, `numRacers <= 1` (no competitive opponents).
- **Behavior Changes**:
  - **`OnCollisionForce`**: When `it->second.isPlayer && IsInFreeRoam()`, collision impacts to the player car are ignored and deal 0 damage. Traffic and opponent cars continue to take damage normally.
  - **`DamagePlayerCar`**: When in Free Roam, programmatic/debug damage to the player car is ignored.
  - **`DisqualifyCarInRace`**: Skipped completely when in Free Roam (prevents bogus "Rank 1/1 Disqualified" screens).
  - **`Update`**: While in Free Roam, the player car is locked at 100% full health.
- **Config Option**:
  - Added `DisablePlayerHealthInFreeRoam = 1` under `[Gameplay]` in `scripts/NFSU2VehicleHealth.ini`.

---

## 19. Eliminate Phantom Projection of Cars Behind Camera

### Problem Reported
- When an opponent or traffic car was located **behind** the player/camera (out of the camera's field of view), its health bar was still being rendered on screen (reflected into the forward view plane).

---

### Root Cause Analysis & Solution
- **The Perspective Inversion Bug**:
  - In 3D perspective projection math:
    $$\text{Clip}_W = \mathbf{worldPos} \cdot \mathbf{M}_{\text{proj\_col3}}$$
  - For points in front of the camera, $\text{Clip}_W > 0$.
  - For points **behind** the camera, $\text{Clip}_W < 0$.
  - `eView::WorldToScreen` computes normalized device coordinates by dividing by $\text{Clip}_W$ ($\frac{\text{Clip}_X}{\text{Clip}_W}$).
  - When both $\text{Clip}_X < 0$ and $\text{Clip}_W < 0$, the quotient $\frac{-\text{Clip}_X}{-\text{Clip}_W}$ becomes positive!
  - This mathematically mirrors/inverts points behind the camera into the forward viewport, causing a phantom health bar to appear on the screen.
- **Solution ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp))**:
  - Added strict near-plane & forward-vector culling at the beginning of `ProjectWorldToScreen()` before any projection calculations:
    1. **Camera Line of Sight Dot Product**:
       $$\text{camSpaceZ} = (\mathbf{worldPos} - \mathbf{camPos}) \cdot \mathbf{camForward}$$
       If $\text{camSpaceZ} \le 0.5\text{m}$, the car is behind the camera or touching the lens and is **immediately culled** (`return false;`).
    2. **Homogeneous Clip-W Plane Check**:
       If $W_{\text{clip}} \le 0.5\text{m}$, the object is behind the near projection plane and is **immediately culled** (`return false;`).
  - Guarantees that only cars genuinely located in front of the active camera's view frustum can project and render.

---

## 20. Immediate Removal of Despawned & Pooled Traffic Car Health Bars

### Problem Reported
- When traffic cars despawn, their floating health bars remained frozen in the world at the exact location where they despawned (e.g. floating above the empty road barrier/palm tree with 100% full green health).

---

### Root Cause Analysis (Disassembly of `SPEED2.EXE`)
1. **Static Traffic Vehicle Pool**:
   - In NFSU2's `TheWorld` (`0x00890080`), vehicles are stored in a fixed array `world + 0x1C + i * 4` where `totalCars = *(int*)(world + 0x10)`.
   - When a traffic car despawns (due to distance, being passed, or reaching its destination), the game **never clears or nulls** the pointer in `world + 0x1C + i * 4`. The car structure remains allocated in the pool for reuse.
2. **Engine Despawn Mechanisms**:
   - **`TrafficController` Despawn (`0x0042C92A` & `0x004099FD`)**:
     - Calls `Car::SetActive(car, false)` (`0x005ED5C0`), which sets `*(uint8_t*)(car + 0x550) = 0` (`mIsActive = 0`).
     - Sets the traffic AI inactive flag `*(uint8_t*)(car->trafficAI + 0x77D) = 1` (`0x00409A02`).
     - Crucially, it **does not reset the position coordinates** (`car + 0x60`, `0x64`, `0x68`). The coordinates remain frozen at the exact spot on the road where the traffic car despawned!
   - **`Car::Despawn` (`0x005F6E20`)**:
     - Sets `*(uint8_t*)(car + 0x550) = 0` and teleports off-world: $x = 0, y = 0, z = -123456.0\text{f}$ (`0x008025B8`).
3. **The Flaw in `VehicleHealthManager`**:
   - `GetActiveVehiclesForRender()` unconditionally iterated all `totalCars` in `world + 0x1C + i * 4` without checking whether each vehicle was actually active or spawned in the world.
   - For pooled or despawned traffic cars that were deactivated via `Car::SetActive(car, false)`, their coordinates remained frozen at the road, causing `VehicleHealthManager` to continuously register them into `s_HealthMap` and render a 100% green health bar in empty air.

---

### Solution Implemented

1. **Active & Spawn State Verification (`IsCarActive`)** ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp)):
   - Implemented `VehicleHealthManager::IsCarActive(uintptr_t car)` mirroring the native engine checks in `SPEED2.EXE` (`0x005ED430`, `0x0061B218`, `0x005F152C`, `0x0060A1DB`):
     1. **`Car + 0x550` (`mIsActive`)**: Must be non-zero (1 = active/spawned in world; 0 = pooled/despawned/inactive).
     2. **`Car + 0x68` ($Z$ coordinate)**: Must not be NaN, infinite, or parked off-world at $Z = -123456.0\text{f}$.
     3. **`Car + 0x2C` (`TrafficAI`)**: For traffic vehicles, verifies `*(uint8_t*)(trafficAI + 0x77D) == 0` (non-zero indicates sleeping/inactive in `TrafficController`).
     4. **Local Player Car**: Always guaranteed active.

2. **Immediate Purge & Skipping in `GetActiveVehiclesForRender()`** ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp)):
   - Inside the vehicle render collection loop:
     ```cpp
     if (!IsCarActive(car))
     {
         if (car != playerCar)
         {
             s_HealthMap.erase(car);
         }
         continue;
     }
     ```
   - Instantly purges the despawned vehicle from `s_HealthMap` and omits it from the render list.
   - When the vehicle slot is later respawned as a new car, it starts fresh at 100% HP without carrying stale state.

3. **Active Check in `HealthBarRenderer`** ([`HealthBarRenderer.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/HealthBarRenderer.cpp)):
   - Added `if (!entry.carPtr || !VehicleHealthManager::IsCarActive(entry.carPtr)) continue;` before 3D world-to-screen projection as an extra defense layer.

4. **Periodic & Update-Loop Cleanup** ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp)):
   - `Update()` immediately removes any tracked vehicle that becomes inactive (`!IsCarActive(car)`).
   - The periodic cleanup timer checks `!IsCarActive(mapIt->first)` every 30 frames to prevent memory leaks during extended Free Roam sessions.

---

## 21. Complete AI Racer & Vehicle Immobilization at 0 HP

### Problem Reported
- AI racers still somewhat moved (extremely slow creep/rolling forward) after their HP reached 0.

---

### Root Cause Analysis (Vehicle Drivetrain & Physics Disassembly)
1. **Unapplied & Zeroed Brakes**:
   - In prior implementations of `OnCollisionForce()`, `DelegateDriverInput()`, and `Update()`, when a vehicle reached 0 HP, driver input offsets were set as follows:
     - `driver + 0x20C = 0.0f` (Throttle)
     - `driver + 0x210 = 0.0f` (Foot Brake)
     - `driver + 0x214 = 0.0f` (Handbrake)
   - In NFSU2, `0.0f` on Foot Brake and Handbrake means **0% braking resistance** (completely released brakes).
   - Leaving a vehicle in Neutral with 0% brakes allowed physics rolling inertia, incline gravity on slopes, and engine idle creep to keep pushing the car forward indefinitely at ~2–5 km/h.

2. **Transmission & Engine Automatic Re-engagement**:
   - Inside `PhysicsMover::DelegateDriverInput` (`0x005ABBF0`), the transmission object at `mover + 0x4C` and engine controller at `mover + 0x48` receive gear and throttle updates.
   - Even when `car + 0x4D0` was set to Neutral, the internal transmission gear (`mover + 0x4C + 0x54`) remained in gear 1 or Drive, producing engine idle crawl torque through the automatic transmission torque converter.

3. **Physics Residual Velocity in `RigidBody`**:
   - `Car + 0x1C` points to `SimVehicle`, and `SimVehicle + 0x2C` (also `PhysicsMover + 0x20`) points to the active `RigidBody` (`0x7a20c4`).
   - The `RigidBody` holds the physical linear velocity vector at `+0x70` ($V_x$), `+0x74` ($V_y$), `+0x78` ($V_z$), 2D speed at `+0x24` and `+0x28`, angular velocity vector at `+0x80` ($\omega_x$), `+0x84` ($\omega_y$), `+0x88` ($\omega_z$), and linear/angular momentum at `+0xA0`..`+0xB8`.
   - Without active zeroing or damping, low residual velocity from the fatal collision or road grade kept the vehicle creeping forward on pavement.

---

### Solution Implemented ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp))

1. **Centralized Immobilization Engine (`ImmobilizeCar`)**:
   - Implemented a dedicated function `VehicleHealthManager::ImmobilizeCar(uintptr_t car)` that performs complete multi-layer immobilization:
     1. **Driver Inputs (`Car + 0x30`)**:
        - Steering centered: `*(float*)(driver + 0x208) = 0.0f;`
        - Forward throttle killed: `*(float*)(driver + 0x20C) = 0.0f;`
        - **100% Full Foot Brake Applied**: `*(float*)(driver + 0x210) = 1.0f;`
        - **100% Full Handbrake Locked**: `*(float*)(driver + 0x214) = 1.0f;`
        - Car gear set to Neutral: `*(int*)(car + 0x4D0) = 0;`
     2. **PhysicsMover & Drivetrain (`Car + 0x34`)**:
        - Transmission neutral locked: `*(int*)(trans + 0x50) = 0; *(int*)(trans + 0x54) = 0;`
        - Engine throttle killed: `*(float*)(engine + 0x78) = 0.0f;`
        - Wheel rotation locked: All 4 wheel angular velocities (`*(float*)(wheel + 0x28)`) locked to `0.0f`.
     3. **RigidBody Physical Velocity & Momentum Arrest (`Car + 0x2C` / `SimVehicle + 0x2C`)**:
        - Computes vehicle speed squared: $\text{speedSq} = V_x^2 + V_y^2 + V_z^2$.
        - **Low-speed arrest**: When speed is below $3.0\text{ m/s}$ ($\sim 11\text{ km/h}$, $\text{speedSq} < 9.0\text{f}$), all linear velocity ($V_x, V_y, V_z$), angular velocity ($\omega_x, \omega_y, \omega_z$), 2D speed, and momentum vectors are **hard-clamped to exactly `0.0f`**.
        - **High-speed braking**: When fatal impact occurs at high speed ($\ge 3.0\text{ m/s}$), velocities and momentum are aggressively decelerated by factor $0.80$ each frame, bringing the vehicle to a swift, natural halt within fractions of a second before zeroing out.

2. **Integration Across All Execution Paths**:
   - **`VehicleHealthManager_OnDelegateInput` (`0x005ABBF0`)**: Executed every physics sub-step before inputs are delegated. Invokes `ImmobilizeCar(car)` if the car is marked dead, overriding any AI racer steering/throttle recalculations.
   - **`OnCollisionForce`**: Called the instant a collision reduces vehicle health to 0, immediately triggering braking and decelerating physics momentum.
   - **`DamagePlayerCar`**: Triggers full immobilization if player car reaches 0 HP in races.
   - **`Update(float dt)`**: Continuously enforces `ImmobilizeCar(car)` every frame for any destroyed vehicle, ensuring slopes, bumps, or external nudges cannot cause creeping.

---

## 22. Fix Race Intro Hand-off Crash (`0x0040F697` in `Player::UpdateGameState`)

### Problem Reported
- The game was crashing with a fatal `0xC0000005` Access Violation at the exact moment the race intro/countdown camera sequence ended and gameplay controls engaged.

---

### Root Cause Analysis (Disassembly & Crash Log Diagnostics)

1. **Crash Diagnostic Dump**:
   From `GAME/PC/scripts/NFSU2VehicleHealth.log`:
   ```
   [CRASH] FATAL EXCEPTION 0xC0000005 CAUGHT
   [CRASH] Faulting Address: 0x0040F697 (SPEED2.EXE+0xF697, base=0x00400000)
   [CRASH] Attempted to READ memory at 0x7379685C
   [CRASH] Code bytes at EIP: 8B 40 0C 25 FF 07 00 00 8B 04 85 34 A3 88 00 C3 
   [CRASH] Registers:
   [CRASH]   EAX=0x73796850 EBX=0x00000000 ECX=0x039FD960 EDX=0x07AC5FB8
   [CRASH]   ESI=0x0FE58710 EDI=0x0FE52B38 EBP=0x00C3FE40 ESP=0x00C3FE0C
   [CRASH] Stack dump (ESP):
   [CRASH]   [ESP+0x00] = 0x006027AB (SPEED2.EXE+0x2027AB, base=0x00400000)
   ```

2. **Caller Disassembly at `0x006027A0` (`Player::UpdateGameState`)**:
   ```asm
   0x006027A0: mov edx, dword ptr [esi + 4]    ; edx = player->mVehicle (Car*)
   0x006027A3: mov ecx, dword ptr [edx + 0x2c] ; ecx = [Car + 0x2C] (mRigidBody*)
   0x006027A6: call 0x40f690                   ; OBB / collision bound check
   0x006027AB: test eax, eax
   ```

3. **Faulting Callee at `0x0040F690`**:
   ```asm
   0x0040F690: mov eax, dword ptr [ecx + 0xc]  ; eax = [RigidBody + 0x0C]
   0x0040F693: test eax, eax
   0x0040F695: je 0x40f6a7
   0x0040F697: mov eax, dword ptr [eax + 0xc]  ; CRASH: dereferencing [eax + 0x0C]
   0x0040F69A: and eax, 0x7ff
   0x0040F69F: mov eax, dword ptr [eax*4 + 0x88a334]
   0x0040F6A6: ret
   ```

4. **The Stomped Pointer**:
   - In `Registers`: `EAX = 0x73796850`. In little-endian ASCII, `0x73796850` is `"Phys"` (`50 68 79 73`)!
   - `ECX = 0x039FD960` was actually the `PhysicsMover` object whose header begins with the `"Phys"` type descriptor at `+0x0C`.
   - In `VehicleHealthManager_OnDelegateInput` (`0x005ABBF0`), the code erroneously contained:
     ```cpp
     if (*(uintptr_t*)(car + 0x2C) != (uintptr_t)mover)
     {
         *(uintptr_t*)(car + 0x2C) = (uintptr_t)mover; // STOMPED RIGIDBODY POINTER
     }
     ```
   - **`Car` Structure Memory Map**:
     - `Car + 0x00` $\to$ `SimVehicle` base class.
     - `Car + 0x2C` $\to$ **`mRigidBody*`** (used by collision routines, physics solvers, and `Player::UpdateGameState`).
     - `Car + 0x30` $\to$ **`CarDriver*`** (`driver + 0x208` steering, `+0x20C` throttle, `+0x210` brake, `+0x214` handbrake).
     - `Car + 0x34` $\to$ **`PhysicsMover*`** (verified in `Car::InitializeMoverFromState` at `0x005EBC00`).
   - During the intro/countdown sequence, player input is locked. The millisecond the intro ended and controls switched on, `DelegateDriverInput` ran, stomped `Car + 0x2C` to `mover`, and the very next call to `Player::UpdateGameState` crashed trying to dereference `"Phys" + 0x0C` (`0x7379685C`).

---

### Solutions Implemented ([`VehicleHealthManager.cpp`](file:///mnt/D2/AI/VC/NFSUG2-CodeRestrationTest/src-vehicle-health/VehicleHealthManager.cpp))

1. **Eliminated `Car + 0x2C` Overwrite**:
   - Completely deleted the `*(uintptr_t*)(car + 0x2C) = (uintptr_t)mover;` assignment from `VehicleHealthManager_OnDelegateInput`.
   - `VehicleHealthManager_OnDelegateInput` now strictly checks if the car is marked dead (`IsCarDead`) and only then triggers `ImmobilizeCar`, leaving native engine pointers completely unaltered.

2. **Corrected Drivetrain & RigidBody Offsets in `ImmobilizeCar`**:
   - `mover = *(uintptr_t*)(car + 0x34);` (resolves `PhysicsMover` at `Car + 0x34`).
   - `rigidBody = *(uintptr_t*)(car + 0x2C);` (directly accesses the car's native `mRigidBody*` without secondary pointer chasing).

3. **Clarified Traffic Active Flag**:
   - Renamed temporary variable in `IsCarActive` to `rigidBody` at `Car + 0x2C` and confirmed `rigidBody + 0x77D` matches `TrafficTeleporter::MakeAllTrafficCarsDisappear` (`0x004099D0`).

4. **Verification**:
   - Successfully compiled both plugins via `make all` and verified that launching races, transitioning from intro flyby to active racing, and taking collision damage works with zero crashes.



