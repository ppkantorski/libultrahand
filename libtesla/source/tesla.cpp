/********************************************************************************
 * Custom Fork Information
 * 
 * File: tesla.cpp
 * Author: ppkantorski
 * Description: 
 *   This file serves as the core logic for the Ultrahand Overlay project's custom fork
 *   of libtesla, an overlay executor. Within this file, you will find a collection of
 *   functions, menu structures, and interaction logic designed to facilitate the
 *   smooth execution and flexible customization of overlays within the project.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)
 *
 *   Note: Please be aware that this notice cannot be altered or removed. It is a part
 *   of the project's documentation and must remain intact.
 *
 *  Copyright (c) 2023-2026 ppkantorski
 ********************************************************************************/


#include <tesla.hpp>

// ---------------------------------------------------------------------------
// usingLNY2 — defined here, not in the header.
// 69-line file I/O + malloc body: inlining it into every TU would bloat every
// object file that includes tesla.hpp. Called from exactly one place.
// ---------------------------------------------------------------------------
bool usingLNY2(const std::string& filePath) {
    FILE* file = fopen(filePath.c_str(), "rb");
    if (!file)
        return false;

    // --- Get file size ---
    fseek(file, 0, SEEK_END);
    const long fileSize = ftell(file);
    if (fileSize < (long)(sizeof(NroStart) + sizeof(NroHeader))) {
        fclose(file);
        return false;
    }
    const size_t fileSz = (size_t)fileSize;
    fseek(file, 0, SEEK_SET);

    // --- Read front chunk (header + MOD0 area) ---
    constexpr size_t FRONT_READ_SIZE = 8192;
    const size_t frontReadSize = (fileSz < FRONT_READ_SIZE) ? fileSz : FRONT_READ_SIZE;
    uint8_t* frontBuf = (uint8_t*)malloc(frontReadSize);
    if (!frontBuf) {
        fclose(file);
        return false;
    }

    if (fread(frontBuf, 1, frontReadSize, file) != frontReadSize) {
        free(frontBuf);
        fclose(file);
        return false;
    }

    // --- Extract offsets directly (no NroHeader copy needed) ---
    const uint32_t mod0_rel    = *reinterpret_cast<const uint32_t*>(frontBuf + 0x4);
    const uint32_t text_offset = *reinterpret_cast<const uint32_t*>(frontBuf + 0x20);

    bool isNew = false;

    // --- MOD0 detection ---
    if (text_offset < fileSz && mod0_rel != 0 && text_offset <= fileSz - mod0_rel) {
        const uint32_t mod0_offset = text_offset + mod0_rel;

        // --- MOD0 is inside front buffer ---
        if (mod0_offset <= frontReadSize - 60) {
            const uint8_t* mod0_ptr = frontBuf + mod0_offset;
            if (memcmp(mod0_ptr, "MOD0", 4) == 0 &&
                memcmp(mod0_ptr + 52, "LNY2", 4) == 0) {
                isNew = (*reinterpret_cast<const uint32_t*>(mod0_ptr + 56) >= 1);
            }
        }
        // --- MOD0 must be read separately ---
        else if (mod0_offset <= fileSz - 60) {
            uint8_t mod0Buf[60];
            fseek(file, mod0_offset, SEEK_SET);
            if (fread(mod0Buf, 1, 60, file) == 60) {
                if (memcmp(mod0Buf, "MOD0", 4) == 0 &&
                    memcmp(mod0Buf + 52, "LNY2", 4) == 0) {
                    isNew = (*reinterpret_cast<const uint32_t*>(mod0Buf + 56) >= 1);
                }
            }
        }
    }

    free(frontBuf);
    fclose(file);
    return isNew;
}

namespace tsl {

// ---------------------------------------------------------------------------
// Theme color variables
// ---------------------------------------------------------------------------

Color logoColor1;
Color logoColor2;

size_t defaultBackgroundAlpha = 0;
Color defaultBackgroundColor;
Color defaultTextColor;
Color notificationTextColor;
Color notificationTitleColor;
Color notificationClockColor;
Color headerTextColor;
Color headerSeparatorColor;
Color starColor;
Color selectionStarColor;
Color buttonColor;
Color bottomTextColor;
Color bottomSeparatorColor;
Color unfocusedColor;
Color topSeparatorColor;

Color defaultOverlayColor;
Color defaultPackageColor;
Color defaultScriptColor;
Color clockColor;
Color temperatureColor;
Color batteryColor;
Color batteryChargingColor;
Color batteryLowColor;
size_t widgetBackdropAlpha = 0;
Color widgetBackdropColor;

Color overlayTextColor;
Color ultOverlayTextColor;
Color packageTextColor;
Color ultPackageTextColor;

Color bannerVersionTextColor;
Color overlayVersionTextColor;
Color ultOverlayVersionTextColor;
Color packageVersionTextColor;
Color ultPackageVersionTextColor;
Color onTextColor;
Color offTextColor;

#if IS_LAUNCHER_DIRECTIVE
Color dynamicLogoRGB1;
Color dynamicLogoRGB2;
#endif

bool   invertBGClickColor = false;

size_t selectionBGAlpha = 0;
Color  selectionBGColor;

Color highlightColor1;
Color highlightColor2;
Color highlightColor3;
Color highlightColor4;

// Has a meaningful default — initialized to ColorHighlight
Color s_highlightColor = tsl::style::color::ColorHighlight;

size_t clickAlpha = 0;
Color  clickColor;

size_t progressAlpha = 0;
Color  progressColor;

Color scrollBarColor;
Color scrollBarWallColor;

size_t separatorAlpha = 0;
Color  separatorColor;

// Constant — initialized once here via RGB888()
const Color edgeSeparatorColor = RGB888("303030");

Color textSeparatorColor;

Color selectedTextColor;
Color selectedValueTextColor;
Color inprogressTextColor;
Color invalidTextColor;
Color clickTextColor;

size_t tableBGAlpha = 0;
Color  tableBGColor;
Color  sectionTextColor;
Color  infoTextColor;
Color  warningTextColor;

Color healthyRamTextColor;
Color neutralRamTextColor;
Color badRamTextColor;

Color trackBarSliderColor;
Color trackBarSliderBorderColor;
Color trackBarSliderMalleableColor;
Color trackBarFullColor;
Color trackBarEmptyColor;


// Prepare a map of default settings
constexpr ThemeDefault defaultThemeSettings[] = {
    // Must stay sorted alphabetically for binary search
    {"bad_ram_text_color",              "FF0000"},
    {"banner_version_text_color",       "AAAAAA"},
    {"battery_charging_color",          "00FF00"},
    {"battery_color",                   "FFFF45"},
    {"battery_low_color",               "FF0000"},
    {"bg_alpha",                        "13"},
    {"bg_color",                        "000000"},
    {"bottom_button_color",             "FFFFFF"},
    {"bottom_separator_color",          "FFFFFF"},
    {"bottom_text_color",               "FFFFFF"},
    {"unfocused_color",                 "666666"},
    {"click_alpha",                     "7"},
    {"click_color",                     "3E25F7"},
    {"click_text_color",                "FFFFFF"},
    {"clock_color",                     "FFFFFF"},
    {"default_overlay_color",           "FFFFFF"},
    {"default_package_color",           "FFFFFF"},
    {"default_script_color",            "FF33FF"},
    {"dynamic_logo_color_1",            "00E669"},
    {"dynamic_logo_color_2",            "8080EA"},
    {"header_separator_color",          "FFFFFF"},
    {"header_text_color",               "FFFFFF"},
    {"healthy_ram_text_color",          "00FF00"},
    {"highlight_color_1",               "2288CC"},
    {"highlight_color_2",               "88FFFF"},
    {"highlight_color_3",               "FFFF45"},
    {"highlight_color_4",               "F7253E"},
    {"inprogress_text_color",           "FFFF45"},
    {"invalid_text_color",              "FF0000"},
    {"invert_bg_click_color",           "false"},
    {"logo_color_1",                    "FFFFFF"},
    {"logo_color_2",                    "FF0000"},
    {"neutral_ram_text_color",          "FFAA00"},
    {"notification_text_color",         "FFFFFF"},
    {"notification_title_color",        "FFFFFF"},
    {"notification_clock_color",        "AAAAAA"},
    {"off_text_color",                  "AAAAAA"},
    {"on_text_color",                   "00FFDD"},
    {"overlay_text_color",              "FFFFFF"},
    {"overlay_version_text_color",      "AAAAAA"},
    {"package_text_color",              "FFFFFF"},
    {"package_version_text_color",      "AAAAAA"},
    {"progress_alpha",                  "7"},
    {"progress_color",                  "253EF7"},
    {"scrollbar_color",                 "555555"},
    {"scrollbar_wall_color",            "AAAAAA"},
    {"selection_bg_alpha",              "11"},
    {"selection_bg_color",              "000000"},
    {"selection_star_color",            "FFFFFF"},
    {"selection_text_color",            "9ED0FF"},
    {"selection_value_text_color",      "FF7777"},
    {"separator_alpha",                 "15"},
    {"separator_color",                 "404040"},
    {"star_color",                      "FFFFFF"},
    {"table_bg_alpha",                  "14"},
    {"table_bg_color",                  "2C2C2C"},
    {"table_info_text_color",           "9ED0FF"},
    {"table_section_text_color",        "FFFFFF"},
    {"temperature_color",               "FFFFFF"},
    {"text_color",                      "FFFFFF"},
    {"text_separator_color",            "404040"},
    {"top_separator_color",             "404040"},
    {"trackbar_empty_color",            "404040"},
    {"trackbar_full_color",             "00FFDD"},
    {"trackbar_slider_border_color",    "505050"},
    {"trackbar_slider_color",           "606060"},
    {"trackbar_slider_malleable_color", "A0A0A0"},
    {"ult_overlay_text_color",          "9ED0FF"},
    {"ult_overlay_version_text_color",  "00FFDD"},
    {"ult_package_text_color",          "9ED0FF"},
    {"ult_package_version_text_color",  "00FFDD"},
    {"warning_text_color",              "FF7777"},
    {"widget_backdrop_alpha",           "15"},
    {"widget_backdrop_color",           "000000"},
};
const size_t defaultThemeSettingsCount = sizeof(defaultThemeSettings) / sizeof(defaultThemeSettings[0]);

const char* getThemeDefault(const char* key) {
    size_t lo = 0, hi = defaultThemeSettingsCount;
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        const int cmp = strcmp(defaultThemeSettings[mid].key, key);
        if (cmp == 0) return defaultThemeSettings[mid].value;
        if (cmp < 0)  lo = mid + 1;
        else          hi = mid;
    }
    return "";
}


