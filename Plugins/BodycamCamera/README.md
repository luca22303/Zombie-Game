# BodycamCamera (UE5 plugin)

A chest-mounted **bodycam first-person camera**, built to be dropped into a UE5 C++ project.

| Layer | What it does |
|---|---|
| **Rotation lag** | The view chases the player's aim through an under-damped quaternion spring: it trails fast turns, overshoots slightly and settles. |
| **Locomotion bob** | Stride phase advances with distance travelled (not time), so the bob matches the footsteps and stops when you stop. Separate walk / sprint / crouch profiles. |
| **Physical impulses** | Every footstep, jump, landing, hit and shot kicks a position and a rotation spring. Acceleration (start/stop/strafe) pushes the camera the opposite way. |
| **Idle life** | Breathing and slow hand-held drift. |
| **Lens look** | FOV, vignette, grain, chromatic aberration, lens dirt, slow auto-exposure (built in) + fisheye, rolling shutter, compression artifacts and sensor noise (one post-process material). |
| **OSD** | Timestamp, blinking REC, battery, device ID (UMG base class). |
| **Viewmodel sway** | Arms/weapon float and lag inside the frame. |

> **Status:** written without access to the Unreal Editor, so it has **not been compiled or run yet**.
> Expect to fix a few small compile issues on first build (most likely engine-version API differences).
> Target: UE 5.3+ (uses `TObjectPtr`, double-precision `FVector`). Tell me the error output and I'll fix it.

## 1. Add it to your project

1. Copy this folder to `<YourProject>/Plugins/BodycamCamera/`.
2. Right-click your `.uproject` → *Generate Visual Studio project files* (or the platform equivalent), then build.
3. Open the editor, *Edit > Plugins*, confirm **Bodycam Camera** is enabled (it is by default).
4. If your game module has its own `Build.cs`, add `"BodycamCamera"` to `PublicDependencyModuleNames` **only if** you reference its C++ classes from your own code. Blueprint-only use needs nothing.

## 2. Create the lens material (once)

Either run the script or build it by hand.

**Script (fastest):** enable *Python Editor Script Plugin*, restart, then in the Output Log (Python mode):
```
py "<YourProject>/Plugins/BodycamCamera/Scripts/CreateBodycamMaterial.py"
```
This creates `/Game/Bodycam/M_Bodycam_Lens`. The script is untested against your engine version; if it errors, do it by hand:

**By hand:**
1. New Material `M_Bodycam_Lens`. Details: *Material Domain = Post Process*, *Blendable Location = After Tonemapping*.
2. Add a **Custom** node, *Output Type = CMOT Float3*, paste the contents of `Docs/HLSL/BodycamLens.hlsl` into *Code*.
3. Add 9 inputs named exactly: `Distortion`, `RollingShutter`, `YawRate`, `Compression`, `SensorNoise`, `Aspect`, `ViewWidth`, `ViewHeight`, `ClockTime`.
4. Feed them with **Scalar Parameters** of the same names (defaults: 0.35, 0, 0, 0, 0, 1.7778, 1920, 1080) and a **Time** node into `ClockTime`.
5. Connect the Custom node output to **Emissive Color**.

If the shader fails to compile, the two engine helpers `GetViewportUV` / `ViewportUVToSceneTextureUV` may be named differently in your engine version. Replace them with `GetDefaultSceneTextureUV(Parameters, 14)` (and sample with `SceneTextureLookup(uv, 14, false)` directly).

## 3. Set up the camera in your character

1. Content Browser → *Add > Miscellaneous > Data Asset* → class **BodycamProfile** → e.g. `DA_Bodycam_Default`. Assign `M_Bodycam_Lens` to *Lens > Lens Material*.
2. In your character Blueprint (or C++): **remove/disable the default first-person camera** and add a **Bodycam Camera Component**.
   - Attach it to the **Capsule** (needed for smooth crouching), location about `X=15, Z=40` relative to the capsule centre for a chest mount (≈130 cm above the feet with the default capsule).
   - Set *Profile* to `DA_Bodycam_Default`. Make sure *Auto Activate* is on and it is the only active camera.
3. On the character: *Use Controller Rotation Yaw = true* (as in any FPS). **Do not** tick *Use Pawn Control Rotation* on the Bodycam component (the component manages rotation itself).
4. (Optional) OSD: create a Widget Blueprint with parent **BodycamOSDWidget**, add text blocks named `TimestampText`, `RecText`, `BatteryText`, `DeviceText` (and any widget named `RecDot`), then set the component's *OSD Widget Class*.
5. (Optional) Arms/weapon: attach the mesh to the Bodycam component, add **Bodycam Viewmodel Sway Component** to the character, set *Target Component Name* to the mesh's name.

### C++ example

```cpp
// MyCharacter.h
UPROPERTY(VisibleAnywhere) TObjectPtr<UBodycamCameraComponent> Bodycam;

// MyCharacter.cpp
Bodycam = CreateDefaultSubobject<UBodycamCameraComponent>(TEXT("Bodycam"));
Bodycam->SetupAttachment(GetCapsuleComponent());
Bodycam->SetRelativeLocation(FVector(15.f, 0.f, 40.f));
```

## 4. Hook up gameplay

| Event | Call |
|---|---|
| Fire weapon | `Bodycam->AddRecoil(PitchKickDegPerSec, YawKick, PushBackCmPerSec)` |
| Take damage / zombie grab | `Bodycam->AddHitReaction(HitDirectionWorld, Strength)` |
| Explosion, custom bump | `Bodycam->AddCameraImpulse(PositionCmPerSec, RotationDegPerSec)` |
| Respawn / teleport | `Bodycam->ResetState()` |
| Aiming down sights | lower `SetMotionIntensity` (e.g. 0.4) |
| Options menu | `SetMotionIntensity`, `SetLensIntensity` (motion-sickness comfort) |

