# libultrahand
[![platform](https://img.shields.io/badge/platform-Switch-898c8c?logo=C++.svg)](https://gbatemp.net/forums/nintendo-switch.283/?prefix_id=44)
[![language](https://img.shields.io/badge/language-C++-ba1632?logo=C++.svg)](https://github.com/topics/cpp)
[![GPLv2 License](https://img.shields.io/badge/license-GPLv2-189c11.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![Latest Version](https://img.shields.io/github/v/release/ppkantorski/libultrahand?label=latest%20version&color=blue)](https://github.com/ppkantorski/libultrahand/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/ppkantorski/libultrahand/total?color=6f42c1)](https://github.com/ppkantorski/libultrahand/graphs/traffic)
[![GitHub issues](https://img.shields.io/github/issues/ppkantorski/libultrahand?color=222222)](https://github.com/ppkantorski/libultrahand/issues)
[![GitHub stars](https://img.shields.io/github/stars/ppkantorski/libultrahand)](https://github.com/ppkantorski/libultrahand/stargazers)


Expanded [**libtesla**](https://github.com/WerWolv/libtesla) (originally by [WerWolv](https://github.com/WerWolv)) + **libultra** libraries for overlay development on the Nintendo Switch

![libultrahand Logo](.pics/libultrahand.png)

## Usage
### Overriding Themes and Wallpapers

To customize the theme and wallpaper for your overlay, you can override the default settings by adding the following lines to your `initServices` function:

```cpp
if (isFileOrDirectory("sdmc:/config/<OVERLAY_NAME>/theme.ini"))
    THEME_CONFIG_INI_PATH = "sdmc:/config/<OVERLAY_NAME>/theme.ini"; // Override theme path (optional)
if (isFileOrDirectory("sdmc:/config/<OVERLAY_NAME>/wallpaper.rgba"))
    WALLPAPER_PATH = "sdmc:/config/<OVERLAY_NAME>/wallpaper.rgba"; // Override wallpaper path (optional)
```

Replace `<OVERLAY_NAME>` with the name of your overlay directory. This allows you to specify custom Ultrahand theme and wallpaper files for your overlay to use located in your SD card's `/config/<OVERLAY_NAME>/` directory.


### Initializing Settings

Ultrahand Overlay theme variables and settings for your overlay are read automatically upon initialization.  Themes loading implementation is currently set within `OverlayFrame` and `HeaderOverlayFrame`.  However if you are breaking your project up into individual parts that only import `tesla.hpp` modify Elements, you may still need to include this line somewhere for the theme to be applied to that element.

```cpp
tsl::initializeThemeVars(); // Initialize variables for ultrahand themes
```

### Download Methods

To utilize the `libultra` download methods in your project, you will need to add the following line to your `initServices` function:
```cpp
initializeCurl();
```
As well as the following line to your `exetServices` function:
```cpp
cleanupCurl();
```

These lines will ensure `curl` functions properly within the overlay.

## Compiling
### Necessary Libraries
Developers should include the following libararies in their `Makefile` if they want full `libultra` functionality in their projects.

```
LIBS := -lcurl -lz -lzzip -lmbedtls -lmbedx509 -lmbedcrypto -ljansson -lnx
```

### Optional Compilation Flags
```
-ffunction-sections -fdata-sections
```
These options are present in both CFLAGS and CXXFLAGS. They instruct the compiler to place each function and data item in its own section, which allows the linker to more easily identify and remove unused code.

```
-Wl,--gc-sections
```
Included in LDFLAGS. This linker flag instructs the linker to remove unused sections that were created by -ffunction-sections and -fdata-sections. This ensures that functions or data that are not used are removed from the final executable.

```
-flto (Link Time Optimization)
```
Present in CFLAGS, CXXFLAGS, and LDFLAGS. It enables link-time optimization, allowing the compiler to optimize across different translation units and remove any unused code during the linking phase. You also use -flto=6 to control the number of threads for parallel LTO, which helps speed up the process.

```
-fuse-linker-plugin
```
This flag allows the compiler and linker to better collaborate when using LTO, which further helps in optimizing and eliminating unused code.
Together, these flags (-ffunction-sections, -fdata-sections, -Wl,--gc-sections, and -flto) ensure that any unused functions or data are stripped out during the build process, leading to a smaller and more optimized final binary.


## Examples
- [Ultrahand Overlay](https://github.com/ppkantorski/Ultrahand-Overlay)

- [Edizon Overlay](https://github.com/ppkantorski/EdiZon-Overlay)

- [Sysmodules](https://github.com/ppkantorski/ovl-sysmodules)

- [FPSLocker](https://github.com/ppkantorski/FPSLocker)

## Contributing

Contributions are welcome! If you have any ideas, suggestions, or bug reports, please raise an [issue](https://github.com/ppkantorski/libultrahand/issues/new/choose), submit a [pull request](https://github.com/ppkantorski/libultrahand/compare) or reach out to me directly on [GBATemp](https://gbatemp.net/threads/ultrahand-overlay-the-fully-craft-able-overlay-executor.633560/).

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/X8X3VR194)

## License

This project is licensed and distributed under [GPLv2](LICENSE) with a [custom library](libultra) utilizing [CC-BY-4.0](SUB_LICENSE).

Copyright (c) 2024 ppkantorski