bool isValidHexColor(std::string_view s) {
    if (s.size() != 6) return false;
    for (char c : s) if (!isxdigit(c)) return false;
    return true;
}


// ---------------------------------------------------------------------------
// initializeThemeVars
// Reads theme INI and populates all color vars above.
// Falls through to getThemeDefault() for any missing key so colors are always
// valid even when no theme file exists.
// ---------------------------------------------------------------------------
void initializeThemeVars() {
    auto themeData = ult::getParsedDataFromIniFile(ult::THEME_CONFIG_INI_PATH);

    auto getValue = [&](const char* key) -> const char* {
        auto sectionIt = themeData.find(ult::THEME_STR);
        if (sectionIt != themeData.end()) {
            auto it = sectionIt->second.find(key);
            if (it != sectionIt->second.end()) return it->second.c_str();
        }
        return getThemeDefault(key);
    };

    auto getColor = [&](const char* key, size_t alpha = 15) {
        return RGB888(getValue(key), alpha);
    };

    auto getAlpha = [&](const char* key) {
        return ult::stoi(getValue(key));
    };

    #if IS_LAUNCHER_DIRECTIVE
    logoColor1      = getColor("logo_color_1");
    logoColor2      = getColor("logo_color_2");
    dynamicLogoRGB1 = getColor("dynamic_logo_color_1");
    dynamicLogoRGB2 = getColor("dynamic_logo_color_2");
    #endif

    defaultBackgroundAlpha       = getAlpha("bg_alpha");
    defaultBackgroundColor       = getColor("bg_color", defaultBackgroundAlpha);
    defaultTextColor             = getColor("text_color");
    notificationTextColor        = getColor("notification_text_color");
    notificationTitleColor       = getColor("notification_title_color");
    notificationClockColor       = getColor("notification_clock_color");
    headerTextColor              = getColor("header_text_color");
    headerSeparatorColor         = getColor("header_separator_color");
    starColor                    = getColor("star_color");
    selectionStarColor           = getColor("selection_star_color");
    buttonColor                  = getColor("bottom_button_color");
    bottomTextColor              = getColor("bottom_text_color");
    bottomSeparatorColor         = getColor("bottom_separator_color");
    unfocusedColor               = getColor("unfocused_color");
    topSeparatorColor            = getColor("top_separator_color");
    defaultOverlayColor          = getColor("default_overlay_color");
    defaultPackageColor          = getColor("default_package_color");
    defaultScriptColor           = getColor("default_script_color");
    clockColor                   = getColor("clock_color");
    temperatureColor             = getColor("temperature_color");
    batteryColor                 = getColor("battery_color");
    batteryChargingColor         = getColor("battery_charging_color");
    batteryLowColor              = getColor("battery_low_color");
    widgetBackdropAlpha          = getAlpha("widget_backdrop_alpha");
    widgetBackdropColor          = getColor("widget_backdrop_color", widgetBackdropAlpha);
    overlayTextColor             = getColor("overlay_text_color");
    ultOverlayTextColor          = getColor("ult_overlay_text_color");
    packageTextColor             = getColor("package_text_color");
    ultPackageTextColor          = getColor("ult_package_text_color");
    bannerVersionTextColor       = getColor("banner_version_text_color");
    overlayVersionTextColor      = getColor("overlay_version_text_color");
    ultOverlayVersionTextColor   = getColor("ult_overlay_version_text_color");
    packageVersionTextColor      = getColor("package_version_text_color");
    ultPackageVersionTextColor   = getColor("ult_package_version_text_color");
    onTextColor                  = getColor("on_text_color");
    offTextColor                 = getColor("off_text_color");
    invertBGClickColor           = (getValue("invert_bg_click_color") == ult::TRUE_STR);
    selectionBGAlpha             = getAlpha("selection_bg_alpha");
    selectionBGColor             = getColor("selection_bg_color", selectionBGAlpha);
    highlightColor1              = getColor("highlight_color_1");
    highlightColor2              = getColor("highlight_color_2");
    highlightColor3              = getColor("highlight_color_3");
    highlightColor4              = getColor("highlight_color_4");
    clickAlpha                   = getAlpha("click_alpha");
    clickColor                   = getColor("click_color", clickAlpha);
    progressAlpha                = getAlpha("progress_alpha");
    progressColor                = getColor("progress_color", progressAlpha);
    scrollBarColor               = getColor("scrollbar_color");
    scrollBarWallColor           = getColor("scrollbar_wall_color");
    separatorAlpha               = getAlpha("separator_alpha");
    separatorColor               = getColor("separator_color", separatorAlpha);
    textSeparatorColor           = getColor("text_separator_color");
    selectedTextColor            = getColor("selection_text_color");
    selectedValueTextColor       = getColor("selection_value_text_color");
    inprogressTextColor          = getColor("inprogress_text_color");
    invalidTextColor             = getColor("invalid_text_color");
    clickTextColor               = getColor("click_text_color");
    tableBGAlpha                 = getAlpha("table_bg_alpha");
    tableBGColor                 = getColor("table_bg_color", tableBGAlpha);
    sectionTextColor             = getColor("table_section_text_color");
    infoTextColor                = getColor("table_info_text_color");
    warningTextColor             = getColor("warning_text_color");
    healthyRamTextColor          = getColor("healthy_ram_text_color");
    neutralRamTextColor          = getColor("neutral_ram_text_color");
    badRamTextColor              = getColor("bad_ram_text_color");
    trackBarSliderColor          = getColor("trackbar_slider_color");
    trackBarSliderBorderColor    = getColor("trackbar_slider_border_color");
    trackBarSliderMalleableColor = getColor("trackbar_slider_malleable_color");
    trackBarFullColor            = getColor("trackbar_full_color");
    trackBarEmptyColor           = getColor("trackbar_empty_color");
}


