#!/bin/bash
# Run by the android-launch workflow inside the emulator runner.
set -u
pkg=dev.fe8.android.eu
apk=$(ls android/app/build/outputs/apk/eu/release/*.apk | head -1)
adb install -r "$apk"
# Let the system finish registering the package: a launch right after the
# install can be killed by the package-change handling.
sleep 15
adb logcat -c
adb shell am start -W -n $pkg/dev.fe8.android.Fe8Activity
sleep 20
pid=$(adb shell pidof $pkg | tr -d '\r')
echo "pid after 20 s: '${pid}'"
adb logcat -d > logcat.txt
grep -E "fe8|SDL|linker|DEBUG|AndroidRuntime|libc|$pkg" logcat.txt | tail -150
if [ -z "$pid" ] || grep -q "Finished main function" logcat.txt; then
    echo "::error title=Android launch::the game is not running 20 s after launch"
    grep -E " (E|F) (fe8|SDL|DEBUG|libc|linker) " logcat.txt | tail -20
    exit 1
fi
echo "the app is running"
