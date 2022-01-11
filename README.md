# FontLoader

Modifying fonts is a common scenario using the Magisk module. For example, fonts for CJK languages in the Android system only have one font-weight, users can use the Magisk module to modify font files to add the remaining font weights.

However, starting from Android 12, fonts are loaded only when the app needs to render the font. Before, fonts are preloaded in the zygote process. When users revert the change of Magisk (with MagiskHide before or DenyList nowadays), apps will not be able to access font files from modules and finally result in a crash.

This module is a Zygisk module that is designed to solve the problem. The principle is simple, preload the font when the app has not yet lost access to the font.

## Usage

1. Install FontLoader module in Magisk app
2. Install hiding modules like Shamiko rather than use DenyList
3. Disable DenyList (since modules will not load for apps in the list)

To be clear, DenyList is NOT for hide purposes. This is as topjohnwu, the author of Magisk, said. And using DenyList for hiding is not enough.

Shamiko is a close-sourced hide module created by other people. Currently, it has not yet been widely released, you can download Shamiko from https://t.me/magiskalpha/388.

## Something else

* Why not using the font upgrade feature from Android 12?

  The font upgrade feature cannot set the default font-weight and language. Therefore it can only be used to upgrade/add a font with a single font-weight like emoji font. This is exactly how it is used in the documentation.

  Also, the font upgrade feature requires signing font files. It is impossible to add our key without modifying Magisk.
