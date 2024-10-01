# libultrahand
Expanded [**libtesla**](https://github.com/WerWolv/libtesla) (originally by WerWolv) + **libultra** libraries for overlay development on the Nintendo Switch

![libultrahand Logo](.pics/libultrahand.png)

# Usage
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

To initialize / enable the Ultrahand Overlay theme variables and settings for your overlay, ensure you add the following lines to your `initServices` function:

```cpp
tsl::initializeThemeVars(); // Initialize variables for ultrahand themes
tsl::initializeUltrahandSettings(); // Set up for opaque screenshots and swipe-to-open functionality
```


## Examples
- [Ultrahand Overlay](https://github.com/ppkantorski/Ultrahand-Overlay)

- [Edizon Overlay](https://github.com/ppkantorski/EdiZon-Overlay)

- [Sysmodules](https://github.com/ppkantorski/ovl-sysmodules)

- [FPSLocker](https://github.com/ppkantorski/FPSLocker)

## License

This project is licensed and distributed under [GPLv2](LICENSE) with a [custom library](libultra) utilizing [CC-BY-4.0](SUB_LICENSE).

Copyright (c) 2024 ppkantorski