void initializeTheme(const std::string& themeIniPath) {
    auto themeData = ult::getParsedDataFromIniFile(themeIniPath);
    auto& themeSection = themeData[ult::THEME_STR];
    bool needsUpdate = false;

    const bool hasThemeSection = ult::isFile(themeIniPath) && (themeData.count(ult::THEME_STR) > 0);
    for (size_t i = 0; i < tsl::defaultThemeSettingsCount; ++i) {
        const auto& setting = tsl::defaultThemeSettings[i];
        if (!hasThemeSection || themeSection.count(setting.key) == 0) {
            themeSection[setting.key] = setting.value;
            needsUpdate = true;
        }
    }

    if (needsUpdate)
        ult::saveIniFileData(themeIniPath, themeData);
    if (!ult::isDirectory(ult::THEMES_PATH))
        ult::createDirectory(ult::THEMES_PATH);
}


namespace gfx {
        
    // Updated thread-safe calculateStringWidth function
    float calculateStringWidth(const std::string& originalString, const float fontSize, const bool monospace) {
        if (originalString.empty() || !FontManager::isInitialized()) {
            return 0.0f;
        }
        
        // Thread-safe translation cache access
        std::string text;
        {
            std::shared_lock<std::shared_mutex> readLock(s_translationCacheMutex);
            auto translatedIt = ult::translationCache.find(originalString);
            if (translatedIt != ult::translationCache.end()) {
                text = translatedIt->second;
            } else {
                // Don't insert anything, just fallback to original string
                text = originalString;
            }
        }
        
        // CRITICAL: Use the same data types as drawString
        s32 maxWidth = 0;
        s32 currentLineWidth = 0;
        ssize_t codepointWidth;
        u32 currCharacter = 0;
        
        // Convert fontSize to u32 to match drawString behavior
        const u32 fontSizeInt = static_cast<u32>(fontSize);
        
        auto itStrEnd = text.cend();
        auto itStr = text.cbegin();
        
        // Fast ASCII check
        bool isAsciiOnly = true;
        for (unsigned char c : text) {
            if (c > 127) {
                isAsciiOnly = false;
                break;
            }
        }
        
        while (itStr != itStrEnd) {
            // Decode UTF-8 codepoint
            if (isAsciiOnly) {
                currCharacter = static_cast<u32>(*itStr);
                codepointWidth = 1;
            } else {
                codepointWidth = decode_utf8(&currCharacter, reinterpret_cast<const u8*>(&(*itStr)));
                if (codepointWidth <= 0) break;
            }
            
            itStr += codepointWidth;
            
            // Handle newlines
            if (currCharacter == '\n') {
                maxWidth = std::max(currentLineWidth, maxWidth);
                currentLineWidth = 0;
                continue;
            }
            
            // Use u32 fontSize to match drawString - now thread-safe
            std::shared_ptr<FontManager::Glyph> glyph = FontManager::getOrCreateGlyph(currCharacter, monospace, fontSizeInt);
            if (!glyph) continue;
            
            // CRITICAL: Use the same calculation as drawString
            currentLineWidth += static_cast<s32>(glyph->xAdvance * glyph->currFontSize);
        }
        
        // Final width calculation
        maxWidth = std::max(currentLineWidth, maxWidth);
        return static_cast<float>(maxWidth);
    }
}

namespace hlp {

    /**
     * @brief Toggles focus between the Tesla overlay and the rest of the system
     *
     * @param enabled Focus Tesla?
     */
    void requestForeground(bool enabled, bool updateGlobalFlag) {
        if (updateGlobalFlag)
            ult::currentForeground.store(enabled, std::memory_order_release);

        u64 applicationAruid = 0, appletAruid = 0;
        
        for (u64 programId = 0x0100000000001000UL; programId < 0x0100000000001020UL; programId++) {
            pmdmntGetProcessId(&appletAruid, programId);
            
            if (appletAruid != 0)
                hidsysEnableAppletToGetInput(!enabled, appletAruid);
        }
        

        pmdmntGetApplicationProcessId(&applicationAruid);
        hidsysEnableAppletToGetInput(!enabled, applicationAruid);
        
        hidsysEnableAppletToGetInput(true, 0);
    }

    namespace ini {
        /**
         * @brief Unparses ini data into a string
         *
         * @param iniData Ini data
         * @return Ini string
         */
        std::string unparseIni(const IniData &iniData) {
            std::string result;
            bool addSectionGap = false;
        
            for (const auto &section : iniData) {
                if (addSectionGap) {
                    result += '\n';
                }
                result += '[' + section.first + "]\n";
                for (const auto &keyValue : section.second) {
                    result += keyValue.first + '=' + keyValue.second + '\n';
                }
                addSectionGap = true;
            }
        
            return result;
        }
    }

    /**
     * @brief Encodes key codes into a combo string
     *
     * @param keys Key codes
     * @return Combo string
     */
    std::string keysToComboString(u64 keys) {
        if (keys == 0) return "";  // Early return for empty input
    
        std::string result;
        bool first = true;
    
        for (const auto &keyInfo : ult::KEYS_INFO) {
            if (keys & keyInfo.key) {
                if (!first) {
                    result += "+";
                }
                result += keyInfo.name;
                first = false;
            }
        }
    
        return result;
    }

    // Function to load key combo mappings from both overlays.ini and packages.ini
    void loadEntryKeyCombos() {
        std::lock_guard<std::mutex> lock(comboMutex);
        ult::g_entryCombos.clear();
    
        // Load overlay combos from overlays.ini
        auto overlayData = ult::getParsedDataFromIniFile(ult::OVERLAYS_INI_FILEPATH);
        std::string fullPath;
        u64 keys;

        std::vector<std::string> modeList, comboList;
        for (auto& [fileName, settings] : overlayData) {
            fullPath = ult::OVERLAY_PATH + fileName;
    
            // 1) main key_combo
            if (auto it = settings.find(ult::KEY_COMBO_STR); it != settings.end() && !it->second.empty()) {
                keys = hlp::comboStringToKeys(it->second);
                if (keys) ult::g_entryCombos[keys] = { fullPath, "" };
            }
    
            // 2) per-mode combos
            auto modesIt = settings.find("mode_args");
            auto argsIt  = settings.find("mode_combos");
            if (modesIt != settings.end()) {
                modeList  = ult::splitIniList(modesIt->second);
                comboList = (argsIt != settings.end())
                               ? ult::splitIniList(argsIt->second)
                               : std::vector<std::string>();
                if (comboList.size() < modeList.size())
                    comboList.resize(modeList.size());
    
                for (size_t i = 0; i < modeList.size(); ++i) {
                    const std::string& comboStr = comboList[i];
                    if (comboStr.empty()) continue;
                    keys = hlp::comboStringToKeys(comboStr);
                    if (!keys) continue;
                    // launchArg is the *mode* (i.e. modeList[i])
                    ult::g_entryCombos[keys] = { fullPath, modeList[i] };
                }
            }
        }
    
        // Load package combos from packages.ini
        auto packageData = ult::getParsedDataFromIniFile(ult::PACKAGES_INI_FILEPATH);
        for (auto& [packageName, settings] : packageData) {
            // Only handle main key_combo for packages (no modes for packages)
            if (auto it = settings.find(ult::KEY_COMBO_STR); it != settings.end() && !it->second.empty()) {
                keys = hlp::comboStringToKeys(it->second);
                if (keys) ult::g_entryCombos[keys] = { ult::OVERLAY_PATH + "ovlmenu.ovl", "--package " + packageName};
            }
        }
    }
}


// Max notifications cap (max value of 4 on limited memory, 8 otherwise)
int maxNotifications = 3;


