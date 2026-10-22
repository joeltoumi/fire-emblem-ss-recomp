#!/bin/sh
# Packs an SDL build of the game into an AppImage (Linux, Steam Deck).
#   packaging/linux/build-appimage.sh BUILD_DIR OUTPUT.AppImage
# BUILD_DIR must hold `fe8` built with -DSDL_STATIC=ON -DSDL_SHARED=OFF, so
# the only libraries it needs are the system's (SDL loads X11/Wayland, the
# audio server and the gamepad libraries itself). appimagetool must be in
# PATH or in $APPIMAGETOOL. No game data goes in: the player brings the ROM.
set -eu
BUILD=$1
OUT=$2
SRC="$(cd "$(dirname "$0")" && pwd)"
APPDIR="$BUILD/AppDir"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/192x192/apps"
cp "$BUILD/fe8" "$APPDIR/usr/bin/fe8"
strip "$APPDIR/usr/bin/fe8" || true
cp "$SRC/AppRun" "$APPDIR/AppRun"
cp "$SRC/fire-emblem-ss-recomp.desktop" "$APPDIR/"
cp "$SRC/fire-emblem-ss-recomp.desktop" "$APPDIR/usr/share/applications/"
cp "$SRC/fire-emblem-ss-recomp.png" "$APPDIR/"
cp "$SRC/fire-emblem-ss-recomp.png" "$APPDIR/usr/share/icons/hicolor/192x192/apps/"

ARCH=x86_64 "${APPIMAGETOOL:-appimagetool}" "$APPDIR" "$OUT"
