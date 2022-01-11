if [ "$ARCH" = "arm64" ]; then
  ARCH_NAME="arm64-v8a"
  ARCH_NAME_SECONDARY="armeabi-v7a"
  ARCH_DIR="lib64"
  ARCH_DIR_SECONDARY="lib"
elif [ "$ARCH" = "arm" ]; then
  ARCH_NAME="armeabi-v7a"
  ARCH_DIR="lib"
elif [ "$ARCH" = "x64" ]; then
  ARCH_NAME="x86_64"
  ARCH_NAME_SECONDARY="x86"
  ARCH_DIR="lib64"
  ARCH_DIR_SECONDARY="lib"
elif [ "$ARCH" = "x86" ]; then
  ARCH_NAME="x86"
  ARCH_DIR="lib"
fi

enforce_install_from_magisk_app() {
  if [ ! "$BOOTMODE" ]; then
    ui_print "*********************************************************"
    ui_print "! Install from recovery is NOT supported"
    ui_print "! Some recovery has broken implementations, install with such recovery will finally cause module not working"
    ui_print "! Please install from Magisk app"
    abort "*********************************************************"
  fi
}

check_arch() {
  if [ -z $ARCH_NAME ]; then
    abort "! Unsupported platform: $ARCH"
  else
    ui_print "- Device platform: $ARCH"
  fi
}

check_android_version() {
  if [ "$API" -ge 31 ]; then
    ui_print "- Android SDK version: $API"
  else
    ui_print "*********************************************************"
    ui_print "! Requires Android 12 (API 31) or above"
    abort "*********************************************************"
  fi
}

check_magisk_version() {
  ui_print "- Magisk version: $MAGISK_VER ($MAGISK_VER_CODE)"
  ui_print "- Installing Font Loader"

  if [ "$MAGISK_VER_CODE" -lt 23014 ]; then
    ui_print "*********************************************************"
    ui_print "! Zygisk requires Magisk 23014+"
    abort "*********************************************************"
  fi
}