// ---------------------------------------------------------------------------
// initializeUltrahandSettings  (non-launcher overlays only)
// ---------------------------------------------------------------------------
//#if !IS_LAUNCHER_DIRECTIVE
//void initializeUltrahandSettings() {
//    auto ultrahandSection = ult::getKeyValuePairsFromSection(
//        ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME);
//
//    auto getStringValue = [&](const std::string& key,
//                               const std::string& defaultValue = "") -> std::string {
//        auto it = ultrahandSection.find(key);
//        if (it != ultrahandSection.end() && !it->second.empty())
//            return it->second;
//        return defaultValue;
//    };
//
//    auto getBoolValue = [&](const std::string& key, bool defaultValue = false) -> bool {
//        auto it = ultrahandSection.find(key);
//        if (it != ultrahandSection.end() && !it->second.empty())
//            return (it->second == ult::TRUE_STR);
//        return defaultValue;
//    };
//
//    std::string defaultLang = getStringValue(ult::DEFAULT_LANG_STR, "en");
//    if (defaultLang.empty()) defaultLang = "en";
//
//    #ifdef UI_OVERRIDE_PATH
//    {
//        std::string UI_PATH = UI_OVERRIDE_PATH;
//        ult::preprocessPath(UI_PATH);
//        ult::createDirectory(UI_PATH);
//
//        const std::string NEW_THEME_CONFIG_INI_PATH = UI_PATH + "theme.ini";
//        const std::string NEW_WALLPAPER_PATH        = UI_PATH + "wallpaper.rgba";
//        const std::string TRANSLATION_JSON_PATH     = UI_PATH + "lang/" + defaultLang + ".json";
//
//        if (ult::isFile(NEW_THEME_CONFIG_INI_PATH))
//            ult::THEME_CONFIG_INI_PATH = NEW_THEME_CONFIG_INI_PATH;
//        if (ult::isFile(NEW_WALLPAPER_PATH))
//            ult::WALLPAPER_PATH = NEW_WALLPAPER_PATH;
//        if (ult::isFile(TRANSLATION_JSON_PATH))
//            ult::loadTranslationsFromJSON(TRANSLATION_JSON_PATH);
//    }
//    #endif
//
//    ult::useLaunchCombos    = getBoolValue("launch_combos", true);
//    ult::useNotifications   = getBoolValue("notifications", true);
//
//    if (ult::useNotifications) {
//        if (!ult::isFile(ult::NOTIFICATIONS_FLAG_FILEPATH)) {
//            if (FILE* file = fopen(ult::NOTIFICATIONS_FLAG_FILEPATH.c_str(), "w"))
//                fclose(file);
//        }
//    } else {
//        ult::deleteFileOrDirectory(ult::NOTIFICATIONS_FLAG_FILEPATH);
//    }
//
//    ult::useNotificationsHotkey = getBoolValue("notifications_hotkey", true);
//    ult::silenceNotifications = getBoolValue("silence_notifications", false);
//
//    const std::string maxNotifStr = getStringValue("max_notifications");
//    if (!maxNotifStr.empty()) {
//        const int maxAllowed = ult::limitedMemory ? 4 : tsl::NotificationPrompt::MAX_VISIBLE;
//        tsl::maxNotifications = std::max(1, std::min(ult::stoi(maxNotifStr), maxAllowed));
//    }
//
//    ult::useSoundEffects      = getBoolValue("sound_effects", false);
//    ult::useHapticFeedback    = getBoolValue("haptic_feedback", false);
//    ult::useSwipeToOpen       = getBoolValue("swipe_to_open", true);
//    ult::useOpaqueScreenshots = getBoolValue("opaque_screenshots", true);
//
//    ultrahandSection.clear();
//
//    const std::string langFile = ult::LANG_PATH + defaultLang + ".json";
//    if (ult::isFile(langFile))
//        ult::parseLanguage(langFile);
//    else
//        ult::reinitializeLangVars();
//}
//#endif



// Returns true for scripts where every character is a standalone word unit
static bool isWordPerCharScript(u32 cp) {
    return (cp >= 0x1100  && cp <= 0x11FF)  // Hangul Jamo
        || (cp >= 0x2E80  && cp <= 0x2FFF)  // CJK radicals, Kangxi
        || (cp >= 0x3000  && cp <= 0x303F)  // CJK symbols & punctuation
        || (cp >= 0x3040  && cp <= 0x30FF)  // Hiragana + Katakana
        || (cp >= 0x3100  && cp <= 0x318F)  // Bopomofo + Hangul compat jamo
        || (cp >= 0x3200  && cp <= 0x33FF)  // Enclosed/compat CJK
        || (cp >= 0x3400  && cp <= 0x4DBF)  // CJK extension A
        || (cp >= 0x4E00  && cp <= 0x9FFF)  // CJK unified ideographs
        || (cp >= 0xA000  && cp <= 0xA4FF)  // Yi
        || (cp >= 0xA960  && cp <= 0xA97F)  // Hangul Jamo extended A
        || (cp >= 0xAC00  && cp <= 0xD7FF)  // Hangul syllables + Jamo extended B
        || (cp >= 0xF900  && cp <= 0xFAFF)  // CJK compatibility ideographs
        || (cp >= 0xFE30  && cp <= 0xFE4F)  // CJK compatibility forms
        || (cp >= 0x20000 && cp <= 0x2A6DF) // CJK extension B
        || (cp >= 0x2A700 && cp <= 0x2CEAF) // CJK extensions C/D/E
        || (cp >= 0x2CEB0 && cp <= 0x2EBEF) // CJK extension F
        || (cp >= 0x30000 && cp <= 0x3134F); // CJK extension G
}

static bool isLineStartForbidden(u32 cp) {
    // Punctuation that must not appear at the start of a line
    return cp == 0x3001  // 、
        || cp == 0x3002  // 。
        || cp == 0xFF0C  // ，
        || cp == 0xFF0E  // ．
        || cp == 0xFF1A  // ：
        || cp == 0xFF1B  // ；
        || cp == 0xFF01  // ！
        || cp == 0xFF1F  // ？
        || cp == 0x30FB  // ・
        || cp == 0xFF65  // ･
        || cp == 0xFF09  // ）
        || cp == 0x3015  // 〕
        || cp == 0x3011  // 】
        || cp == 0x3009  // 〉
        || cp == 0x300B  // 》
        || cp == 0x3003  // 」
        || cp == 0x300D  // 」
        || cp == 0x300F  // 』
        || cp == 0x2026  // …
        || cp == 0x2014  // —
        || cp == 0xFF5D  // ｝
        || cp == 0x30FC; // ー (prolonged sound mark, also line-start forbidden)
}

