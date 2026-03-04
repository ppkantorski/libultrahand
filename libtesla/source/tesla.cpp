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
Color  defaultBackgroundColor;
Color  defaultTextColor;
Color  notificationTextColor;
Color  headerTextColor;
Color  headerSeparatorColor;
Color  starColor;
Color  selectionStarColor;
Color  buttonColor;
Color  bottomTextColor;
Color  bottomSeparatorColor;
Color  topSeparatorColor;

Color defaultOverlayColor;
Color defaultPackageColor;
Color defaultScriptColor;
Color clockColor;
Color temperatureColor;
Color batteryColor;
Color batteryChargingColor;
Color batteryLowColor;
size_t widgetBackdropAlpha = 0;
Color  widgetBackdropColor;

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
        return ult::getThemeDefault(key);
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
    headerTextColor              = getColor("header_text_color");
    headerSeparatorColor         = getColor("header_separator_color");
    starColor                    = getColor("star_color");
    selectionStarColor           = getColor("selection_star_color");
    buttonColor                  = getColor("bottom_button_color");
    bottomTextColor              = getColor("bottom_text_color");
    bottomSeparatorColor         = getColor("bottom_separator_color");
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

// ---------------------------------------------------------------------------
// initializeUltrahandSettings  (non-launcher overlays only)
// ---------------------------------------------------------------------------
#if !IS_LAUNCHER_DIRECTIVE
void initializeUltrahandSettings() {
    auto ultrahandSection = ult::getKeyValuePairsFromSection(
        ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME);

    auto getStringValue = [&](const std::string& key,
                               const std::string& defaultValue = "") -> std::string {
        auto it = ultrahandSection.find(key);
        if (it != ultrahandSection.end() && !it->second.empty())
            return it->second;
        return defaultValue;
    };

    auto getBoolValue = [&](const std::string& key, bool defaultValue = false) -> bool {
        auto it = ultrahandSection.find(key);
        if (it != ultrahandSection.end() && !it->second.empty())
            return (it->second == ult::TRUE_STR);
        return defaultValue;
    };

    std::string defaultLang = getStringValue(ult::DEFAULT_LANG_STR, "en");
    if (defaultLang.empty()) defaultLang = "en";

    #ifdef UI_OVERRIDE_PATH
    {
        std::string UI_PATH = UI_OVERRIDE_PATH;
        ult::preprocessPath(UI_PATH);
        ult::createDirectory(UI_PATH);

        const std::string NEW_THEME_CONFIG_INI_PATH = UI_PATH + "theme.ini";
        const std::string NEW_WALLPAPER_PATH        = UI_PATH + "wallpaper.rgba";
        const std::string TRANSLATION_JSON_PATH     = UI_PATH + "lang/" + defaultLang + ".json";

        if (ult::isFile(NEW_THEME_CONFIG_INI_PATH))
            ult::THEME_CONFIG_INI_PATH = NEW_THEME_CONFIG_INI_PATH;
        if (ult::isFile(NEW_WALLPAPER_PATH))
            ult::WALLPAPER_PATH = NEW_WALLPAPER_PATH;
        if (ult::isFile(TRANSLATION_JSON_PATH))
            ult::loadTranslationsFromJSON(TRANSLATION_JSON_PATH);
    }
    #endif

    ult::useLaunchCombos    = getBoolValue("launch_combos", true);
    ult::useNotifications   = getBoolValue("notifications", true);

    if (ult::useNotifications) {
        if (!ult::isFile(ult::NOTIFICATIONS_FLAG_FILEPATH)) {
            if (FILE* file = std::fopen(ult::NOTIFICATIONS_FLAG_FILEPATH.c_str(), "w"))
                std::fclose(file);
        }
    } else {
        ult::deleteFileOrDirectory(ult::NOTIFICATIONS_FLAG_FILEPATH);
    }

    ult::useNotificationsHotkey = getBoolValue("notifications_hotkey", true);
    ult::silenceNotifications = getBoolValue("silence_notifications", false);

    ult::useSoundEffects      = getBoolValue("sound_effects", false);
    ult::useHapticFeedback    = getBoolValue("haptic_feedback", false);
    ult::useSwipeToOpen       = getBoolValue("swipe_to_open", true);
    ult::useOpaqueScreenshots = getBoolValue("opaque_screenshots", true);

    ultrahandSection.clear();

    const std::string langFile = ult::LANG_PATH + defaultLang + ".json";
    if (ult::isFile(langFile))
        ult::parseLanguage(langFile);
    else
        ult::reinitializeLangVars();
}
#endif



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
    if (wrappingMode == "none" || (wrappingMode != "char" && wrappingMode != "word"))
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

} // namespace tsl