**Shooting and interaction traces must use `GetAimLocation()` / `GetAimRotation()`**, not the camera transform, so shake never affects accuracy.

Console: `bodycam.Debug 1` (on-screen state), `bodycam.MotionScale 0.5` (global motion multiplier).

## 5. Tuning cheat-sheet (all in the `BodycamProfile` asset)

| Want | Change |
|---|---|
| Camera feels glued / too stiff | Lower *Rotation Lag Frequency* (3-4) |
| Too floaty / seasick | Raise *Rotation Lag Frequency* (6-8), raise *Rotation Lag Damping* toward 1, lower *Max Rotation Lag* |
| Footsteps too weak/strong | *Walk/Sprint > Footstep Impulse* |
| Bob too fast | Raise *Stride Length* |
| Camera bounces too long | Raise *Position Damping* / *Rotation Damping* |
| Too wide / cropped | Lower *Lens > Distortion*, raise *Lens > Field Of View* |
| Rolling shutter skews the wrong way | Make *Lens > Rolling Shutter* negative |
| Footage too clean/dirty | *Compression*, *Sensor Noise*, *Film Grain*, *Lens Dirt Mask* |
| Sprint threshold wrong | Set *Sprint Speed Threshold* between your walk and sprint `MaxWalkSpeed` |

## Notes and limits

- Only runs for the **locally controlled pawn**. Other players/AI don't pay for it.
- Crouch smoothing assumes the camera is attached to the capsule and the capsule shrinks on crouch (default `ACharacter` behaviour). Turn off with *Smooth Stance Change* if you drive crouch differently.
- Auto-exposure *min/max* are off by default because their units depend on the project's exposure setup; enable *Override Exposure Range* if you want them.
- Lag/overshoot and the lens effects can cause motion sickness. Ship the intensity sliders.

## 8. Weapon pose: sprint lock, collision, hand grip

Two components, both optional, both written without access to the engine (not yet compiled). They need only the plugin's
`BodycamCameraComponent` and the viewmodel hierarchy. They do not touch the sway, stance or body components.

### 8.1 Scene setup (once)

Insert a plain scene component, the **pivot**, between the camera and the viewmodel:

```
BodycamCamera
  WeaponPivot            <- new SceneComponent, identity transform, name must match TargetComponentName
    ViewmodelRoot        <- (existing) sway target
      ViewmodelArms      <- (existing)
        Weapon           <- (existing)
```

Re-parent `ViewmodelRoot` under `WeaponPivot`. Add **Bodycam Weapon Pose Component** and **Bodycam Hand Grip Component** to the character.

### 8.2 Sprint lock (R6 Siege style low ready)

While ground speed >= *Sprint Speed Threshold* the weapon blends into a low-ready pose and is held in a stable frame
(view yaw, aim pitch, **no camera bob**). The weapon follows the view pitch only while you look at or below *Lock Pitch*
(default -15 degrees). Look higher and the weapon stays at that pitch, so it sinks out of the bottom of the frame: clear
view, no bobbing gun. Looking down brings it back into frame, still steady.

| Want | Change |
|---|---|
| Gun leaves the frame earlier / later | *Lock Pitch* (more negative = later) |
| Softer corner at the lock pitch | *Lock Softness* |
| Different low-ready pose | *Sprint Offset*, *Sprint Rotation* |
| Aim down sights must cancel it | call `SetSprintLockSuppressed(true)` while aiming or firing |
| Block fire / ADS while sprinting | `IsSprintLocked()` / `GetSprintAlpha()` |

### 8.3 Weapon collision

Five sphere sweeps from the eye to the muzzle (centre + four around the muzzle). The deepest penetration pulls the weapon
back, tilts the muzzle up (wall or floor) or down (ceiling) and yaws it away from side walls, through springs. Set
*Weapon Reach* to the real eye-to-muzzle distance: turn on *Debug Draw* and compare the probe tips with the barrel.
`IsWeaponBlocked()` is true when the muzzle is buried deeper than *Blocked Threshold*; use it to refuse firing.
Walls must block *Trace Channel* (Visibility by default).

### 8.4 Hand grip

After the arm animation is evaluated, a two-bone IK pins the **left** hand to *Left Hand > Grip In Weapon Space* (weapon
space, X forward, origin at the top of the pistol grip for the AKM). The first value (32, 0, 2) is a guess: turn on
*Debug Draw* (blue sphere = grip point) and move it until the palm sits under the handguard. *Rotation Weight* 0 keeps the
animated hand orientation, 1 snaps it to the grip transform. The right hand is off by default because the weapon is attached
to the right hand bone and already follows it.

If your engine version does not allow bone edits in `OnBoneTransformsFinalized` (the hands will not move), use
`GetGripWorldTransform(true)` as the effector of a **Two Bone IK** node in `ABP_Rifle_FP` (bone `hand_l`, joint target from
the animation) and set *Alpha* on this component to 0.

## Layout

```
BodycamCamera.uplugin
Source/BodycamCamera/
  Public/  BodycamSpring.h               spring math (header-only)
           BodycamProfile.h              data asset with every tunable value
           BodycamCameraComponent.h      the camera
           BodycamOSDWidget.h            REC / timestamp overlay base class
           BodycamViewmodelSwayComponent.h
  Private/ *.cpp
           BodycamWeaponPoseComponent.h  sprint lock + weapon collision (section 8)
           BodycamHandGripComponent.h    hand IK onto the weapon (section 8)
Docs/HLSL/BodycamLens.hlsl               post-process shader (Custom node body)
Scripts/CreateBodycamMaterial.py         editor script that builds the material
```
