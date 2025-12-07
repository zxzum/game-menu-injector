#!/usr/bin/env bash
set -euo pipefail

ADB=${ADB:-adb}
BINARY_PATH=${1:-"libs/arm64-v8a/game-menu-injector"}
TARGET_NAME=$(basename "$BINARY_PATH")
REMOTE_TMP=/data/local/tmp/$TARGET_NAME
SDCARD_DEST=/sdcard/Download/$TARGET_NAME

if [[ ! -f "$BINARY_PATH" ]]; then
  echo "Binary not found at $BINARY_PATH. Build with ndk-build first." >&2
  exit 1
fi

echo "[*] Pushing $BINARY_PATH to device..."
$ADB push "$BINARY_PATH" "$SDCARD_DEST"

echo "[*] Escalating and preparing inside shell..."
$ADB shell <<SH
su -c "\
  set -e; \
  echo '[*] Stopping previous instance (if any)...'; \
  pkill -f $TARGET_NAME 2>/dev/null || true; \
  killall -q $TARGET_NAME 2>/dev/null || true; \
  rm -f $REMOTE_TMP 2>/dev/null || true; \
  echo '[*] Copying binary into /data/local/tmp'; \
  cp $SDCARD_DEST $REMOTE_TMP; \
  chmod 755 $REMOTE_TMP; \
  echo '[*] Launching injector'; \
  nohup $REMOTE_TMP >/dev/null 2>&1 &"
SH

echo "[*] Injector started from $REMOTE_TMP. Send SIGUSR1 to toggle menu:"
$ADB shell "su -c 'kill -USR1 \$(pidof $TARGET_NAME)'" || true

