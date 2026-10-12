# Minecraft Pocket Edition (v0.6.1) Source Code

This repository contains the original source code for **Minecraft: Pocket Edition v0.6.1**, supporting both Android and iOS platforms.

## Meta Quest (standalone VR APK)

The `quest` port builds a standalone arm64 APK for Meta Quest headsets. It renders in stereo through OpenXR and uses the controllers for input. It lives in `handheld/project/quest`; the game sources, assets and resources are shared with the Android project in `handheld/project/android`.

### Build

Requirements: JDK 17, Android SDK with `platforms;android-32`, `build-tools;34.0.0`, `ndk;26.1.10909125` and `cmake;3.22.1`, and network access (the OpenXR loader is fetched from GitHub at configure time).

```bash
cd handheld/project/quest
export ANDROID_HOME=/path/to/android-sdk
./gradlew :app:assembleDebug
```

The APK is written to `app/build/outputs/apk/debug/app-debug.apk`. A CI workflow (`.github/workflows/build-quest.yml`) builds it on every push and uploads it as an artifact.

### Install

Enable developer mode on the headset, then:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

The app appears in the Unknown Sources section of the library and launches as a VR app.

### Controls

| Input | Action |
| --- | --- |
| Left stick | Move (click to sneak) |
| Right stick left/right | Turn |
| Right stick up/down (flick) | Previous / next hotbar slot |
| Head | Look; the crosshair and block targeting follow the head |
| Right trigger (hold) | Break blocks and attack |
| Right grip | Place or use the held item (repeats while held) |
| Right A | Jump (flying: go up) |
| Right B | Sneak (flying: go down) |
| Left X | 2x2 crafting |
| Left menu | Pause, or close the open screen |
| Right controller pointer + trigger | Click in menus, inventory and options |

The HUD and every menu are drawn onto a head-locked panel. Use the right controller ray to click on it.

### How it works

- `src/platform/vr`: OpenXR session (instance, EGL context, per-eye swapchains, controller actions), per-eye render targets, the GUI panel and laser, and the controller input holder.
- `src/platform/gles1_shim.cpp`: the renderer's GLES 1.1 fixed-function calls are implemented on GLES 3, because OpenXR only provides GLES 3 contexts. `tests/gles1_shim/run.sh` renders one scene with desktop GL 2.1 and with the shim and compares the pixels. Run it with Mesa installed.
- `MCPE_VR` is defined only by the Quest build. The iOS, Windows and Raspberry Pi builds never reference the VR code.

### Status and limitations

- The port builds for arm64 and the native library links against OpenXR and GLES 3. The shim matches a reference renderer on a test scene.
- It has **not** been run on a headset in this repository's development environment. Rendering speed, comfort and input feel need to be checked on hardware.
- Block targeting uses the head direction, not the controller.
- Touch-only controls (the on-screen joystick and buttons) are replaced by the controllers.
- Multiplayer (RakNet) is still untested on this platform.
- Android system dialogs (world name, seed, text entry) appear as 2D panels.

---

## Original iOS and Android source

Right now you have to compile this yourself, but I'll supply a precompiled .ipa to put on you iPhone soon! (not everyone has Macs!)

> [!WARNING]
> The `main` branch is preserved for historical purposes and contains legacy 32-bit code (armv7) that is **not compatible** with modern 64-bit iOS devices or recent versions of Xcode. You could try to compile it with an old version of Xcode, but that's untested an I'm not sure if that works

## Modern iDevice Support (arm64)

To build and run the game, you have to use the `arm64-build` branch. This branch has been modernized to comply with the new C++ standards as they have changed since 2011. Also includes fixes for modern A-series chips to actually get the game running.

* **Architecture:** Migrated from `armv7` to `arm64`.
* **Legacy Patching:** Fixed header file extensions (e.g., `.height` back to `.h`) and resolved ambiguous function redefinitions.

---

## Cloning & Compiling Instructions

Follow these steps to get the game running on your own physical iOS device using Xcode. You will need a Mac. If you don't have a Mac, you could try using a macOS virtual machine. I'd reccomend using Ventura or newer.

### 1. Clone the arm64 Branch

Open your terminal and clone the repository, ensuring you switch to the `arm64-build` branch:

```bash
git clone https://github.com/deepfriedwaffles/minecraft-pe-source-code.git
cd minecraft-pe-source-code
git checkout arm64-build

```

### 2. Open in Xcode

Locate the project file in `handheld/project/iosproj/minecraftpe.xcodeproj` and open it.

### 3. Configure Code Signing

Before you can deploy to an iPhone, you must sign the app with your own Apple Developer account:

1. Select the **minecraftpe** project in the left sidebar.
2. Go to **Signing & Capabilities**.
3. Change the **Bundle Identifier** to something unique (e.g., `com.yourname.mcpe`).
4. Select your **Team** from the dropdown menu.

### 4. Build and Run

1. Connect your iPhone via USB or LAN.
2. Select your device from the run destination menu at the top of Xcode.
3. Press **Cmd + R** (or the Play button).
4. **Note:** If you encounter a "Developer Mode" or "Untrusted Developer" error on your phone, go to **Settings > General > VPN & Device Management** to trust your certificate.

---

## Known Issues

* **Display:** The game renders at a fixed 480x320 resolution and does not yet support the big iPhone displays.
* **UI:** The Options menu is absolutely broken.
* **Multiplayer:** Local/Online multiplayer is currently untested on `arm64` and may require further RakNet patching.

---
