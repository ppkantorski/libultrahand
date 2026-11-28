# libultra

![Ultrahand Logo](https://github.com/ppkantorski/Ultrahand-Overlay/blob/main/.pics/ultrahand.png)

---

## Overview

`libultra` is the utility library behind the [Ultrahand Overlay](https://github.com/ppkantorski/Ultrahand-Overlay) ecosystem. Its C++ headers cover file I/O, strings, INI and JSON files, hex editing, downloads, mod conversion, audio, haptics, the frosted glass, wallpapers and system utilities, all for Nintendo Switch overlay development with libnx.

Include everything at once with the central header:

```cpp
#include <ultra.hpp>
```

---

## Headers

### `ultra.hpp`
Central include that pulls in all libultra headers at once. Supports the `ULTRA_TARGETED_SPEED` and `ULTRA_TARGETED_SIZE` build directives for per-component optimization.

---

### `global_vars.hpp`
Global constants, shared paths and atomic state used across the library.

- Paths: `SETTINGS_PATH`, `OVERLAY_PATH`, `PACKAGE_PATH`, `DOWNLOADS_PATH`, `SOUNDS_PATH`, `NOTIFICATIONS_PATH`, etc.
- Shared progress counters: `downloadPercentage`, `unzipPercentage`, `copyPercentage`, `displayPercentage`
- UI symbols: `CHECKMARK_SYMBOL`, `CROSSMARK_SYMBOL`, `DOWNLOAD_SYMBOL`, `STAR_SYMBOL`, `THROBBER_SYMBOLS`, etc.
- Common strings for modes, toggle states, UI labels and config keys

---

### `debug_funcs.hpp`
Thread-safe, timestamped logging to a file.

```cpp
ult::logMessage("something happened");
ult::logMessage(someStdString);
```

- Log file path set with `ult::logFilePath`
- Logging can be turned off at runtime with `ult::disableLogging`
- Built in with `-DUSING_LOGGING_DIRECTIVE=1`

---

### `string_funcs.hpp`
String utilities used throughout the library.

- `trim`, `trimNewline`, `removeWhiteSpaces`, `removeQuotes`
- `preprocessPath`, `preprocessUrl`, `resolveDirectoryTraversal`
- `startsWith`, `splitString`, `splitStringAtIndex`, `sliceString`
- `stringToLowercase`, `stringToUppercase`, `formatPriorityString`
- `cleanVersionLabel`, `extractTitle`, `removeTag`, `getFirstLongEntry`
- `customAlign`, `returnOrNull`, `isValidNumber`
- `StringStream`: a small `std::istringstream` replacement with hex mode

---

### `get_funcs.hpp`
File system queries and path helpers.

- `getNameFromPath`, `getFileName`, `getParentDirFromPath`, `getParentDirNameFromPath`
- `getDestinationPath`, `getValueFromLine`
- `getSubdirectories`, `getFilesListFromDirectory`
- `getFilesListByWildcards`: lists files and folders matching a wildcard, with an optional `maxLines` cap

---

### `path_funcs.hpp`
File and folder operations.

- Checks: `isFile`, `isDirectory`, `isFileOrDirectory`, `isDirectoryEmpty`
- Creating: `createDirectory`, `createSingleDirectory`, `createTextFile`
- Deleting: `deleteFileOrDirectory`, `deleteFileOrDirectoryByPattern`
- Copying: `copySingleFile`, `copyFileOrDirectory`, `copyFileOrDirectoryByPattern`
- Moving: `moveFile`, `moveDirectory`, `moveFileOrDirectory`, `moveFilesOrDirectoriesByPattern`
- `mirrorFiles`: mirrors a source folder's deletion structure onto a target path
- `createFlagFiles`: creates `.txt` flag files from wildcard-matched names
- `dotCleanDirectory`: removes macOS `._` files recursively
- Stopping an operation: `abortFileOp`, with progress in `copyPercentage`

---

### `list_funcs.hpp`
List and set operations, mostly backed by files.

- `readListFromFile`, `readSetFromFile`, `writeSetToFile`
- `getEntryFromListFile`: reads one entry of a file-backed list by index
- `stringToList`, `splitIniList`, `joinIniList`
- `removeEntryFromList`, `filterItemsList`
- `compareFilesLists`, `compareWildcardFilesLists`: compare two file lists and write the duplicates to a file

---

### `ini_funcs.hpp`
INI file reading and writing, with section and key management.

- `getParsedDataFromIniFile`: returns a `map<section, map<key, value>>`
- `parseIni`, `parseSectionsFromIni`, `parseValueFromIniSection`, `getKeyValuePairsFromSection`
- `setIniFileValue`, `setIniFileKey`, `setIniFile`
- `addIniSection`, `renameIniSection`, `removeIniSection`, `removeIniKey`
- `cleanIniFormatting`: tidies section spacing and removes blank lines
- `saveIniFileData`: writes a whole parsed structure back to disk
- `addKeyToMatchingSections`, `removeKeyFromMatchingSections`: bulk edits by pattern
- `loadOptionsFromIni`, `loadSpecificSectionFromIni`: load commands for Ultrahand packages
- `parseCommandLine`: splits a command line into words, with quoted strings
- `getPackageHeaderFromIni`: reads a package's title, version, creator, about, credits and display options
- `syncIniValue`: keeps a value in step between a map in memory and a file

---

### `json_funcs.hpp`
JSON reading and writing, built on `cJSON`.

- `readJsonFromFile`, `stringToJson`
- `getStringFromJson`, `getStringFromJsonFile`
- `setJsonValue`: creates the file if it doesn't exist
- `renameJsonKey`
- `pushNotificationJson`: writes a notification to Ultrahand's notifications folder
- `json_t` with `JsonDeleter`, so pointers are freed automatically

---

### `hex_funcs.hpp`
Binary file editing and hex helpers.

- `asciiToHex`, `decimalToHex`, `hexToDecimal`
- `hexToReversedHex`, `decimalToReversedHex`: byte-order reversal
- `findHexDataOffsets`: finds every offset of a hex pattern in a file, including matches that cross a read boundary
- `hexEditByOffset`: overwrites bytes at a file offset
- `hexEditByCustomOffset`: edits at an offset relative to an ASCII anchor pattern
- `hexEditFindReplace`: find and replace, optionally only one occurrence
- `parseHexDataAtCustomOffset`: reads hex data at an anchored offset
- `replaceHexPlaceholder`: resolves `{hex_file:...}` placeholders
- `extractVersionFromBinary`: finds a version string in a binary
- `decodeBase64ToString`
- Lookup tables built at compile time (`hexTable`, `hexLookup`) and a `hexSumCache` for repeated searches of the same file

---

### `download_funcs.hpp`
File downloads and ZIP extraction with `libcurl` and `minizip`.

```cpp
ult::downloadFile(url, destination);
ult::unzipFile(zipPath, extractTo);
```

- Sockets are opened and closed per download to save memory (skip it with `noSocketInit`)
- Progress in `downloadPercentage` and `unzipPercentage`
- Stopping: `abortDownload`, `abortUnzip`
- Buffer sizes: `DOWNLOAD_READ_BUFFER`, `DOWNLOAD_WRITE_BUFFER`, `UNZIP_READ_BUFFER`, `UNZIP_WRITE_BUFFER`
- `syncNtp`: sets the system clock from an NTP server (Ultrahand does this once before the first download, so SSL handshakes don't fail on a wrong clock)

---

### `mod_funcs.hpp`
Game mod format conversion.

- `pchtxt2ips`: converts a `.pchtxt` patch to an IPS32 binary (with the `EEOF` footer)
- `pchtxt2cheat`: converts a `.pchtxt` file to an Atmosphere cheat file, with `@disabled` support
- `appendCheatToFile`, `cheatExists`, `extractCheatName`
- `isValidTitleID`, `findTitleID`: title ID helpers
- `toBigEndian`: `uint32_t` / `uint16_t` byte swaps

---

### `audio.hpp`
WAV playback with libnx `audout`.

```cpp
ult::Audio::initialize();
ult::Audio::playNavigateSound();
ult::Audio::setMasterVolume(0.8f);
```

- 16-bit PCM WAV files from 8 to 48 kHz, mono or stereo
- Eleven sound types: `Navigate`, `Enter`, `Exit`, `Wall`, `On`, `Off`, `Settings`, `Move`, `Notification`, `MainSettings` and `Hold`
    - `MainSettings` plays when Ultrahand's own settings open, and `Hold` on held list and trackbar steps. Both are optional: sound packs without them play `settings.wav` / `tick.wav` instead
- Sounds are loaded from `sdmc:/config/ultrahand/sounds/`
- `playSounds(types, count)`: mixes up to 8 sounds into one submission
- Mixing and resampling share one buffer, so playing a sound allocates nothing
- Master volume from 0.0 to 1.0
- `reloadIfDockedChanged`: reloads the sounds when the docked state changes
- `unloadAllSounds`, optionally keeping some

---

### `haptics.hpp`
Rumble feedback.

```cpp
ult::initHaptics();
ult::rumbleClickStandalone();       // single click
ult::rumbleDoubleClickStandalone(); // double click
ult::rumbleHoldBeat(50);            // hold-to-run beat at 50% progress (-1 stops it)
```

- Meant to run on a background thread
- The click functions block until the pattern is done. `rumbleHoldBeat` returns at once and the haptics thread times the beats
- `checkAndReinitHaptics`: sets the vibration devices up again if they become unavailable
- Whether buttons and touch rumble is set by `ult::useButtonHaptics` and `ult::useTouchHaptics` (`tsl_utils.hpp`)

---

### `glass.hpp`
The frosted glass: a blurred, tinted copy of whatever is behind the overlay, drawn as its background. `tesla.hpp` uses it directly, so most overlays only need the settings below.

- Settings (in `tsl_utils.hpp`, read from Ultrahand's `config.ini` unless an overlay sets its own): `useFrostedGlass`, `glassBlurRadius`, `glassDarkness`, `glassRed` / `glassGreen` / `glassBlue`, `glassWithCheats` and `useGlassLayer` (full color above 8 MB)
- How it gets its picture:
    - Live: in games it reads each frame straight from the game, read-only. On the HOME menu and in applets it reads the frames the system is showing.
    - When a game has cheats loaded, Atmosphere's cheat engine is already attached to it, so the glass copies the picture from the screen with the VIC instead (`nx-ovlloader v2.0.4` or newer), or reads the game through the cheat engine with an older `nx-ovlloader`
- A background worker thread does the reading and blurring. The renderer draws the latest blurred strip each frame
- Controls for the overlay loop: `newSession`, `dropFrames`, `refreshFrames`, `wakeForShow`, `hasFrame`, `unavailable`, `shutdown`, `stopForPowerOff`
- `sharedAlloc` / `sharedFree`: the glass and the wallpaper share one block of memory, so switching between them doesn't fragment the heap
- Panels: `regionKey`, `geoOf` and `regionFits` describe a glass area of any size and place (with `USING_GLASS_PANEL_DIRECTIVE`, see the [libultrahand README](../README.md))
- Requires 6 MB of overlay memory or more

---

### `animated.hpp`
Animated wallpapers (`.uaw`).

```cpp
ult::anim::startFromPath(path);   // start playing a .uaw into wallpaperData
ult::anim::saveState();           // remember the picture and position (when the overlay closes)
ult::anim::stop();
```

- The animated wallpaper in use is named in `wallpaper.ini` (`[wallpaper] animated=<path>`)
- Frames are read ahead on a small thread into a ring buffer and decoded as they're shown (`frame()`)
- `saveState` / `clearState`: the picture on screen and the playback position are kept in `cache/animated.cache`, so the next overlay continues where the last one stopped
- `s_ringCapLimit`: a smaller read-ahead for an overlay with less memory to spare
- An overlay's own `wallpaper.uaw` in its UI override folder keeps its resume point in that folder's `cache/`

---

### `image_fx.hpp`
The wallpaper `Light` and `Color` adjustments (`wallpaper.ini`, -100 to 100 each), done with NEON.

- `WallAdj` / `wallAdjSetup`: turns the slider values into a ready-to-use set of factors. Stages with all sliders at 0 are skipped
- Applied once when a still wallpaper loads, and to each block's palette as an animated wallpaper is decoded, so drawing a frame costs nothing extra
- While the sliders are being moved, the renderer applies them live instead so the preview follows

---

### `lang_table.hpp`
Every translated UI string as `LANG(VARIABLE, "English default")`. The JSON key in a language file is the variable name.

- Included twice by `tsl_utils.cpp` (once for the strings, once for the variables), so it has no include guard and must not be included anywhere else
- Adding a string: add one `LANG(...)` line, and the variable and its default exist everywhere

---

### `tsl_utils.hpp`
System utilities shared by libultra and the libtesla rendering layer.

- Framebuffer size: `DefaultFramebufferWidth`, `DefaultFramebufferHeight`, `correctFrameSize`
- Translations: `loadTranslationsFromJSON`, `clearTranslationCache`, `applyLangReplacements`
- Console state: `consoleIsDocked`, `getBuildIdAsString`, `getTitleIdAsString`
- Wallpapers:
    - Loading: `loadWallpaperFile`, `loadRGBA8888toRGBA4444`, `reloadWallpaper`
    - `wallpaperData` is a `WallpaperBuffer` (`data()`, `size()`, `empty()`, `clear()`, `resize()`); `fullColorWallpaper` tells whether it holds 8-bit RGB
    - PNG: `convertPngToRgba`, `setWallpaperFromPng`
    - Adjustments: `wallpaperIniPath`, `wallpaperAdjust`, `wallpaperAlpha`
- Overlay memory: `getCurrentHeapSize`, `setOverlayHeapSize`, `requestOverlayExit`
- Battery and temperature (widget): `powerGetDetails`, `ReadSocTemperature`, `ReadPcbTemperature`
- Input key aliases (`KEY_A` to `KEY_RIGHT`) and touch input macros
- Feature flags: `useSoundEffects`, `useButtonHaptics`, `useTouchHaptics`, `useNotifications`, `useLaunchRecall`, `useAutoNTPSync`, the frosted glass settings, etc.
- `initializeThemeVars`, `initializeUltrahandSettings`, `reinitializeLangVars`

---

### `exception_wrap.hpp`
Optional linker-level exception interception that makes builds about 20 KB smaller.

Turned on with `USE_EXCEPTION_WRAP=1`. Needs the matching `--wrap` linker flags and `-fno-exceptions`. See the main [libultrahand README](../README.md) for the full setup.

---

## Usage

Add `lib/libultrahand` to your project and include the central header:

```cpp
#include <ultra.hpp>   // all libultra utilities
#include <tesla.hpp>   // libtesla UI layer (includes ultra.hpp too)
```

See the [libultrahand README](../README.md) for the build setup, required libraries, makefile directives and the Ultrahand signature step.

---

## License

Licensed under [GPLv2](LICENSE) and [CC-BY-4.0](SUB_LICENSE).

Copyright © 2023-2026 ppkantorski