std::vector<std::string> wrapText(
    const std::string& text,
    float maxWidth,
    const std::string& wrappingMode,
    bool useIndent,
    const std::string& indent,
    float indentWidth,
    size_t fontSize
) {
    if (wrappingMode == "none" || (wrappingMode != "char" && wrappingMode != ult::WORD_STR))
        return { text };

    std::vector<std::string> wrappedLines;
    bool firstLine = true;
    std::string currentLine;

    // Single shared helper — replaces two independent space-counting loops
    auto getLeadingSpaces = [&]() -> size_t {
        if (wrappedLines.empty()) return 0;
        const std::string& s = wrappedLines.back();
        size_t i = 0;
        while (i < s.size() && s[i] == ' ') i++;
        return i;
    };

    auto pushLine = [&](const std::string& line) {
        if (useIndent && !firstLine) {
            wrappedLines.push_back(
                wrappedLines.back().substr(0, getLeadingSpaces()) + indent + line
            );
        } else {
            wrappedLines.push_back(line);
        }
    };

    auto currentMaxWidth = [&]() -> float {
        if (firstLine) return maxWidth;
        if (!useIndent) return maxWidth - indentWidth;
        const size_t spaces = getLeadingSpaces();
        const float gapWidth = tsl::gfx::calculateStringWidth(
            wrappedLines.empty() ? "" : wrappedLines.back().substr(0, spaces),
            fontSize, false
        );
        return maxWidth - gapWidth - indentWidth;
    };

    if (wrappingMode == "char") {
        static constexpr char hyphen = '-';
        u32 prevCharacter = 0;
        u32 prevPrevCharacter = 0;
        auto itStr = text.cbegin();
        const auto itStrEnd = text.cend();

        while (itStr != itStrEnd) {
            u32 currCharacter;
            const ssize_t codepointWidth = decode_utf8(&currCharacter, reinterpret_cast<const u8*>(&(*itStr)));
            if (codepointWidth <= 0) break;

            std::string charStr(itStr, itStr + codepointWidth);
            const bool overflows = tsl::gfx::calculateStringWidth(currentLine + charStr, fontSize, false) > currentMaxWidth();

            if (overflows && !currentLine.empty()) {
                if (isLineStartForbidden(currCharacter)) {
                    pushLine(currentLine + charStr);
                    currentLine.clear();
                } else {
                    const bool needsHyphen = !useIndent
                                          && (prevCharacter != ' ')
                                          && (currCharacter != ' ')
                                          && !isWordPerCharScript(prevCharacter)
                                          && !isWordPerCharScript(currCharacter);
                    if (needsHyphen) {
                        std::string withHyphen = currentLine + hyphen;
                        if (tsl::gfx::calculateStringWidth(withHyphen, fontSize, false) > currentMaxWidth()) {
                            auto it = currentLine.end();
                            while (it != currentLine.begin()) {
                                --it;
                                if ((*it & 0xC0) != 0x80) break;
                            }
                            charStr = std::string(it, currentLine.end()) + charStr;
                            currentLine.erase(it, currentLine.end());
                            withHyphen = (prevPrevCharacter != 0 && prevPrevCharacter != ' ')
                                       ? currentLine + hyphen
                                       : currentLine;
                        }
                        pushLine(withHyphen);
                    } else {
                        pushLine(currentLine);
                    }

                    currentLine = (currCharacter == ' ') ? std::string{} : charStr;
                }
                firstLine = false;
            } else if (!overflows) {
                currentLine += charStr;
            } else {
                // overflows but currentLine is empty — just start with this char
                currentLine = charStr;
                firstLine = false;
            }

            // Single advance point — was repeated 3x before
            prevPrevCharacter = prevCharacter;
            prevCharacter = currCharacter;
            itStr += codepointWidth;
        }
    } else {
        ult::StringStream stream(text);
        std::string currentWord;
        std::string testLine;

        while (stream.getline(currentWord, ' ')) {
            u32 firstCp = 0;
            decode_utf8(&firstCp, reinterpret_cast<const u8*>(currentWord.c_str()));

            if (isWordPerCharScript(firstCp)) {
                auto itW = currentWord.cbegin();
                const auto itWEnd = currentWord.cend();
                bool firstChar = true;
                while (itW != itWEnd) {
                    u32 cp;
                    const ssize_t w = decode_utf8(&cp, reinterpret_cast<const u8*>(&(*itW)));
                    if (w <= 0) break;
                    std::string charStr(itW, itW + w);
                    testLine = currentLine;
                    if (firstChar && !testLine.empty())
                        testLine.push_back(' ');
                    testLine += charStr;
                    if (tsl::gfx::calculateStringWidth(testLine, fontSize, false) > currentMaxWidth()) {
                        if (!currentLine.empty()) {
                            pushLine(isLineStartForbidden(cp) ? currentLine + charStr : currentLine);
                            currentLine = isLineStartForbidden(cp) ? std::string{} : charStr;
                        } else {
                            currentLine = charStr;
                        }
                        firstLine = false;
                    } else {
                        currentLine = testLine;
                    }
                    firstChar = false;
                    itW += w;
                }
            } else {
                testLine = currentLine;
                if (!testLine.empty()) testLine.push_back(' ');
                testLine += currentWord;

                if (tsl::gfx::calculateStringWidth(testLine, fontSize, false) > currentMaxWidth()) {
                    if (!currentLine.empty()) {
                        pushLine(currentLine);
                        currentLine.clear();
                        firstLine = false;
                    }
            
                    // Word is too long to fit on a line — split it char by char with hyphenation
                    auto itW = currentWord.cbegin();
                    const auto itWEnd = currentWord.cend();
                    while (itW != itWEnd) {
                        u32 cp;
                        const ssize_t cw = decode_utf8(&cp, reinterpret_cast<const u8*>(&(*itW)));
                        if (cw <= 0) break;
                        std::string charStr(itW, itW + cw);
                        const std::string withHyphen = currentLine + '-';
                        const std::string withChar   = currentLine + charStr;
            
                        if (tsl::gfx::calculateStringWidth(withChar, fontSize, false) > currentMaxWidth()
                            && !currentLine.empty()) {
                            // Only hyphenate if more chars remain after this one
                            const bool moreRemain = (itW + cw) != itWEnd;
                            pushLine(moreRemain ? withHyphen : currentLine);
                            currentLine = charStr;
                            firstLine = false;
                        } else {
                            currentLine += charStr;
                        }
                        itW += cw;
                    }
                } else {
                    currentLine.swap(testLine);
                }
            }
        }
    }

    if (!currentLine.empty())
        pushLine(currentLine);

    return wrappedLines;
}


// ── NotificationPrompt out-of-line definitions ───────────────────────────────

static const std::string s_emptyStr;

bool NotificationPrompt::hasActiveFile(std::string_view fname) const {
    if (fname.empty()) return false;
    if (isStale()) return false;
    std::lock_guard<std::mutex> lg(state_mutex_);
    for (int i = 0; i < maxNotifications; ++i)
        if ((slots_[i].flags & SLOT_ACTIVE) && slots_[i].data.fileName == fname) return true;
    return false;
}

int NotificationPrompt::findHitSlot_NoLock(s32 tx, s32 ty) const {
    const s32 sx = ult::useRightAlignment
        ? static_cast<s32>(tsl::cfg::FramebufferWidth) - NOTIF_WIDTH : 0;
    for (int i = 0; i < maxNotifications; ++i) {
        const Slot& slot = slots_[i];
        if (!(slot.flags & SLOT_ACTIVE) || slot.data.state == PromptState::Inactive) continue;
        const s32 sy = static_cast<s32>(slot.yCurrent);
        const s32 sh = getEffectiveHeight(slot);
        if (tx >= sx && tx < sx + NOTIF_WIDTH &&
            ty >= sy && ty < sy + sh)
            return i;
    }
    return -1;
}

