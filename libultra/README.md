# libultra

![Ultrahand Logo](https://github.com/ppkantorski/Ultrahand-Overlay/blob/main/.pics/ultrahand.png)

---

## Overview

`libultra` is the utility backbone of the [Ultrahand Overlay](https://github.com/ppkantorski/Ultrahand-Overlay) ecosystem. It provides a comprehensive set of C++ headers covering file I/O, string processing, INI/JSON handling, hex editing, downloads, mod conversion, audio, haptics, and system utilities — all designed for Nintendo Switch overlay development with libnx.

Include everything at once via the central header:

```cpp
#include <ultra.hpp>
```

---

## Headers

### `ultra.hpp`
Central include that pulls in all libultra headers in one shot. Supports `ULTRA_TARGETED_SPEED` and `ULTRA_TARGETED_SIZE` build directives for per-component optimization.

---

### `global_vars.hpp`
Global constants, shared path definitions, and atomic state variables used across the library.

- Canonical paths: `SETTINGS_PATH`, `OVERLAY_PATH`, `PACKAGE_PATH`, `DOWNLOADS_PATH`, `SOUNDS_PATH`, `NOTIFICATIONS_PATH`, etc.
- Shared atomic counters: `downloadPercentage`, `unzipPercentage`, `copyPercentage`, `displayPercentage`
- Pre-defined UI symbols: `CHECKMARK_SYMBOL`, `CROSSMARK_SYMBOL`, `DOWNLOAD_SYMBOL`, `STAR_SYMBOL`, `THROBBER_SYMBOLS`, etc.
- Common string constants covering modes, toggle states, UI labels, and config keys

---

### `debug_funcs.hpp`
Thread-safe timestamped logging to a file.

```cpp
ult::logMessage("something happened");
ult::logMessage(someStdString);
```

- Configurable log file path via `ult::logFilePath`
- Logging can be suppressed at runtime with `ult::disableLogging`
- Enabled at build time with `-DUSING_LOGGING_DIRECTIVE=1`

---

### `string_funcs.hpp`
String manipulation utilities used throughout the library.

- `trim`, `trimNewline`, `removeWhiteSpaces`, `removeQuotes`
- `preprocessPath`, `preprocessUrl`, `resolveDirectoryTraversal`
- `startsWith`, `splitString`, `splitStringAtIndex`, `sliceString`
- `stringToLowercase`, `stringToUppercase`, `formatPriorityString`
- `cleanVersionLabel`, `extractTitle`, `removeTag`, `getFirstLongEntry`
- `customAlign`, `returnOrNull`, `isValidNumber`
- Lightweight `StringStream` class — a minimal `std::istringstream` replacement with hex mode support

---

### `get_funcs.hpp`
File system query and path extraction utilities.

- `getNameFromPath`, `getFileName`, `getParentDirFromPath`, `getParentDirNameFromPath`
- `getDestinationPath`, `getValueFromLine`
- `getSubdirectories`, `getFilesListFromDirectory`
- `getFilesListByWildcards` — wildcard-based file/directory enumeration with optional `maxLines` cap

---

### `path_funcs.hpp`
File and directory operations.

- Existence checks: `isFile`, `isDirectory`, `isFileOrDirectory`, `isDirectoryEmpty`
- Creation: `createDirectory`, `createSingleDirectory`, `createTextFile`
- Deletion: `deleteFileOrDirectory`, `deleteFileOrDirectoryByPattern`
- Copy: `copySingleFile`, `copyFileOrDirectory`, `copyFileOrDirectoryByPattern`
- Move: `moveFile`, `moveDirectory`, `moveFileOrDirectory`, `moveFilesOrDirectoriesByPattern`
- `mirrorFiles` — mirrors a source directory's deletion structure onto a target path
- `createFlagFiles` — creates `.txt` flag files from wildcard-matched basenames
- `dotCleanDirectory` — removes macOS `._` metadata files recursively
- Atomic operation control: `abortFileOp`, `copyPercentage`

---

### `list_funcs.hpp`
List and set operations, primarily file-backed.

- `readListFromFile`, `readSetFromFile`, `writeSetToFile`
- `getEntryFromListFile` — index-based access into a file-backed list
- `stringToList`, `splitIniList`, `joinIniList`
- `removeEntryFromList`, `filterItemsList`
- `compareFilesLists`, `compareWildcardFilesLists` — diff two file lists and write duplicates to output

---

### `ini_funcs.hpp`
Full INI file read/write with section and key management.

- `getParsedDataFromIniFile` — returns a `map<section, map<key, value>>`
- `parseIni`, `parseSectionsFromIni`, `parseValueFromIniSection`, `getKeyValuePairsFromSection`
- `setIniFileValue`, `setIniFileKey`, `setIniFile`
- `addIniSection`, `renameIniSection`, `removeIniSection`, `removeIniKey`
- `cleanIniFormatting` — normalizes section spacing and removes blank lines
- `saveIniFileData` — writes a full parsed data structure back to disk
- `addKeyToMatchingSections`, `removeKeyFromMatchingSections` — pattern-matched bulk edits
- `loadOptionsFromIni`, `loadSpecificSectionFromIni` — command-oriented loading for Ultrahand packages
- `parseCommandLine` — tokenizes a command line string with quoted-string support
- `getPackageHeaderFromIni` — extracts title, version, creator, about, credits, and display options
- `syncIniValue` — synchronizes a value between an in-memory map and a file

---

### `json_funcs.hpp`
JSON read/write backed by `cJSON`.

- `readJsonFromFile`, `stringToJson`
- `getStringFromJson`, `getStringFromJsonFile`
- `setJsonValue` — creates the file if it does not exist
- `renameJsonKey`
- `pushNotificationJson` — writes a notification entry to the Ultrahand notifications directory
- `json_t` typedef with `JsonDeleter` for RAII-safe pointer management

---

### `hex_funcs.hpp`
Binary file editing and hex data utilities.

- `asciiToHex`, `decimalToHex`, `hexToDecimal`
- `hexToReversedHex`, `decimalToReversedHex` — byte-order reversal utilities
- `findHexDataOffsets` — searches a binary file for a hex pattern and returns all offsets
- `hexEditByOffset` — overwrites bytes at an absolute file offset
- `hexEditByCustomOffset` — resolves an offset relative to a found ASCII anchor pattern
- `hexEditFindReplace` — find-and-replace with optional occurrence targeting
- `parseHexDataAtCustomOffset` — reads hex data at a custom-anchored offset
- `replaceHexPlaceholder` — resolves `{hex_file:...}` placeholders in argument strings
- `extractVersionFromBinary` — scans a binary for an embedded version string
- `decodeBase64ToString`
- Compile-time lookup tables (`hexTable`, `hexLookup`) and a `hexSumCache` for repeated-file performance

---

### `download_funcs.hpp`
File downloads and ZIP extraction via `libcurl` and `minizip`.

```cpp
ult::downloadFile(url, destination);
ult::unzipFile(zipPath, extractTo);
```

- Per-download socket init/exit to reduce memory pressure (opt-out with `noSocketInit`)
- Atomic progress tracking: `downloadPercentage`, `unzipPercentage`
- Abort signals: `abortDownload`, `abortUnzip`
- Configurable buffer sizes: `DOWNLOAD_READ_BUFFER`, `DOWNLOAD_WRITE_BUFFER`, `UNZIP_READ_BUFFER`, `UNZIP_WRITE_BUFFER`

---

### `mod_funcs.hpp`
Game mod format conversion.

- `pchtxt2ips` — converts a `.pchtxt` patch text file to an IPS32 binary (with `EEOF` footer)
- `pchtxt2cheat` — converts a `.pchtxt` file to an Atmosphere cheat file, with `@disabled` flag support
- `appendCheatToFile`, `cheatExists`, `extractCheatName`
- `isValidTitleID`, `findTitleID` — title ID extraction helpers
- `toBigEndian` — inline `uint32_t` / `uint16_t` byte-swap helpers

---

### `audio.hpp`
WAV audio playback via libnx `audout`.

```cpp
ult::Audio::initialize();
ult::Audio::playNavigateSound();
ult::Audio::setMasterVolume(0.8f);
```

- Supports 8–48 kHz, 16-bit PCM WAV files; mono and stereo
- Nine built-in sound types: `Navigate`, `Enter`, `Exit`, `Wall`, `On`, `Off`, `Settings`, `Move`, `Notification`
- Sounds loaded from `sdmc:/config/ultrahand/sounds/`
- `playSounds(types, count)` — mixes up to 8 sounds into a single DMA submission
- Per-sound and resampling handled in a single shared DMA buffer; no extra allocation per play
- Volume control via fixed-point master volume (0.0–1.0)
- `reloadIfDockedChanged` — automatically reloads sounds when docked state changes
- `unloadAllSounds` with optional exclusion list

---

### `haptics.hpp`
Haptic rumble feedback.

```cpp
ult::initHaptics();
ult::rumbleClickStandalone();       // single click pattern
ult::rumbleDoubleClickStandalone(); // double click pattern
```

- Designed to run on a dedicated background thread
- Both rumble functions are blocking — they return only after the full pattern completes
- `checkAndReinitHaptics` — reinitializes vibration devices if they become unavailable

---

### `tsl_utils.hpp`
System-level utilities bridging libultra and the Tesla/libtesla rendering layer.

- Framebuffer geometry: `DefaultFramebufferWidth`, `DefaultFramebufferHeight`, `correctFrameSize`
- Translation system: `loadTranslationsFromJSON`, `clearTranslationCache`, `applyLangReplacements`
- Console state: `consoleIsDocked`, `getBuildIdAsString`, `getTitleIdAsString`
- Theme and wallpaper loading: `loadWallpaperFile`, `loadRGBA8888toRGBA4444`, `reloadWallpaper`
- Overlay heap management: `getCurrentHeapSize`, `setOverlayHeapSize`, `requestOverlayExit`
- Battery and temperature (widget mode): `powerGetDetails`, `ReadSocTemperature`, `ReadPcbTemperature`
- Input key aliases (`KEY_A`–`KEY_RIGHT`) and touch input macros
- Runtime feature flags: `useSoundEffects`, `useHapticFeedback`, `useNotifications`, `useLaunchRecall`, `useAutoNTPSync`, etc.
- `initializeThemeVars`, `initializeUltrahandSettings`, `reinitializeLangVars`

---

### `exception_wrap.hpp`
Optional linker-level exception interception for binary size reduction (~20 KB).

Activated with `USE_EXCEPTION_WRAP=1`. Requires corresponding `--wrap` linker flags and `-fno-exceptions`. See the main [libultrahand README](README.md) for full setup instructions.

---

## Usage

Add `lib/libultrahand` to your project and include the central header:

```cpp
#include <ultra.hpp>   // all libultra utilities
#include <tesla.hpp>   // libtesla UI layer (includes ultra.hpp transitively)
```

Refer to the [libultrahand README](README.md) for full build setup, required libraries, makefile directives, and the Ultrahand signature step.

---

## License

Licensed under [GPLv2](LICENSE) and [CC-BY-4.0](SUB_LICENSE).

Copyright © 2023–2026 ppkantorski
