#!/system/bin/sh
MODDIR=${0%/*}
MODULE_ID=$(basename "$MODDIR")

MAGISK_VER_CODE=$(magisk -V)
if [ "$MAGISK_VER_CODE" -ge 21000 ]; then
  MAGISK_PATH="$(magisk --path)/.magisk/modules/$MODULE_ID"
else
  MAGISK_PATH=/sbin/.magisk/modules/$MODULE_ID
fi

log -p i -t "FontLoader" "Magisk version $MAGISK_VER_CODE"
log -p i -t "FontLoader" "Magisk module path $MAGISK_PATH"
