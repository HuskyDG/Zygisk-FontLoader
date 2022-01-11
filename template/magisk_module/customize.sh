SKIPUNZIP=1

# Extract verify.sh
ui_print "- Extracting verify.sh"
unzip -o "$ZIPFILE" 'verify.sh' -d "$TMPDIR" >&2
if [ ! -f "$TMPDIR/verify.sh" ]; then
  ui_print "*********************************************************"
  ui_print "! Unable to extract verify.sh!"
  ui_print "! This zip may be corrupted, please try downloading again"
  abort "*********************************************************"
fi
. $TMPDIR/verify.sh

# Extract util_functions.sh
ui_print "- Extracting util_functions.sh"
extract "$ZIPFILE" 'util_functions.sh' "$TMPDIR"
. $TMPDIR/util_functions.sh

#########################################################

enforce_install_from_magisk_app
check_magisk_version
check_android_version
check_arch

# Extract libs
ui_print "- Extracting module files"

extract "$ZIPFILE" 'module.prop' "$MODPATH"
extract "$ZIPFILE" 'post-fs-data.sh' "$MODPATH"

mkdir "$MODPATH/zygisk"

extract "$ZIPFILE" "lib/$ARCH_NAME/libfontloader.so" "$MODPATH/zygisk" true
mv "$MODPATH/zygisk/libfontloader.so" "$MODPATH/zygisk/$ARCH_NAME.so"

if [ "$IS64BIT" = true ]; then
  extract "$ZIPFILE" "lib/$ARCH_NAME_SECONDARY/libfontloader.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/libfontloader.so" "$MODPATH/zygisk/$ARCH_NAME_SECONDARY.so"
fi

set_perm_recursive "$MODPATH" 0 0 0755 0644