void NotificationPrompt::draw(gfx::Renderer* renderer, bool promptOnly) {
    if (ult::launchingOverlay.load(std::memory_order_acquire) || isStale()) return;
    if (!enabled_.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lg(state_mutex_);
    const u64 now = ult::nowNs();

    for (int i = 0; i < maxNotifications; ++i) {
        const Slot& slot = slots_[i];
        if (!(slot.flags & SLOT_ACTIVE) || slot.data.text.empty() ||
            slot.data.state == PromptState::Inactive) continue;

        float fadeAlpha = 1.0f;
        if (slot.data.state != PromptState::Visible) {
            const float t = std::min(1.0f,
                static_cast<float>((now - slot.data.stateStartNs) / 1'000'000ULL) / FADE_DURATION_MS);
            fadeAlpha = easeInOut(slot.data.state == PromptState::FadingIn ? t : 1.0f - t);
        }

        drawSlot(renderer, slot, static_cast<s32>(slot.yCurrent), fadeAlpha, promptOnly);
    }
}

void NotificationPrompt::update() {
    std::lock_guard<std::mutex> lg(state_mutex_);
    if (isStale()) { clearAll_NoLock(); return; }
    if (ult::launchingOverlay.load(std::memory_order_acquire)) return;

    const u64 now = ult::nowNs();
    bool anyCleared = false;

    for (int i = 0; i < maxNotifications; ++i) {
        Slot& slot = slots_[i];
        if (!(slot.flags & SLOT_ACTIVE)) continue;

        const bool outOfBounds = static_cast<s32>(slot.yTarget) + getEffectiveHeight(slot) > static_cast<s32>(tsl::cfg::FramebufferHeight);
        if (outOfBounds) {
            slot.data.stateStartNs = now;
            slot.data.expireNs     = now + static_cast<u64>(slot.data.durationMs) * 1'000'000ULL
                                   + FADE_DURATION_MS * 1'000'000ULL;
        } else if (slot.flags & SLOT_SOUND_PENDING) {
            slot.flags &= ~SLOT_SOUND_PENDING;
            triggerNotificationSound.store(true, std::memory_order_release);
        }

        if (slot.data.iconPending) {
            slot.data.iconPending = false;
            const std::string& fname = slot.data.fileName;
            if (!fname.empty()) {
                std::string base = fname;
                const size_t dot  = base.rfind('.');
                if (dot  != std::string::npos) base.erase(dot);
                const size_t dash = base.rfind('-');
                if (dash != std::string::npos) base.erase(dash);
                const std::string iconPath = ult::NOTIFICATIONS_ICONS_PATH + base + ".rgba";
                slot.iconBuf = std::make_unique<u8[]>(NOTIF_ICON_BYTES);
                if (ult::loadRGBA8888toRGBA4444(iconPath, slot.iconBuf.get(),
                                                NOTIF_ICON_DIM * NOTIF_ICON_DIM * 4)) {
                    slot.flags        |= SLOT_ICON_LOADED;
                    slot.data.hasIcon  = true;
                } else {
                    slot.iconBuf.reset();
                }
            }
        }

        if (slot.flags & SLOT_SLIDING) {
            const float st = std::min(1.0f,
                static_cast<float>((now - slot.slideStartNs) / 1'000'000ULL) / SLIDE_DURATION_MS);
            slot.yCurrent = slot.ySlideFrom + (slot.yTarget - slot.ySlideFrom) * easeInOut(st);
            if (st >= 1.0f) { slot.yCurrent = slot.yTarget; slot.flags &= ~SLOT_SLIDING; }
        }

        const u64 elapsedMs = slot.data.stateStartNs == 0 ? 0
                            : (now - slot.data.stateStartNs) / 1'000'000ULL;
        if (!outOfBounds) switch (slot.data.state) {
            case PromptState::FadingIn:
                if (elapsedMs >= FADE_DURATION_MS) {
                    slot.data.state        = PromptState::Visible;
                    slot.data.stateStartNs = now;
                }
                break;
            case PromptState::Visible:
                if (now >= slot.data.expireNs) {
                    slot.data.state        = PromptState::FadingOut;
                    slot.data.stateStartNs = now;
                }
                break;
            case PromptState::FadingOut:
                if (elapsedMs >= FADE_DURATION_MS) {
                    evictSlot_NoLock(i);
                    anyCleared = true;
                }
                break;
            default: break;
        }
    }

    if (anyCleared) repackSlots_NoLock(now);

    while (!pending_queue_.empty()) {
        int freeSlot = -1;
        for (int i = 0; i < maxNotifications; ++i)
            if (!(slots_[i].flags & SLOT_ACTIVE)) { freeSlot = i; break; }
        if (freeSlot < 0) break;
        NotifEntry next = std::move(const_cast<NotifEntry&>(pending_queue_.top()));
        pending_queue_.pop();
        placeInSlot_NoLock(freeSlot, std::move(next), false, false);
    }
}

bool NotificationPrompt::isActive() const {
    if (isStale()) return false;
    std::lock_guard<std::mutex> lg(state_mutex_);
    for (int i = 0; i < maxNotifications; ++i)
        if (slots_[i].flags & SLOT_ACTIVE) return true;
    return !pending_queue_.empty();
}

int NotificationPrompt::activeCount() const {
    if (isStale()) return 0;
    std::lock_guard<std::mutex> lg(state_mutex_);
    int count = 0;
    for (int i = 0; i < maxNotifications; ++i)
        if (slots_[i].flags & SLOT_ACTIVE) ++count;
    return count;
}

void NotificationPrompt::shutdown() {
    enabled_.store(false, std::memory_order_release);
    notificationGeneration.fetch_add(1, std::memory_order_acq_rel);
    generation_ = notificationGeneration.load(std::memory_order_acquire);
    std::lock_guard<std::mutex> lg(state_mutex_);
    clearAll_NoLock();
}

bool NotificationPrompt::hitTest(s32 tx, s32 ty) const {
    if (isStale()) return false;
    std::lock_guard<std::mutex> lg(state_mutex_);
    return findHitSlot_NoLock(tx, ty) >= 0;
}

bool NotificationPrompt::dismissAt(s32 tx, s32 ty) {
    if (isStale()) return false;
    std::lock_guard<std::mutex> lg(state_mutex_);
    const int idx = findHitSlot_NoLock(tx, ty);
    if (idx < 0) return false;
    Slot& slot = slots_[idx];
    if (slot.data.state != PromptState::FadingOut) {
        slot.data.state        = PromptState::FadingOut;
        slot.data.stateStartNs = ult::nowNs();
    }
    return true;
}

bool NotificationPrompt::dismissFront() {
    if (isStale()) return false;
    std::lock_guard<std::mutex> lg(state_mutex_);
    int best = -1;
    float bestY = 1.0e9f;
    for (int i = 0; i < maxNotifications; ++i) {
        const Slot& slot = slots_[i];
        if (!(slot.flags & SLOT_ACTIVE) || slot.data.state == PromptState::FadingOut) continue;
        if (slot.yCurrent < bestY) { bestY = slot.yCurrent; best = i; }
    }
    if (best < 0) return false;
    slots_[best].data.state        = PromptState::FadingOut;
    slots_[best].data.stateStartNs = ult::nowNs();
    return true;
}

NotificationPrompt::Lines
NotificationPrompt::getWrappedLines(const std::string& text, float pixelWidth,
                                    size_t fontSize, u8 maxLines,
                                    SplitType splitType) const {
    const std::string& stStr = (splitType == SplitType::Char) ? ult::CHAR_STR : ult::WORD_STR;

    Lines split;
    {
        size_t start = 0;
        while (start < text.size() && split.count < maxLines) {
            size_t       pos  = text.find('\n', start);
            const size_t pos2 = text.find("\\n", start);
            if (pos2 != std::string::npos && (pos == std::string::npos || pos2 < pos)) pos = pos2;
            if (pos == std::string::npos) { split.buf[split.count++] = text.substr(start); break; }
            split.buf[split.count++] = text.substr(start, pos - start);
            start = pos + (text[pos] == '\n' ? 1 : 2);
        }
    }

    Lines result;
    for (u8 si = 0; si < split.count && result.count < maxLines; ++si) {
        const auto wrapped = wrapText(split.buf[si], pixelWidth,
                                      stStr, /*useIndent=*/false, s_emptyStr, 0.f, fontSize);
        for (const auto& wl : wrapped) {
            if (result.count >= maxLines) break;
            result.buf[result.count++] = wl;
        }
    }
    if (result.count == 0 && split.count > 0)
        result.buf[result.count++] = "";
    return result;
}

s32 NotificationPrompt::getEffectiveHeight(const Slot& slot) const {
    s32 h = NOTIF_HEIGHT;
    if (!slot.data.text.empty() && !slot.data.title.empty()) {
        const s32   baseIconPad = (NOTIF_HEIGHT - NOTIF_ICON_DIM) / 2;
        const bool  hasIconCol  = (slot.data.hasIcon && (slot.flags & SLOT_ICON_LOADED)) || slot.data.iconPending;
        const s32   colWidth    = baseIconPad + NOTIF_ICON_DIM + baseIconPad;
        const s32   textAreaW   = hasIconCol ? (NOTIF_WIDTH - colWidth)
                                             : (NOTIF_WIDTH - 2 * (baseIconPad + 2));
        const float innerW      = hasIconCol ? static_cast<float>(textAreaW - baseIconPad - 2)
                                             : static_cast<float>(textAreaW);
        const Lines lines = getWrappedLines(slot.data.text, innerW, slot.data.fontSize, 4, slot.data.splitType);
        if (lines.count > 1) {
            const auto fm = tsl::gfx::FontManager::getFontMetricsForCharacter('A', slot.data.fontSize);
            h += static_cast<s32>(lines.count - 1) * fm.lineHeight;
        }
    }
    return h;
}

void NotificationPrompt::placeInSlot_NoLock(int idx, NotifEntry&& e,
                                             bool isShowNow, bool skipFadeIn,
                                             bool suppressSound) {
    const u64 now = ult::nowNs();

    float ty = 0.f;
    for (int i = 0; i < idx; ++i)
        if (slots_[i].flags & SLOT_ACTIVE) ty += static_cast<float>(getEffectiveHeight(slots_[i]));

    Slot& slot             = slots_[idx];
    slot.data              = std::move(e);
    slot.data.state        = skipFadeIn ? PromptState::Visible : PromptState::FadingIn;
    slot.data.stateStartNs = now;
    slot.data.expireNs     = (slot.data.durationMs == 0) ? UINT64_MAX
                           : now
                             + (skipFadeIn ? 0ULL : FADE_DURATION_MS * 1'000'000ULL)
                             + slot.data.durationMs * 1'000'000ULL;
    slot.data.hasIcon      = false;
    slot.data.iconPending  = !slot.data.fileName.empty();
    slot.yTarget           = ty;
    slot.yCurrent          = ty;
    slot.iconBuf.reset();
    slot.flags             = SLOT_ACTIVE
                           | (isShowNow     ? SLOT_SHOW_NOW      : 0)
                           | (suppressSound ? 0                  : SLOT_SOUND_PENDING);
}

void NotificationPrompt::repackSlots_NoLock(u64 now) {
    int order[MAX_VISIBLE];
    int count = 0;

    for (int i = 0; i < maxNotifications; ++i)
        if ((slots_[i].flags & SLOT_ACTIVE) && (slots_[i].flags & SLOT_SHOW_NOW)) { order[count++] = i; break; }
    for (int i = 0; i < maxNotifications && count < maxNotifications; ++i)
        if ((slots_[i].flags & SLOT_ACTIVE) && !(slots_[i].flags & SLOT_SHOW_NOW)) order[count++] = i;

    float y = 0.f;
    for (int i = 0; i < count; ++i) {
        Slot& s = slots_[order[i]];
        if (s.yTarget != y || s.yCurrent != y) {
            s.ySlideFrom   = s.yCurrent;
            s.yTarget      = y;
            s.slideStartNs = now;
            s.flags       |= SLOT_SLIDING;
        }
        y += static_cast<float>(getEffectiveHeight(s));
    }

    Slot packed[MAX_VISIBLE];
    for (int i = 0; i < count; ++i)
        packed[i] = std::move(slots_[order[i]]);
    for (int i = 0; i < maxNotifications; ++i)
        slots_[i] = (i < count) ? std::move(packed[i]) : Slot{};
}

void NotificationPrompt::applyEllipsis(Lines& lines, u8 maxLines,
                                        float pixelWidth, size_t fontSize,
                                        gfx::Renderer* renderer) const {
    if (lines.count <= maxLines) return;

    std::string overflow = lines.buf[maxLines];
    lines.count = maxLines;

    std::string& last = lines.buf[maxLines - 1];
    last += ' ';
    last += overflow;
    while (!last.empty()) {
        last += "...";
        if (static_cast<float>(renderer->getNotificationTextDimensions(last, false, fontSize).first) <= pixelWidth)
            return;
        last.resize(last.size() - 4);
    }
    last = "...";
}

void NotificationPrompt::drawSlot(gfx::Renderer* renderer, const Slot& slot,
                                   s32 baseY, float fadeAlpha, bool promptOnly) {
    auto fc = [renderer, fadeAlpha](Color c) {
        return renderer->a2(NotificationPrompt::applyAlpha(c, fadeAlpha));
    };

    // Pre-compute every faded color used in this draw call once.
    // fc() calls applyAlpha() + renderer->a2() (which branches + bit-masks);
    // calling it inside loops or repeated draw calls wastes those cycles.
    const Color fadedBgColor    = fc(defaultBackgroundColor);
    const Color fadedTextColor  = fc(notificationTextColor);
    const Color fadedTitleColor = fc(notificationTitleColor);
    const Color fadedClockColor = fc(notificationClockColor);
    const Color fadedEdgeColor  = fc(edgeSeparatorColor);

    const s32 x = ult::useRightAlignment
        ? (tsl::cfg::FramebufferWidth - NOTIF_WIDTH) : 0;

    const s32  baseIconPad = (NOTIF_HEIGHT - NOTIF_ICON_DIM) / 2;
    const s32  iconColW    = baseIconPad + NOTIF_ICON_DIM + baseIconPad;
    const bool hasIconCol  = (slot.data.hasIcon && (slot.flags & SLOT_ICON_LOADED)) || slot.data.iconPending;
    s32 textAreaX = x;
    s32 textAreaW = NOTIF_WIDTH;
    if (hasIconCol) {
        textAreaX = x + iconColW;
        textAreaW = NOTIF_WIDTH - iconColW;
    }
    const float     innerWf        = static_cast<float>(textAreaW - baseIconPad - 2);
    const s32       titleTextAreaX = hasIconCol ? textAreaX : x + (baseIconPad + 2);
    const s32       titleTextAreaW = hasIconCol ? textAreaW : NOTIF_WIDTH - 2 * (baseIconPad + 2);
    const float     titleInnerWf   = hasIconCol ? innerWf   : static_cast<float>(titleTextAreaW);
    const u8        fontSize       = slot.data.fontSize;
    const Alignment alignment      = slot.data.alignment;

    const bool hasTitleIconLayout = !slot.data.text.empty() && !slot.data.title.empty();

    Lines titleIconLines;
    // Declared here so both the height-calculation block and the draw block
    // share one getFontMetricsForCharacter lookup instead of two.
    // Default-constructed (all zeros) when hasTitleIconLayout is false;
    // the draw block never reaches it in that case.
    tsl::gfx::FontManager::FontMetrics titleMessageFm;
    s32 effectiveHeight = NOTIF_HEIGHT;
    if (hasTitleIconLayout) {
        titleIconLines = getWrappedLines(slot.data.text, titleInnerWf, fontSize, 5, slot.data.splitType);
        applyEllipsis(titleIconLines, 4, titleInnerWf, fontSize, renderer);
        titleMessageFm = tsl::gfx::FontManager::getFontMetricsForCharacter('A', fontSize);
        if (titleIconLines.count > 1) {
            effectiveHeight = NOTIF_HEIGHT
                            + static_cast<s32>(titleIconLines.count - 1) * titleMessageFm.lineHeight;
        }
    }
    Lines regularLines;
    s32 regularExtraSpacing = 0;
    if (!hasTitleIconLayout && !slot.data.text.empty() && textAreaW > 0) {
        regularLines = getWrappedLines(slot.data.text, static_cast<float>(textAreaW-4), fontSize, 9, slot.data.splitType);
        applyEllipsis(regularLines, 8, static_cast<float>(textAreaW-4), fontSize, renderer);
        if (regularLines.count > 1) {
            regularExtraSpacing = static_cast<s32>(regularLines.count - 1) * 3;
            effectiveHeight += regularExtraSpacing;
        }
    }

    if (baseY + effectiveHeight > static_cast<s32>(tsl::cfg::FramebufferHeight)) return;

    renderer->enableScissoring(static_cast<u32>(std::max(0, x)),
                               static_cast<u32>(std::max(0, baseY)),
                               static_cast<u32>(NOTIF_WIDTH),
                               static_cast<u32>(effectiveHeight));

    #if IS_STATUS_MONITOR_DIRECTIVE
        renderer->drawRect(x, baseY, NOTIF_WIDTH, effectiveHeight,
                           fadedBgColor);
    #else
        if (!promptOnly && ult::expandedMemory)
            renderer->drawRectMultiThreaded(x, baseY, NOTIF_WIDTH,
                                            effectiveHeight, fadedBgColor);
        else
            renderer->drawRect(x, baseY, NOTIF_WIDTH, effectiveHeight,
                               fadedBgColor);
    #endif

    const s32 iconVertPad = (effectiveHeight - NOTIF_ICON_DIM) / 2;
    if (hasIconCol && slot.data.hasIcon && (slot.flags & SLOT_ICON_LOADED) && slot.iconBuf) {
        const s32 dstX = x + baseIconPad;
        s32 drawH = NOTIF_ICON_DIM;
        if (dstX + NOTIF_ICON_DIM > static_cast<s32>(tsl::cfg::FramebufferWidth))
            drawH = 0;
        if (iconVertPad + baseY + drawH > static_cast<s32>(tsl::cfg::FramebufferHeight))
            drawH = static_cast<s32>(tsl::cfg::FramebufferHeight) - iconVertPad - baseY;
        if (drawH > 0)
            renderer->drawBitmapRGBA4444(
                static_cast<u32>(dstX), static_cast<u32>(iconVertPad + baseY),
                static_cast<u32>(NOTIF_ICON_DIM), static_cast<u32>(drawH),
                slot.iconBuf.get(), fadeAlpha);
    }

    if (!slot.data.text.empty() && textAreaW > 0) {
        if (hasTitleIconLayout) {
            const Lines& lines     = titleIconLines;
            const auto   titleFm   = tsl::gfx::FontManager::getFontMetricsForCharacter('A', TITLE_FONT);
            const auto&  messageFm = titleMessageFm;  // computed once above, reused here
            const s32    innerW    = static_cast<s32>(titleInnerWf);
            const s32    lineCount = static_cast<s32>(lines.count);
            const s32    lineStep  = messageFm.lineHeight + 3;
            static constexpr s32 LINE_GAP = 4;
            const s32 blockH   = titleFm.lineHeight + LINE_GAP + lineCount * messageFm.lineHeight;
            const s32 originY  = (effectiveHeight - blockH) / 2 + baseY;
            const s32 titleY   = originY + titleFm.ascent - 3;
            const s32 messageY = originY + titleFm.lineHeight + LINE_GAP + messageFm.ascent + 1;

            #if IS_LAUNCHER_DIRECTIVE
            if (slot.data.title.find(ult::CAPITAL_ULTRAHAND_PROJECT_NAME) != std::string::npos)
                drawUltrahandLine(renderer, slot.data.title, titleTextAreaX, titleY, TITLE_FONT, fadeAlpha, notificationTitleColor);
            else {
            #endif
                renderer->drawNotificationString(slot.data.title, false,
                    titleTextAreaX, titleY, TITLE_FONT, fadedTitleColor);
            #if IS_LAUNCHER_DIRECTIVE
            }
            #endif

            if (slot.data.showTime && slot.data.timestamp[0] != '\0') {
                const auto tsW = renderer->getNotificationTextDimensions(
                                            slot.data.timestamp, false, TITLE_FONT).first;
                const s32 tsX = textAreaX + innerW - tsW;
                if (tsX > textAreaX)
                    renderer->drawNotificationString(slot.data.timestamp, false,
                        tsX, titleY, TITLE_FONT, fadedClockColor);
            }

            for (s32 li = 0; li < lineCount; ++li) {
                s32 msgX;
                if (alignment == Alignment::Left) {
                    msgX = titleTextAreaX;
                } else {
                    const auto lw = renderer->getNotificationTextDimensions(lines[li], false, fontSize).first;
                    msgX = (alignment == Alignment::Right)
                        ? titleTextAreaX + innerW - lw
                        : titleTextAreaX + (innerW - lw) / 2;
                }
                renderer->drawNotificationString(lines[li], false, msgX,
                    messageY + li * lineStep,
                    fontSize, fadedTextColor);
            }

        } else {
            const Lines& lines        = regularLines;
            const auto   fm           = tsl::gfx::FontManager::getFontMetricsForCharacter('A', fontSize);
            const s32    lineCount    = static_cast<s32>(lines.count);
            const s32    totalTextHeight = lineCount * fm.lineHeight + (lineCount > 1 ? regularExtraSpacing : 0);
            const s32    startY       = (effectiveHeight - totalTextHeight) / 2 + fm.ascent + baseY;
            const s32    padAdjust    = hasIconCol ? baseIconPad+2 : 0;
            const s32    extraPerLine = lineCount > 1 ? 3 : 0;

            auto alignedX = [&](s32 contentW) -> s32 {
                if (alignment == Alignment::Left)  return textAreaX + 2;
                if (alignment == Alignment::Right) return textAreaX + textAreaW - contentW - padAdjust - 2;
                return textAreaX + (textAreaW - contentW - padAdjust) / 2;
            };

            // For Left alignment alignedX() ignores contentW, so skip the
            // width measurement entirely — it's a no-draw drawString pass
            // (same cost as measuring) wasted on every line.
            const bool needsLineWidth = (alignment != Alignment::Left);

            for (s32 li = 0; li < lineCount; ++li) {
                const std::string& line  = lines[li];
                const s32          lineY = startY + li * (fm.lineHeight + extraPerLine);

                #if IS_LAUNCHER_DIRECTIVE
                const size_t up = line.find(ult::CAPITAL_ULTRAHAND_PROJECT_NAME);
                if (up != std::string::npos) {
                    const std::string  before = line.substr(0, up);
                    const std::string  after  = line.substr(up + ult::CAPITAL_ULTRAHAND_PROJECT_NAME.length());
                    const std::string& hand   = ult::SPLIT_PROJECT_NAME_2;
                    s32 bw = 0, aw = 0;
                    if (!before.empty()) bw = renderer->getNotificationTextDimensions(before, false, fontSize).first;
                    if (!after.empty())  aw = renderer->getNotificationTextDimensions(after,  false, fontSize).first;
                    const s32 hw = renderer->getNotificationTextDimensions(hand, false, fontSize).first;
                    const s32 uw = tsl::elm::calculateUltraTextWidth(renderer, fontSize, true);
                    drawUltrahandLine(renderer, line, alignedX(bw + uw + hw + aw), lineY, fontSize, fadeAlpha);
                } else
                #endif
                {
                    const s32 lw = needsLineWidth
                        ? renderer->getNotificationTextDimensions(line, false, fontSize).first
                        : 0;
                    renderer->drawNotificationString(line, false,
                        alignedX(lw), lineY,
                        fontSize, fadedTextColor);
                }
            }
        }
    }

    const s32 edgeX = ult::useRightAlignment ? x : x + NOTIF_WIDTH - 1;
    renderer->drawRect(edgeX, baseY, 1, effectiveHeight, fadedEdgeColor);
    renderer->drawRect(x, baseY + effectiveHeight - 1, NOTIF_WIDTH, 1, fadedEdgeColor);
    renderer->disableScissoring();
}

#if IS_LAUNCHER_DIRECTIVE
void NotificationPrompt::drawUltrahandLine(gfx::Renderer* renderer, const std::string& line,
                                            s32 x, s32 y, u32 fontSize, float fadeAlpha,
                                            Color textColor) {
    auto fc = [&](Color c) { return applyAlpha(c, fadeAlpha); };
    const size_t up = line.find(ult::CAPITAL_ULTRAHAND_PROJECT_NAME);
    if (up == std::string::npos) {
        renderer->drawNotificationString(line, false, x, y, fontSize, fc(textColor));
        return;
    }
    const std::string  before = line.substr(0, up);
    const std::string& hand   = ult::SPLIT_PROJECT_NAME_2;
    const std::string  after  = line.substr(up + ult::CAPITAL_ULTRAHAND_PROJECT_NAME.length());
    s32 bw = 0, hw = 0;
    if (!before.empty()) bw = renderer->getNotificationTextDimensions(before, false, fontSize).first;
    hw = renderer->getNotificationTextDimensions(hand, false, fontSize).first;
    s32 curX = x;
    if (!before.empty()) {
        renderer->drawNotificationString(before, false, curX, y, fontSize, fc(textColor));
        curX += bw;
    }
    curX = tsl::elm::drawDynamicUltraText(renderer, curX, y, fontSize, fc(logoColor1), true);
    renderer->drawNotificationString(hand, false, curX, y, fontSize, fc(logoColor2));
    curX += hw;
    if (!after.empty())
        renderer->drawNotificationString(after, false, curX, y, fontSize, fc(textColor));
}
#endif

} // namespace tsl