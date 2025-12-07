# Game Menu Injector (test overlay)

This project demonstrates a minimal native injector built for educational modding tests. It discovers a target Android game process, attaches using a KittyMemory bridge placeholder, and renders a lightweight overlay loop with a toggled menu indicator.

## Building with ndk-build

1. Place your Android NDK on `PATH` so `ndk-build` is available.
2. From the project root run:

```sh
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./jni/Android.mk APP_PLATFORM=android-21
```

The executable will be generated at `libs/arm64-v8a/game-menu-injector`.

## Deploying and running

Use the helper script after building:

```sh
./scripts/push_and_run.sh
```

The script:
- Pushes the binary to `/sdcard/Download` via `adb`.
- Enters `adb shell`, switches to `su`, copies the binary into `/data/local/tmp`, grants execution, and launches it in the background.

## Toggling the test menu

The overlay renders a square-style HUD label in the top-left corner of the virtual surface and prints frames to `logcat`. Send `SIGUSR1` to the injector process to toggle the menu state while it is running:

```sh
adb shell "su -c 'kill -USR1 $(pidof game-menu-injector)'"
```

You can also edit `src/main.cpp` to change the package name (`com.example.testgame`) to your target process or adjust the placeholder KittyMemory hook points.

