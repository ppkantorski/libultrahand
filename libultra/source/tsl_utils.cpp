/********************************************************************************
 * File: tsl_utils.cpp
 * Author: ppkantorski
 * Description: 
 *   'tsl_utils.cpp' provides the implementation of various utility functions
 *   defined in 'tsl_utils.hpp' for the Ultrahand Overlay project. This source file
 *   includes functionality for system checks, input handling, time-based interpolation,
 *   and other application-specific features essential for operating custom overlays
 *   on the Nintendo Switch.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository:
 *   GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay
 *
 *   Note: This notice is integral to the project's documentation and must not be 
 *   altered or removed.
 *
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2023-2026 ppkantorski
 ********************************************************************************/

#include <tsl_utils.hpp>
#include <glass.hpp>
#include <animated.hpp>
#include <image_fx.hpp>
#if IS_LAUNCHER_DIRECTIVE
#include <zlib.h>   // PNG wallpapers (convertPngToRgba)
#endif

//#include <cstdlib>
extern "C" { // assertion override
    void __assert_func(const char *_file, int _line, const char *_func, const char *_expr ) {
        abort();
    }
}

namespace ult {

    void formatTimestamp(time_t t, char* buf, size_t bufSize) {
        struct tm tm;
        localtime_r(&t, &tm);
        int hour12 = tm.tm_hour % 12;
        if (hour12 == 0) hour12 = 12;
        std::snprintf(buf, bufSize, "%d:%02d %s",
                      hour12, tm.tm_min, (tm.tm_hour >= 12) ? "PM" : "AM");
    }

    double cos(double x) {
        //static constexpr double PI = 3.14159265358979323846;
        static constexpr double TWO_PI = 6.28318530717958647692;
        static constexpr double HALF_PI = 1.57079632679489661923;
        
        x = x - TWO_PI * static_cast<int>(x * 0.159154943091895);
        if (x < 0) x += TWO_PI;
        
        int sign = 1;
        if (x > _M_PI) {
           x -= _M_PI;
           sign = -1;
        }
        if (x > HALF_PI) {
           x = _M_PI - x;
           sign = -sign;
        }
        
        const double x2 = x * x;
        return sign * (1.0 + x2 * (-0.5 + x2 * (0.04166666666666666 + x2 * (-0.001388888888888889 + x2 * (0.0000248015873015873 - x2 * 0.0000002755731922398589)))));
    }

    bool correctFrameSize; // for detecting the correct Overlay display size

    u16 DefaultFramebufferWidth = 448;            ///< Width of the framebuffer
    u16 DefaultFramebufferHeight = 720;           ///< Height of the framebuffer

    bool windowedLayerPixelPerfect = false;

    std::unordered_map<std::string, std::string> translationCache;
    
    std::unordered_map<u64, OverlayCombo> g_entryCombos;
    std::atomic<bool> launchingOverlay(false);
    std::atomic<bool> settingsInitialized(false);
    std::atomic<bool> currentForeground(false);

    // Helper function to read file content into a string
    bool readFileContent(const std::string& filePath, std::string& content) {
        FILE* file = fopen(filePath.c_str(), "r");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to open JSON file: " + filePath);
            #endif
            return false;
        }
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), file) != nullptr) {
            content += buffer;
        }
        fclose(file);
        
        return true;
    }
    
    // Helper function to parse JSON-like content into a map
    void parseJsonContent(const std::string& content, std::unordered_map<std::string, std::string>& result) {
        size_t pos = 0;
        size_t keyStart, keyEnd, colonPos, valueStart, valueEnd;
        std::string key, value;
        
        auto normalizeNewlines = [](std::string &s) {
            size_t n = 0;
            while ((n = s.find("\\n", n)) != std::string::npos) {
                s.replace(n, 2, "\n");
                n += 1;
            }
        };
        
        while ((pos = content.find('"', pos)) != std::string::npos) {
            keyStart = pos + 1;
            keyEnd = content.find('"', keyStart);
            if (keyEnd == std::string::npos) break;
            
            key = content.substr(keyStart, keyEnd - keyStart);
            colonPos = content.find(':', keyEnd);
            if (colonPos == std::string::npos) break;
            
            valueStart = content.find('"', colonPos);
            valueEnd = content.find('"', valueStart + 1);
            if (valueStart == std::string::npos || valueEnd == std::string::npos) break;
            
            value = content.substr(valueStart + 1, valueEnd - valueStart - 1);
            
            // Convert escaped newlines (\\n) into real ones
            normalizeNewlines(key);
            normalizeNewlines(value);
            
            result[key] = value;
            
            key.clear();
            value.clear();
            pos = valueEnd + 1; // Move to next pair
        }
    }
    
    // Function to parse JSON key-value pairs into a map
    bool parseJsonToMap(const std::string& filePath, std::unordered_map<std::string, std::string>& result) {
        std::string content;
        if (!readFileContent(filePath, content)) {
            return false;
        }
        
        parseJsonContent(content, result);
        return true;
    }
    
    // Function to load translations from a JSON-like file into the translation cache
    bool loadTranslationsFromJSON(const std::string& filePath) {
        return parseJsonToMap(filePath, translationCache);
    }

    // Function to clear the translation cache
    void clearTranslationCache() {
        translationCache.clear();
        translationCache.rehash(0);
    }
    
    
    u16 activeHeaderHeight = 98;

    bool consoleIsDocked() {
        Result rc = apmInitialize();
        if (R_FAILED(rc)) return false;
        
        ApmPerformanceMode perfMode = ApmPerformanceMode_Invalid;
        rc = apmGetPerformanceMode(&perfMode);
        apmExit();
        
        return R_SUCCEEDED(rc) && (perfMode == ApmPerformanceMode_Boost);
    }

    std::string getBuildIdAsString() {
        u64 pid = 0;
        if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) || R_FAILED(ldrDmntInitialize()))
            return NULL_STR;
        
        LoaderModuleInfo moduleInfos[2];
        s32 count = 0;
        Result rc = ldrDmntGetProcessModuleInfo(pid, moduleInfos, 2, &count);
        ldrDmntExit();
        
        if (R_FAILED(rc) || count == 0)
            return NULL_STR;
        
        u64 buildid;
        std::memcpy(&buildid, moduleInfos[1].build_id, sizeof(u64));
        
        char buildIdStr[17];
        snprintf(buildIdStr, sizeof(buildIdStr), "%016lX", __builtin_bswap64(buildid));
        return std::string(buildIdStr);
    }
    

    std::string getTitleIdAsString() {
        u64 pid = 0, tid = 0;
        if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) ||
            R_FAILED(pmdmntGetProgramId(&tid, pid)))
            return NULL_STR;
    
        char tidStr[17];
        snprintf(tidStr, sizeof(tidStr), "%016lX", tid);
        return std::string(tidStr);
    }

    
    std::string lastTitleID;
    std::atomic<bool> resetForegroundCheck(false); // initialize as true
    std::atomic<u64> foregroundReassertStartTick(0); // 0 = burst inactive
    std::atomic<bool> internalTouchReleased(true);

    u32 layerEdge = 0;
    bool useRightAlignment = false;
    bool useSwipeToOpen = true;
    bool useTextBorder = true;  // thin black ring around all text but the banner logo (Theme Settings > Text Border)
    s32 swipeOffset = 0; // "swipe_offset" — swipe-to-open deadzone calibration

    bool useLaunchCombos = true;
    //bool useLaunchRecall = true;
    //bool usePageRecall = true;
    bool useNotifications = true;
    bool useNotificationsHotkey = true;
    bool useSwitch2Notifications = true;
    bool useStartupNotification = true;
    bool silenceNotifications = false;
    bool useSoundEffects = true;
    bool useButtonHaptics = false;
    bool useTouchHaptics = true;
    bool useAutoNTPSync = true;
    bool useStickNavigation = true;
    bool usePageSwap = false;
    bool useSwitch2Style = true;
    bool useFrostedGlass = false;
    bool glassWithCheats = false;
    std::atomic<bool> glassFallback{false};
    u8 glassDarkness = 5;
    u8 glassBlurRadius = 1;
    u8 glassRed = 0xFF, glassGreen = 0xFF, glassBlue = 0xFF;  // 0xFF: follow the theme
#if USING_LIVE_LOGGING_DIRECTIVE
    bool glassLog = false;
#endif
    bool useGlassLayer = true;
    bool useDynamicLogo = false;
    bool useUniformLogo = false;
    bool useDynamicTableColors = true;
    bool useSelectionBG = true;
    bool useSelectionText = true;
    bool useSelectionValue = false;
    bool useBottomLine = false;
    bool useUltrahandVersion = false;
    bool useDividerLines = false;
    bool useEdgeLine = false;

    std::atomic<bool> noClickableItems{false};
    
    #if IS_LAUNCHER_DIRECTIVE
    std::atomic<bool> overlayLaunchRequested{false};
    std::string requestedOverlayPath;
    std::string requestedOverlayArgs;
    std::mutex overlayLaunchMutex;
    void (*openCommandInvokedCallback)() = nullptr;
    #endif
    
    // CUSTOM SECTION START
    std::atomic<float> backWidth;
    std::atomic<float> selectWidth;
    std::atomic<float> nextPageWidth;
    std::atomic<bool> inMainMenu{false};
    std::atomic<bool> inHiddenMode{false};
    std::atomic<bool> inSettingsMenu{false};
    std::atomic<bool> inSubSettingsMenu{false};
    std::atomic<bool> inOverlaysPage{false};
    std::atomic<bool> inPackagesPage{false};

    std::atomic<bool> hasNextPageButton{false};
    
    bool firstBoot = true; // for detecting first boot
    bool reloadingBoot = false; // for detecting reloading boots
    
    // Define an atomic bool for interpreter completion
    std::atomic<bool> threadFailure(false);
    std::atomic<bool> runningInterpreter(false);
    std::atomic<bool> shakingProgress(true);
    
    std::atomic<bool> isHidden(false);
    std::atomic<bool> externalAbortCommands(false);
    
    bool disableTransparency = false;
    bool useOpaqueScreenshots = false;
    
    std::atomic<bool> onTrackBar(false);
    std::atomic<bool> allowSlide(false);
    std::atomic<bool> unlockedSlide(false);
    
    

    void atomicToggle(std::atomic<bool>& b) {
        bool expected = b.load(std::memory_order_relaxed);
        for (;;) {
            const bool desired = !expected;
            if (b.compare_exchange_weak(expected, desired,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
                break; // success
            }
            // expected has been updated with the current value on failure; loop continues
        }
    }

    
    bool updateMenuCombos = false;


    const std::array<KeyInfo, 18> KEYS_INFO = {{
        { HidNpadButton_L, "L", "\uE0E4" }, { HidNpadButton_R, "R", "\uE0E5" },
        { HidNpadButton_ZL, "ZL", "\uE0E6" }, { HidNpadButton_ZR, "ZR", "\uE0E7" },
        { HidNpadButton_AnySL, "SL", "\uE0E8" }, { HidNpadButton_AnySR, "SR", "\uE0E9" },
        { HidNpadButton_Left, "DLEFT", "\uE0ED" }, { HidNpadButton_Up, "DUP", "\uE0EB" },
        { HidNpadButton_Right, "DRIGHT", "\uE0EE" }, { HidNpadButton_Down, "DDOWN", "\uE0EC" },
        { HidNpadButton_A, "A", "\uE0E0" }, { HidNpadButton_B, "B", "\uE0E1" },
        { HidNpadButton_X, "X", "\uE0E2" }, { HidNpadButton_Y, "Y", "\uE0E3" },
        { HidNpadButton_StickL, "LS", "\uE08A" }, { HidNpadButton_StickR, "RS", "\uE08B" },
        { HidNpadButton_Minus, "MINUS", "\uE0B6" }, { HidNpadButton_Plus, "PLUS", "\uE0B5" }
    }};

    // The glyph for a button name ("ZL" -> its icon), or nullptr. KEYS_INFO's names are
    // unique, so this is what a name -> glyph map would give, without building one.
    static const char* buttonGlyph(const std::string& name) {
        for (const auto& keyInfo : KEYS_INFO)
            if (name == keyInfo.name) return keyInfo.glyph;
        return nullptr;
    }
    
    
    void convertComboToUnicode(std::string& combo) {
        // Quick check to see if the string contains a '+'
        if (combo.find('+') == std::string::npos) {
            return;  // No '+' found, nothing to modify
        }

        // Exit early if the combo contains any spaces
        if (combo.find(' ') != std::string::npos) {
            return;  // Spaces found, return without modifying
        }
        
        std::string unicodeCombo;
        bool modified = false;
        size_t start = 0;
        const size_t length = combo.length();
        size_t end = 0;  // Moved outside the loop
        std::string token;  // Moved outside the loop
        
        // Iterate through the combo string and split by '+'
        for (size_t i = 0; i <= length; ++i) {
            if (i == length || combo[i] == '+') {
                // Get the current token (trimmed)
                end = i;  // Reuse the end variable
                while (start < end && std::isspace(combo[start])) start++;  // Trim leading spaces
                while (end > start && std::isspace(combo[end - 1])) end--;  // Trim trailing spaces
                
                token = combo.substr(start, end - start);  // Reuse the token variable
                const char* const glyph = buttonGlyph(token);
                
                if (glyph) {
                    unicodeCombo += glyph;  // Append the mapped Unicode value
                    modified = true;
                } else {
                    unicodeCombo += token;  // Append the original token if not found
                }
                
                if (i != length) {
                    unicodeCombo += "+";  // Only append '+' if we're not at the end
                }
                
                start = i + 1;  // Move to the next token
            }
        }
        
        // If a modification was made, update the original combo
        if (modified) {
            combo = unicodeCombo;
        }
    }
    
    
    CONSTEXPR_STRING std::string whiteColor = "FFFFFF";
    CONSTEXPR_STRING std::string blackColor = "000000";
    CONSTEXPR_STRING std::string greyColor = "AAAAAA";
    
    std::atomic<bool> languageWasChanged{false};

    // --- Variable declarations (no inline init = no constructor code per-variable) ---
    #if IS_LAUNCHER_DIRECTIVE
    std::string ENGLISH;
    std::string SPANISH;
    std::string FRENCH;
    std::string GERMAN;
    std::string JAPANESE;
    std::string KOREAN;
    std::string ITALIAN;
    std::string DUTCH;
    std::string PORTUGUESE;
    std::string RUSSIAN;
    std::string UKRAINIAN;
    std::string POLISH;
    std::string SIMPLIFIED_CHINESE;
    std::string TRADITIONAL_CHINESE;
    std::string OVERLAYS;
    std::string OVERLAYS_ABBR;
    std::string OVERLAY;
    std::string HIDDEN_OVERLAYS;
    std::string PACKAGES;
    std::string PACKAGE;
    std::string HIDDEN_PACKAGES;
    std::string HIDDEN;
    std::string HIDE_OVERLAY;
    std::string HIDE_PACKAGE;
    std::string LAUNCH_ARGUMENTS;
    std::string FORCE_AMS110_SUPPORT;
    std::string QUICK_LAUNCH;
    std::string BOOT_COMMANDS;
    std::string EXIT_COMMANDS;
    std::string ERROR_LOGGING;
    std::string COMMANDS;
    std::string SETTINGS;
    std::string FAVORITE;
    std::string MAIN_SETTINGS;
    std::string UI_SETTINGS;
    std::string WIDGET;
    std::string WIDGET_ITEMS;
    std::string WIDGET_SETTINGS;
    std::string CLOCK;
    std::string BATTERY;
    std::string SOC_TEMPERATURE;
    std::string PCB_TEMPERATURE;
    std::string BACKDROP;
    std::string BORDER;
    std::string DYNAMIC_BORDER;
    std::string DYNAMIC_TEMPS;
    std::string CENTER_ALIGNMENT;
    std::string EXTENDED_BACKDROP;
    std::string MISCELLANEOUS;

    std::string INPUT_SETTINGS;
    std::string LAUNCH_COMBOS;
    std::string SWIPE_TO_OPEN;
    std::string BUTTON_HAPTICS;
    std::string TOUCH_HAPTICS;
    #if IS_LAUNCHER_DIRECTIVE
    std::string HOME_SHORTCUT;
    std::string NO_CONTROLLER_SLOT;
    #endif
    std::string STICK_NAVIGATION;
    std::string HOLD_DURATION;

    std::string FEATURE_SETTINGS;
    std::string OPAQUE_SCREENSHOTS;
    std::string RIGHT_SIDE_MODE;
    std::string NTP_SYNC_DOWNLOADS;

    std::string MENU_SETTINGS;
    std::string PACKAGES_MENU;
    std::string USER_GUIDE;
    std::string SHOW_HIDDEN;
    std::string SHOW_DELETE;
    std::string SHOW_UNSUPPORTED;
    std::string PAGE_SWAP;
    std::string OVERLAY_VERSIONS;
    std::string ULTRAHAND_VERSION;
    std::string PACKAGE_VERSIONS;
    std::string CLEAN_VERSIONS;

    std::string THEME_SETTINGS;
    std::string VERSION_SETTINGS;
    std::string LABEL_COLORS;
    std::string SWITCH_2_STYLE;
    std::string UNIFORM_LOGO;
    std::string TEXT_BORDER;
    std::string LIGHT;
    std::string COLOR;
    std::string EXPOSURE;
    std::string BRIGHTNESS;
    std::string HIGHLIGHTS;
    std::string SHADOWS;
    std::string CONTRAST;
    std::string BLACK_POINT;
    std::string SATURATION;
    std::string VIBRANCE;
    std::string WARMTH;
    std::string TINT;
    std::string TRANSPARENCY;
    std::string PRESETS;
    std::string TOOLS;
    std::string RESTORE_DEFAULTS;
    std::string RESTORE_DEFAULTS_INFO;
    std::string RESTORE_ROOT_PKG;
    std::string RESTORE_ROOT_PKG_INFO;
    std::string CLEAN_INIS;
    std::string CLEAN_INIS_INFO;
    std::string DOT_CLEAN_SD;
    std::string DOT_CLEAN_SD_INFO;
    std::string NTP_SYNC_TIME;
    std::string NTP_SYNC_TIME_INFO;
    std::string BLUR;
    std::string DARKNESS;
    std::string RED;
    std::string GREEN;
    std::string BLUE;
    std::string ENABLE;
    std::string USE_BACKGROUND_COLOR;
    std::string RESET;
    std::string ALL;
    std::string IMAGES;
    std::string VIDEOS;
    std::string FROSTED_GLASS;
    std::string GLASS_WITH_CHEATS;
    std::string GLASS_CHEATS_WARNING;
    std::string DYNAMIC_LOGO;
    std::string DYNAMIC_TABLES;
    std::string SELECTION_BACKGROUND;
    std::string SELECTION_TEXT;
    std::string SELECTION_VALUE;
    std::string BOTTOM_LINE;
    std::string DIVIDER_LINES;
    std::string EDGE_LINE;
    std::string LIBULTRAHAND_TITLES;
    std::string LIBULTRAHAND_VERSIONS;
    std::string PACKAGE_TITLES;
    std::string IN_PACKAGE_TITLES;

    std::string KEY_COMBO;
    std::string MODE;
    std::string LAUNCH_MODES;
    std::string LANGUAGE;
    std::string OVERLAY_INFO;
    std::string SOFTWARE_UPDATE;
    std::string UPDATE_ULTRAHAND;
    std::string SYSTEM;
    std::string DEVICE_INFO;
    std::string FIRMWARE;
    std::string BOOTLOADER;
    std::string HARDWARE;
    std::string MEMORY;
    std::string VENDOR;
    std::string MODEL;
    std::string STORAGE;
    std::string OVERLAY_MEMORY;
    std::string NOT_ENOUGH_MEMORY;
    std::string WALLPAPER_SUPPORT_DISABLED;
    //std::string SOUND_SUPPORT_DISABLED;
    std::string WALLPAPER_SUPPORT_ENABLED;
    std::string SOUND_SUPPORT_ENABLED;
    std::string FULL_COLOR_SUPPORT_ENABLED;
    std::string FULL_COLOR_SUPPORT_DISABLED;
    std::string EXIT_OVERLAY_SYSTEM;
    std::string ULTRAHAND_ABOUT;
    std::string ULTRAHAND_CREDITS_START;
    std::string ULTRAHAND_CREDITS_END;
    std::string LOCAL_IP;
    std::string WALLPAPER;
    std::string THEME;
    std::string SOUNDS;
    std::string DEFAULT;
    std::string ROOT_PACKAGE;
    std::string SORT_PRIORITY;
    std::string OPTIONS;
    std::string FAILED_TO_OPEN;

    std::string NOTIFICATIONS;
    std::string NOTIFICATION_SETTINGS;
    std::string SILENCE_NOTIFICATIONS;
    std::string STARTUP_NOTIFICATION;
    std::string MAX_SLOTS;
    std::string API_NOTIFICATIONS;
    std::string API_TOGGLE_HOTKEY;
    std::string DISMISS_NOTIFICATION;
    std::string API_TOGGLE;
    std::string CLICK;
    std::string TAP;
    std::string HOLD_FOR_4S;

    std::string PACKAGE_INFO;
    std::string _TITLE;
    std::string _VERSION;
    std::string _CREATOR;
    std::string _ABOUT;
    std::string _CREDITS;
    std::string SETTINGS_MENU;
    std::string SCRIPT_OVERLAY;
    std::string STAR_FAVORITE;
    std::string APP_SETTINGS;
    std::string ON_MAIN_MENU;
    std::string ON_A_COMMAND;
    std::string ON_OVERLAY_PACKAGE;
    std::string ULTRAHAND_HAS_STARTED;
    std::string ULTRAHAND_HAS_RESTARTED;
    std::string NEW_UPDATE_IS_AVAILABLE;
    std::string DELETE_PACKAGE;
    std::string DELETE_OVERLAY;
    std::string SELECTION_IS_EMPTY;
    std::string FORCED_SUPPORT_WARNING;
    std::string TASK_IS_COMPLETE;
    std::string TASK_HAS_FAILED;
    std::string REBOOT_TO;
    std::string REBOOT;
    std::string SHUTDOWN;
    std::string BOOT_ENTRY;
    std::string INI_ENTRY;
    #endif

    std::string INCOMPATIBLE_WARNING;
    std::string OVERLAY_DOES_NOT_EXIST;
    std::string SYSTEM_RAM;
    std::string FREE;
    std::string UNAVAILABLE_SELECTION;
    std::string ON;
    std::string OFF;
    std::string OK;
    std::string BACK;
    std::string HIDE;
    std::string CANCEL;
    std::string GAP_1;
    std::string GAP_2;
    std::string API_NOTIFICATIONS_ENABLED;
    std::string API_NOTIFICATIONS_DISABLED;

    std::atomic<float> halfGap = 0.0f;

    #if USING_WIDGET_DIRECTIVE
    std::string SUNDAY;
    std::string MONDAY;
    std::string TUESDAY;
    std::string WEDNESDAY;
    std::string THURSDAY;
    std::string FRIDAY;
    std::string SATURDAY;
    std::string JANUARY;
    std::string FEBRUARY;
    std::string MARCH;
    std::string APRIL;
    std::string MAY;
    std::string JUNE;
    std::string JULY;
    std::string AUGUST;
    std::string SEPTEMBER;
    std::string OCTOBER;
    std::string NOVEMBER;
    std::string DECEMBER;
    std::string SUN;
    std::string MON;
    std::string TUE;
    std::string WED;
    std::string THU;
    std::string FRI;
    std::string SAT;
    std::string JAN;
    std::string FEB;
    std::string MAR;
    std::string APR;
    std::string MAY_ABBR;
    std::string JUN;
    std::string JUL;
    std::string AUG;
    std::string SEP;
    std::string OCT;
    std::string NOV;
    std::string DEC;
    #endif


    // Language table (lang_table.hpp). LANG_STRS holds "KEY\0Default\0" for each entry,
    // LANG_VARS the matching variables, in the same order.
    #define LANG(var, def) #var "\0" def "\0"
    static constexpr char LANG_STRS[] =
        #include "lang_table.hpp"
        ;
    #undef LANG

    #define LANG(var, def) &var,
    static std::string* const LANG_VARS[] = {
        #include "lang_table.hpp"
    };
    #undef LANG


    // Sets every language string to its English default.
    void reinitializeLangVars() {
        const char* p = LANG_STRS;
        for (std::string* var : LANG_VARS) {
            p += strlen(p) + 1;  // skip the key
            *var = p;
            p += strlen(p) + 1;
        }
    }


    // Loads a language JSON. Keys it doesn't have keep the English default.
    void parseLanguage(const std::string& langFile) {
        std::unordered_map<std::string, std::string> jsonMap;
        if (!parseJsonToMap(langFile, jsonMap)) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to parse language file: " + langFile);
            #endif
            return;
        }
        const char* p = LANG_STRS;
        for (std::string* var : LANG_VARS) {
            auto it = jsonMap.find(p);
            p += strlen(p) + 1;
            *var = (it != jsonMap.end()) ? it->second : p;
            p += strlen(p) + 1;
        }
    }


    #if USING_WIDGET_DIRECTIVE
    void localizeTimeStr(char* timeStr) {
        static const struct { const char* key; std::string* val; } mappings[] = {
            {"Sunday",    &SUNDAY},    {"Monday",    &MONDAY},    {"Tuesday",   &TUESDAY},
            {"Wednesday", &WEDNESDAY}, {"Thursday",  &THURSDAY},  {"Friday",    &FRIDAY},
            {"Saturday",  &SATURDAY},
            {"January",   &JANUARY},   {"February",  &FEBRUARY},  {"March",     &MARCH},
            {"April",     &APRIL},     {"June",      &JUNE},      {"July",      &JULY},
            {"August",    &AUGUST},    {"September", &SEPTEMBER}, {"October",   &OCTOBER},
            {"November",  &NOVEMBER},  {"December",  &DECEMBER},
            {"Sun", &SUN}, {"Mon", &MON}, {"Tue", &TUE}, {"Wed", &WED},
            {"Thu", &THU}, {"Fri", &FRI}, {"Sat", &SAT},
            {"Jan", &JAN}, {"Feb", &FEB}, {"Mar", &MAR}, {"Apr", &APR},
            {"May", &MAY_ABBR}, {"Jun", &JUN}, {"Jul", &JUL}, {"Aug", &AUG},
            {"Sep", &SEP}, {"Oct", &OCT}, {"Nov", &NOV}, {"Dec", &DEC},
        };
    
        std::string result = timeStr;
        for (const auto& m : mappings) {
            size_t pos = 0;
            const size_t keyLen = strlen(m.key);
            while ((pos = result.find(m.key, pos)) != std::string::npos) {
                result.replace(pos, keyLen, *m.val);
                pos += m.val->size();
            }
        }
        strcpy(timeStr, result.c_str());
    }
    #endif

    void applyLangReplacements(std::string& text, bool isValue) {
        if (isValue) {
            if (text.length() == 2 && text[0] == 'O' && text[1] == 'n') { text = ON; return; }
            if (text.length() == 3 && text[0] == 'O' && text[1] == 'f' && text[2] == 'f') { text = OFF; return; }
        }
    #if IS_LAUNCHER_DIRECTIVE
        else {
            switch (text.length()) {
                case 6:
                    if (text == "Reboot") { text = REBOOT; }
                    break;
                case 8:
                    if (text == "Shutdown") { text = SHUTDOWN; }
                    break;
                case 9:
                    if (text == "Reboot To") { text = REBOOT_TO; }
                    else if (text == "INI Entry") { text = INI_ENTRY; }
                    break;
                case 10:
                    if (text == "Boot Entry") { text = BOOT_ENTRY; }
                    break;
            }
        }
    #endif
    }
    
    
    
    std::atomic<bool> refreshWallpaperNow(false);
    std::atomic<bool> refreshWallpaper(false);
    std::atomic<bool> refreshCombos(false);
    WallpaperBuffer wallpaperData;

    bool WallpaperBuffer::resize(size_t n) {
        if (n > m_cap) {
            release();
            m_data = static_cast<u8*>(glass::sharedAlloc(glass::PoolUser::Wallpaper, n));
            if (!m_data) return false;
            m_cap = n;
        }
        if (n > m_size) std::memset(m_data + m_size, 0, n - m_size);
        m_size = n;
        return true;
    }

    void WallpaperBuffer::release() {
        glass::sharedFree(m_data);
        m_data = nullptr;
        m_size = m_cap = 0;
    }
    bool fullColorWallpaper = false;
    std::atomic<u32> wallpaperGeneration{0};
    std::atomic<bool> inPlot(false);
    
    std::mutex wallpaperMutex;
    std::atomic<u64> glassStartPauseUntilNs{0};
    std::condition_variable cv;
    
    bool loadRGBA8888toRGBA4444(const std::string& filePath, u8* dst, size_t srcSize, const WallAdj* adj) {
        FILE* f = fopen(filePath.c_str(), "rb");
        if (!f) return false;
    
        const uint8x8_t mask = vdup_n_u8(0xF0);
        constexpr size_t chunkBytes = 128 * 1024;
        uint8_t chunkBuffer[chunkBytes];
        size_t totalRead = 0;
    
        setvbuf(f, nullptr, _IOFBF, chunkBytes);
    
        while (totalRead < srcSize) {
            const size_t toRead = std::min(srcSize - totalRead, chunkBytes);
            const size_t bytesRead = fread(chunkBuffer, 1, toRead, f);
            if (bytesRead == 0) { fclose(f); return false; }
    
            const uint8_t* src = chunkBuffer;
            size_t i = 0;
            if (adj && adj->on) {                       // 8 px: adjust the 8-bit RGB, then 4444
                for (; i + 32 <= bytesRead; i += 32) {
                    uint8x8x4_t v = vld4_u8(src + i);
                    uint8x8x3_t c{{ v.val[0], v.val[1], v.val[2] }};
                    wallAdj8(*adj, c);
                    const uint8x8_t lo = vorr_u8(vand_u8(c.val[0], mask), vshr_n_u8(c.val[1], 4));
                    const uint8x8_t hi = vorr_u8(vand_u8(c.val[2], mask), vshr_n_u8(v.val[3], 4));
                    vst2_u8(dst, uint8x8x2_t{{ lo, hi }});
                    dst += 16;
                }
            }
            for (; i + 16 <= bytesRead; i += 16) {
                uint8x16_t data = vld1q_u8(src + i);
                uint8x8x2_t sep = vuzp_u8(vget_low_u8(data), vget_high_u8(data));
                vst1_u8(dst, vorr_u8(vand_u8(sep.val[0], mask), vshr_n_u8(sep.val[1], 4)));
                dst += 8;
            }
            for (; i + 1 < bytesRead; i += 2)
                *dst++ = (src[i] & 0xF0) | (src[i+1] >> 4);
    
            totalRead += bytesRead;
        }
    
        fclose(f);
        return true;
    }

    
    // Frosted glass replaces the wallpaper entirely: while it is on, the wallpaper
    // is never read from the SD card and any loaded copy is freed (~630 KB).
    // Turning glass off reloads it (see reloadWallpaper / the Frosted Glass toggle).
    static inline bool glassReplacesWallpaper() {
    #if IS_STATUS_MONITOR_DIRECTIVE
        return false;   // Status Monitor: Full mode has no glass and still draws the wallpaper
    #else
        return useFrostedGlass && !limitedMemory && !glassFallback.load(std::memory_order_acquire);
    #endif
    }
    // The wallpaper is about to change: flags it (refreshWallpaper) and waits, locked, until no
    // frame is drawing it. The lock is the caller's for the change.
    static std::unique_lock<std::mutex> beginWallpaperChange() {
        refreshWallpaper.store(true, std::memory_order_release);
        std::unique_lock<std::mutex> lock(wallpaperMutex);
        cv.wait(lock, [] { return !inPlot.load(std::memory_order_acquire); });
        return lock;
    }

    static inline void freeWallpaperData() {   // (an animated wallpaper stops with it)
        anim::stop();
        wallpaperData.release();
        setWallpaperBaked(kNoWallAdjust);
        wallpaperGeneration.fetch_add(1, std::memory_order_acq_rel);
    }

    void loadWallpaperFile(const std::string& filePath, s32 width, s32 height) {
        if (glassReplacesWallpaper()) { freeWallpaperData(); return; }
        const size_t srcSize = width * height * 4;
    #if USING_LIVE_LOGGING_DIRECTIVE
        // load time (SD read + conversion), one line per load
        struct LoadTimer {
            u64 t0 = ult::glassLog ? armGetSystemTick() : 0;
            ~LoadTimer() {
                if (ult::glassLog)
                    glass::live::log("wallpaper: loaded in %lu us (%s)",
                                     (unsigned long)(armTicksToNs(armGetSystemTick() - t0) / 1000),
                                     fullColorWallpaper ? "8-bit RGB" : "RGBA4444");
            }
        } loadTimer;
    #endif
        if (fullColorWallpaper) {
            // Full-colour output (> 8 MB heap): keep 8 bits per channel, but store only
            // what is drawn: pre-multiplied RGB, 3 bytes/px (968 KB instead of 1.29 MB for
            // RGBA8888). The frame's alpha comes from the theme, so the wallpaper's alpha is
            // only needed here. Converted in 32 KB chunks while reading, with no full-size
            // temporary. composeFullColor() draws it.
            const size_t px = static_cast<size_t>(width) * height;
            if (!wallpaperData.resize(px * 3)) { freeWallpaperData(); return; }   // no memory: no wallpaper
            FILE* f = fopen(filePath.c_str(), "rb");
            // 32 KB read chunk (8192 px), only for the duration of the load
            u8* const chunk = static_cast<u8*>(aligned_alloc(16, 32768));
            bool ok = f != nullptr && chunk != nullptr;
            u8* dst = wallpaperData.data();
            size_t done = 0;
            while (ok && done < px) {
                const size_t want = std::min<size_t>(px - done, 32768 / 4);
                if (fread(chunk, 4, want, f) != want) { ok = false; break; }
                size_t i = 0;
                for (; i + 8 <= want; i += 8) {
                    const uint8x8x4_t v = vld4_u8(chunk + i * 4);
                    auto pm = [&](uint8x8_t c) {                  // c * a / 255, rounded
                        const uint16x8_t t = vmull_u8(c, v.val[3]);
                        return vrshrn_n_u16(vsraq_n_u16(t, t, 8), 8);
                    };
                    vst3_u8(dst + (done + i) * 3, uint8x8x3_t{{ pm(v.val[0]), pm(v.val[1]), pm(v.val[2]) }});
                }
                for (; i < want; ++i) {
                    const u8* sp = chunk + i * 4; u8* dp = dst + (done + i) * 3;
                    for (int c = 0; c < 3; ++c) { const u32 t = sp[c] * sp[3]; dp[c] = static_cast<u8>((t + (t >> 8) + 128) >> 8); }
                }
                done += want;
            }
            if (f) fclose(f);
            free(chunk);
            if (!ok) freeWallpaperData();
            else { bakeLoadedWallpaper(); wallpaperGeneration.fetch_add(1, std::memory_order_acq_rel); }
            return;
        }
        // RGBA4444 (6 / 8 MB): the adjustments for this colour mode (wallpaperAdjustSection)
        // go in as the file is converted, before the colour is cut to 4 bits.
        s8 t[WA_COUNT];
        wallpaperAdjustTarget(t);
        setWallpaperBaked(t);
        if (!wallpaperData.resize(srcSize / 2) || !isFile(filePath) ||
            !loadRGBA8888toRGBA4444(filePath, wallpaperData.data(), srcSize,
                                    wallpaperBakedAdj.on ? &wallpaperBakedAdj : nullptr)) {
            wallpaperData.release();
            setWallpaperBaked(kNoWallAdjust);
        }
        wallpaperGeneration.fetch_add(1, std::memory_order_acq_rel);
    }
    
    const std::string& wallpaperIniPath() {
        static const std::string p = SETTINGS_PATH + "wallpaper.ini";
        return p;
    }
    std::string currentWallpaperName;
    const char* const kWallpaperAdjustKeys[WA_COUNT] = {
        "exposure", "brightness", "highlights", "shadows", "contrast", "black_point",
        "saturation", "vibrance", "warmth", "tint"
    };
    std::atomic<s8> wallpaperAdjust[WA_COUNT] = {};
    std::atomic<s8> wallpaperAlpha{-1};

    using WallpaperIni = std::map<std::string, std::map<std::string, std::string>>;
    static const std::string* iniValue(const WallpaperIni& ini, const std::string& section, const char* key) {
        const auto sec = ini.find(section);
        if (sec == ini.end()) return nullptr;
        const auto it = sec->second.find(key);
        return (it == sec->second.end() || it->second.empty()) ? nullptr : &it->second;
    }
    std::string wallpaperAdjustSection(const std::string& name) {
        return name + (fullColorWallpaper ? " - full" : " - limited");
    }
    // wallpaperAdjust[] = wallpaper.ini [<current_wallpaper>] (or its 4-bit section; 0 where missing),
    // and wallpaperAlpha from the same section (default kDefaultWallpaperAlpha).
    static void setAdjustFromIni(const WallpaperIni& ini) {
        const std::string section = currentWallpaperName.empty() ? std::string() : wallpaperAdjustSection(currentWallpaperName);
        for (u32 i = 0; i < WA_COUNT; ++i) {
            int v = 0;
            if (!section.empty())
                if (const std::string* sv = iniValue(ini, section, kWallpaperAdjustKeys[i])) v = ult::stoi(*sv);
            wallpaperAdjust[i].store(static_cast<s8>(std::max(-100, std::min(v, 100))), std::memory_order_relaxed);
        }
        int a = -1;                                   // no named wallpaper: the theme's alpha
        if (!section.empty()) {
            a = kDefaultWallpaperAlpha;
            if (const std::string* sv = iniValue(ini, section, "alpha")) a = std::max(0, std::min(ult::stoi(*sv), 15));
        }
        wallpaperAlpha.store(static_cast<s8>(a), std::memory_order_relaxed);
    }

    // Glass memory and wallpaper take turns in one block (glass::sharedAlloc): with glass off
    // and a wallpaper about to load, the glass memory goes first (it is taken back when glass
    // is switched on again).
    static inline void releaseGlassForWallpaper() {
    #if !IS_STATUS_MONITOR_DIRECTIVE
        if (!limitedMemory) glass::release();
    #endif
    }

    // Loads the current wallpaper (caller holds wallpaperMutex): a still image
    // wins, else the animated one named in wallpaper.ini. Also sets its adjustments from
    // wallpaper.ini [<current_wallpaper>] (baked in by the load). One small file read.
    // resume: an animated wallpaper may continue from animated.cache. readAdjust false: keep
    // wallpaperAdjust[] as it is (a value still changing, not saved to wallpaper.ini yet).
    static void loadCurrentWallpaper(bool resume = true, bool readAdjust = true) {
        releaseGlassForWallpaper();
        const auto ini = getParsedDataFromIniFile(wallpaperIniPath());
        if (readAdjust) setAdjustFromIni(ini);
        // An overlay's own wallpaper.uaw (UI override folder) comes first.
        if (!WALLPAPER_ANIM_OVERRIDE.empty() && anim::startFromPath(WALLPAPER_ANIM_OVERRIDE, resume)) return;
        if (isFile(WALLPAPER_PATH)) loadWallpaperFile(WALLPAPER_PATH);
        else if (const std::string* animPath = iniValue(ini, "wallpaper", "animated")) anim::startFromPath(*animPath, resume);
    }

    // ---- wallpaper adjustments: where they are applied (image_fx.hpp) ----
    std::atomic<bool> wallpaperAdjustLive{false};
    std::atomic<bool> wallpaperAdjustSpread{false};
    s8 wallpaperBaked[WA_COUNT] = {};
    WallAdj wallpaperBakedAdj{};

    void wallAdjBuffer(const WallAdj& wa, u8* rgb, size_t px) {
        if (!wa.on) return;
        for (size_t i = 0; i + 8 <= px; i += 8) {
            uint8x8x3_t w = vld3_u8(rgb + i * 3);
            wallAdj8(wa, w);
            vst3_u8(rgb + i * 3, w);
        }
    }

    void wallAdjBuffer4444(const WallAdj& wa, u8* p, size_t px) {
        if (!wa.on) return;
        for (size_t i = 0; i + 8 <= px; i += 8) {
            uint8x8x2_t v = vld2_u8(p + i * 2);
            wallAdj4444(wa, v.val[0], v.val[1]);
            vst2_u8(p + i * 2, v);
        }
    }

    void wallpaperAdjustTarget(s8 (&out)[WA_COUNT]) {
        const bool use = !wallpaperAdjustLive.load(std::memory_order_acquire);   // (both colour modes)
        for (u32 i = 0; i < WA_COUNT; ++i) out[i] = use ? wallpaperAdjust[i].load(std::memory_order_relaxed) : 0;
    }

    void setWallpaperBaked(const s8 (&v)[WA_COUNT]) {
        memcpy(wallpaperBaked, v, sizeof(wallpaperBaked));
        wallAdjSetup(wallpaperBakedAdj, v);
    }

    static constexpr size_t kFullWallBytes = size_t(448) * 720 * 3;
    static bool anyAdjustValues(const s8 (&v)[WA_COUNT]) {
        for (u32 i = 0; i < WA_COUNT; ++i) if (v[i]) return true;
        return false;
    }

    void bakeLoadedWallpaper() {
        s8 t[WA_COUNT];
        wallpaperAdjustTarget(t);
        if (wallpaperData.size() != kFullWallBytes) { setWallpaperBaked(kNoWallAdjust); return; }
        setWallpaperBaked(t);
        wallAdjBuffer(wallpaperBakedAdj, wallpaperData.data(), kFullWallBytes / 3);
    }

    // Runs f with the wallpaper to itself (same handshake as reloadWallpaper: no frame
    // reads it meanwhile, the animated wallpaper's decoder skips), then re-composes.
    template <typename F>
    static void withWallpaperLocked([[maybe_unused]] const char* what, F&& f) {
    #if USING_LIVE_LOGGING_DIRECTIVE
        const u64 t0 = ult::glassLog ? armGetSystemTick() : 0;
    #endif
        {
            auto lock = beginWallpaperChange();
            f();
            wallpaperGeneration.fetch_add(1, std::memory_order_acq_rel);
        }
        refreshWallpaper.store(false, std::memory_order_release);
        cv.notify_all();
    #if USING_LIVE_LOGGING_DIRECTIVE
        if (ult::glassLog)
            glass::live::log("wallpaper: adjustments %s in %lu us", what,
                             (unsigned long)(armTicksToNs(armGetSystemTick() - t0) / 1000));
    #endif
    }

    static bool anyAdjust(const s8 (&v)[WA_COUNT]) {
        for (u32 i = 0; i < WA_COUNT; ++i) if (v[i]) return true;
        return false;
    }

    // Brings the buffer to wallpaperAdjustTarget() (caller holds wallpaperMutex).
    // resume: an animated wallpaper that must be reloaded may continue from its state.
    static void syncLocked(bool resume) {
        s8 t[WA_COUNT];
        wallpaperAdjustTarget(t);
        if (memcmp(t, wallpaperBaked, sizeof(t)) == 0) return;
        const bool fourBit = !fullColorWallpaper && wallpaperData.size() == size_t(448) * 720 * 2;
        if (!fourBit && wallpaperData.size() != kFullWallBytes) { setWallpaperBaked(kNoWallAdjust); return; }   // nothing drawn
        if (!anyAdjust(wallpaperBaked)) {                 // plain picture: bake in place
            setWallpaperBaked(t);
            if (fourBit) wallAdjBuffer4444(wallpaperBakedAdj, wallpaperData.data(), size_t(448) * 720);
            else         wallAdjBuffer(wallpaperBakedAdj, wallpaperData.data(), kFullWallBytes / 3);
            return;
        }
        // Something else is baked in: the plain wallpaper again (a still one from the SD card,
        // an animated one from frame 0), with the target baked in by the load: the current
        // wallpaperAdjust[].
        loadCurrentWallpaper(resume, false);
    }

    void beginWallpaperAdjustEdit() {
        if (wallpaperAdjustLive.load(std::memory_order_acquire)) return;
        wallpaperAdjustSpread.store(fullColorWallpaper && glass::gameOutOfFocus(), std::memory_order_release);   // (4-bit: drawWallpaper is spread already)
        withWallpaperLocked("live preview on", [] {
            wallpaperAdjustLive.store(true, std::memory_order_release);
            syncLocked(false);                            // target is now "none"
        });
    }

    void endWallpaperAdjustEdit() {
        if (!wallpaperAdjustLive.load(std::memory_order_acquire)) return;
        withWallpaperLocked("baked in", [] {
            wallpaperAdjustLive.store(false, std::memory_order_release);
            syncLocked(false);                            // plain buffer: baked in place
        });
    }

    void syncWallpaperAdjust(bool resume) {
        s8 t[WA_COUNT];
        wallpaperAdjustTarget(t);
        if (memcmp(t, wallpaperBaked, sizeof(t)) == 0) return;
        if (resume) withWallpaperLocked("synced", [] { syncLocked(true); });
        else        withWallpaperLocked("applied (4-bit)", [] { syncLocked(false); });
    }

    #if IS_LAUNCHER_DIRECTIVE
    // Minimal PNG -> RGBA8888 converter for wallpapers (streaming; zlib's inflate, already
    // linked, does the decompression). The output is a plain .rgba file, so loading a PNG
    // wallpaper later is exactly as fast as loading any other wallpaper.
    // Supported: 8-bit grey / grey+alpha / RGB / RGBA / palette (with tRNS alpha), 16-bit
    // grey / grey+alpha / RGB / RGBA (high byte kept), non-interlaced, exactly
    // width x height. Anything else returns false and leaves outPath untouched.
    // Speed: rows are inflated in blocks of ~32 KB (zlib stays on its fast path), the
    // row filters and pixel conversion use NEON, and the file is read through one 32 KB
    // buffer (chunk headers / CRCs never cost an SD read of their own).
    // Temporary memory ~110 KB (zlib state + 32 KB window, row block, output, input).
    namespace png {
        // Unfilter one pixel-serial row (Sub / Average / Paeth) for bpp 3, 4, 6 or 8:
        // the bpp bytes of a pixel are one NEON vector, pixels go left to right.
        template <u32 kBpp, u8 kFilter>
        static void unfilterPixels(u8* x, const u8* p, u32 rowBytes) {
            auto ld = [](const u8* s) { u64 v = 0; memcpy(&v, s, kBpp); return vcreate_u8(v); };
            auto st = [](u8* d, uint8x8_t v) { const u64 t = vget_lane_u64(vreinterpret_u64_u8(v), 0); memcpy(d, &t, kBpp); };
            uint8x8_t a = vdup_n_u8(0), c = vdup_n_u8(0);   // left / upper-left (0 for the first pixel)
            for (u32 i = 0; i < rowBytes; i += kBpp) {
                const uint8x8_t raw = ld(x + i);
                uint8x8_t pred;
                if constexpr (kFilter == 1) {
                    pred = a;
                } else if constexpr (kFilter == 3) {
                    pred = vhadd_u8(a, ld(p + i));                  // floor((a + b) / 2)
                } else {
                    const uint8x8_t b = ld(p + i);
                    const uint16x8_t pa = vabdl_u8(b, c);           // |p - a| = |b - c|
                    const uint16x8_t pb = vabdl_u8(a, c);           // |p - b| = |a - c|
                    const uint16x8_t pc = vabdq_u16(vaddl_u8(a, b), vshll_n_u8(c, 1));   // |a + b - 2c|
                    const uint8x8_t useA = vmovn_u16(vandq_u16(vcleq_u16(pa, pb), vcleq_u16(pa, pc)));
                    const uint8x8_t useB = vmovn_u16(vcleq_u16(pb, pc));
                    pred = vbsl_u8(useA, a, vbsl_u8(useB, b, c));
                    c = b;
                }
                a = vadd_u8(raw, pred);
                st(x + i, a);
            }
        }

        // Undo the row filter (cur[-1] = filter type) of row x against the previous row p.
        static bool unfilterRow(u8 filter, u8* x, const u8* p, u32 rowBytes, u32 bpp) {
            switch (filter) {
                case 0: return true;
                case 2: {
                    u32 i = 0;
                    for (; i + 16 <= rowBytes; i += 16) vst1q_u8(x + i, vaddq_u8(vld1q_u8(x + i), vld1q_u8(p + i)));
                    for (; i < rowBytes; ++i) x[i] += p[i];
                    return true;
                }
                case 1: case 3: case 4: break;
                default: return false;
            }
            #define PNG_UNFILTER_BPP(B) \
                if (bpp == B) { \
                    if (filter == 1) unfilterPixels<B, 1>(x, p, rowBytes); \
                    else if (filter == 3) unfilterPixels<B, 3>(x, p, rowBytes); \
                    else unfilterPixels<B, 4>(x, p, rowBytes); \
                    return true; \
                }
            PNG_UNFILTER_BPP(4) PNG_UNFILTER_BPP(3) PNG_UNFILTER_BPP(8) PNG_UNFILTER_BPP(6)
            #undef PNG_UNFILTER_BPP
            // bpp 1 / 2 (grey, grey + alpha): scalar
            if (filter == 1) {
                for (u32 i = bpp; i < rowBytes; ++i) x[i] += x[i - bpp];
            } else if (filter == 3) {
                for (u32 i = 0; i < bpp; ++i) x[i] += p[i] >> 1;
                for (u32 i = bpp; i < rowBytes; ++i) x[i] += (u32(x[i - bpp]) + p[i]) >> 1;
            } else {
                for (u32 i = 0; i < bpp; ++i) x[i] += p[i];
                for (u32 i = bpp; i < rowBytes; ++i) {
                    const s32 a = x[i - bpp], b = p[i], c = p[i - bpp];
                    const s32 pa = std::abs(b - c), pb = std::abs(a - c), pc = std::abs(a + b - 2 * c);
                    x[i] += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                }
            }
            return true;
        }

        // RGBA8888 pixels -> the in-memory wallpaper format, with the same arithmetic as
        // loadWallpaperFile: RGBA4444 bytes (r|g, b|a high nibbles), or (full colour)
        // pre-multiplied 8-bit RGB.
        static void toWallpaper(const u8* src, size_t px, u8* dst, bool fullColor) {
            size_t i = 0;
            if (fullColor) {
                for (; i + 8 <= px; i += 8) {
                    const uint8x8x4_t v = vld4_u8(src + i * 4);
                    auto pm = [&](uint8x8_t ch) {                   // ch * a / 255, rounded
                        const uint16x8_t t = vmull_u8(ch, v.val[3]);
                        return vrshrn_n_u16(vsraq_n_u16(t, t, 8), 8);
                    };
                    vst3_u8(dst + i * 3, uint8x8x3_t{{ pm(v.val[0]), pm(v.val[1]), pm(v.val[2]) }});
                }
                for (; i < px; ++i) {
                    const u8* sp = src + i * 4; u8* dp = dst + i * 3;
                    for (int ch = 0; ch < 3; ++ch) { const u32 t = sp[ch] * sp[3]; dp[ch] = static_cast<u8>((t + (t >> 8) + 128) >> 8); }
                }
                return;
            }
            const uint8x8_t mask = vdup_n_u8(0xF0);
            const size_t bytes = px * 4;
            for (; i + 16 <= bytes; i += 16) {
                const uint8x16_t data = vld1q_u8(src + i);
                const uint8x8x2_t sep = vuzp_u8(vget_low_u8(data), vget_high_u8(data));
                vst1_u8(dst + i / 2, vorr_u8(vand_u8(sep.val[0], mask), vshr_n_u8(sep.val[1], 4)));
            }
            for (; i + 1 < bytes; i += 2) dst[i / 2] = (src[i] & 0xF0) | (src[i + 1] >> 4);
        }
    }

    // wallDst (optional): also fill the in-memory wallpaper (width x height in the format
    // toWallpaper writes), so a new PNG wallpaper needs no read-back of the .rgba file.
    bool convertPngToRgba(const std::string& pngPath, const std::string& outPath, u32 width, u32 height, u8* wallDst) {
        struct Ctx {
            FILE* in = nullptr; FILE* out = nullptr;
            z_stream zs{}; bool zInit = false;
            u8* mem = nullptr;
            ~Ctx() { if (zInit) inflateEnd(&zs); if (in) fclose(in); if (out) fclose(out); free(mem); }
        } c;
        const std::string tmpPath = outPath + ".tmp";
        auto fail = [&]() { if (c.out) { fclose(c.out); c.out = nullptr; } std::remove(tmpPath.c_str()); return false; };
        auto be32 = [](const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]); };

        c.in = fopen(pngPath.c_str(), "rb");
        if (!c.in) return false;
        setvbuf(c.in, nullptr, _IONBF, 0);                      // our own 32 KB buffer below

        u8 hdr[8 + 8 + 13 + 4];                                 // signature, IHDR (with its CRC)
        static constexpr u8 kSig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        if (fread(hdr, 1, sizeof(hdr), c.in) != sizeof(hdr) || memcmp(hdr, kSig, 8) != 0 ||
            be32(hdr + 8) != 13 || memcmp(hdr + 12, "IHDR", 4) != 0) return false;
        const u8* ih = hdr + 16;
        const u32 w = be32(ih), h = be32(ih + 4);
        const u8 depth = ih[8], type = ih[9];
        if (w != width || h != height || ih[10] != 0 || ih[11] != 0 || ih[12] != 0) return false;   // size, method, interlace
        u32 channels;
        switch (type) {
            case 0: channels = 1; break;   // grey
            case 2: channels = 3; break;   // RGB
            case 3: channels = 1; break;   // palette
            case 4: channels = 2; break;   // grey + alpha
            case 6: channels = 4; break;   // RGBA
            default: return false;
        }
        if (!(depth == 8 || (depth == 16 && type != 3))) return false;
        const u32 bpp = channels * (depth / 8);                // bytes per pixel (filter distance)
        const u32 rowBytes = w * bpp;
        const u32 stride = rowBytes + 1;                        // filter byte + row
        const u32 kRows = std::max<u32>(1, 32768 / stride);     // rows inflated per block
        const u32 outRow = w * 4;
        const u32 wallBpp = fullColorWallpaper ? 3u : 2u;      // bytes per pixel in wallDst

        // One block: palette | previous row | row block | output rows | input buffer
        constexpr u32 kInBytes = 32768;
        const size_t memSize = 1024 + 16 + stride + size_t(kRows) * stride + 16 + size_t(kRows) * outRow + kInBytes;
        c.mem = static_cast<u8*>(malloc(memSize));
        if (!c.mem) return false;
        u32* const pal = reinterpret_cast<u32*>(c.mem);         // 256 x RGBA (bytes R,G,B,A)
        u8* const prevRow = c.mem + 1024 + 16;                  // [filter][row]: the row above block row 0
        u8* const block = prevRow + stride;                     // kRows x [filter][row]
        u8* const outBuf = block + size_t(kRows) * stride + 16;
        u8* const inBuf = outBuf + size_t(kRows) * outRow;
        for (u32 i = 0; i < 256; ++i) pal[i] = 0xFF000000u;     // black, opaque
        memset(prevRow, 0, stride);
        u32 palCount = 0;

        // Buffered reader over the PNG file.
        u32 inPos = 0, inEnd = 0;
        auto refill = [&]() -> bool {
            if (inPos < inEnd) return true;
            inPos = 0; inEnd = static_cast<u32>(fread(inBuf, 1, kInBytes, c.in));
            return inEnd != 0;
        };
        auto get = [&](u8* dst, u32 n) -> bool {
            while (n) {
                if (!refill()) return false;
                const u32 k = std::min(n, inEnd - inPos);
                memcpy(dst, inBuf + inPos, k); inPos += k; dst += k; n -= k;
            }
            return true;
        };
        auto skip = [&](u32 n) -> bool {
            const u32 k = std::min(n, inEnd - inPos); inPos += k; n -= k;
            if (n == 0) return true;
            inPos = inEnd = 0;
            return fseek(c.in, n, SEEK_CUR) == 0;
        };

        if (inflateInit(&c.zs) != Z_OK) return false;
        c.zInit = true;
        c.out = fopen(tmpPath.c_str(), "wb");
        if (!c.out) return false;
        setvbuf(c.out, nullptr, _IONBF, 0);                     // writes are whole output blocks

        u32 rowsDone = 0, blockFill = 0, blockRows = 0;         // bytes inflated into block, rows finished in it
        bool zEnd = false;

        // Converts one unfiltered row to RGBA8888.
        auto convertRow = [&](const u8* s, u8* d) -> bool {
            u32 px = 0;
            if (depth == 8) {
                switch (type) {
                    case 6: memcpy(d, s, outRow); return true;
                    case 2:
                        for (; px + 16 <= w; px += 16) {
                            const uint8x16x3_t v = vld3q_u8(s + px * 3);
                            vst4q_u8(d + px * 4, uint8x16x4_t{{ v.val[0], v.val[1], v.val[2], vdupq_n_u8(255) }});
                        }
                        for (; px < w; ++px) { d[px*4] = s[px*3]; d[px*4+1] = s[px*3+1]; d[px*4+2] = s[px*3+2]; d[px*4+3] = 255; }
                        return true;
                    case 0:
                        for (; px + 16 <= w; px += 16) {
                            const uint8x16_t g = vld1q_u8(s + px);
                            vst4q_u8(d + px * 4, uint8x16x4_t{{ g, g, g, vdupq_n_u8(255) }});
                        }
                        for (; px < w; ++px) { d[px*4] = d[px*4+1] = d[px*4+2] = s[px]; d[px*4+3] = 255; }
                        return true;
                    case 4:
                        for (; px + 16 <= w; px += 16) {
                            const uint8x16x2_t v = vld2q_u8(s + px * 2);
                            vst4q_u8(d + px * 4, uint8x16x4_t{{ v.val[0], v.val[0], v.val[0], v.val[1] }});
                        }
                        for (; px < w; ++px) { d[px*4] = d[px*4+1] = d[px*4+2] = s[px*2]; d[px*4+3] = s[px*2+1]; }
                        return true;
                    case 3:
                        for (; px < w; ++px) {
                            if (s[px] >= palCount) return false;
                            memcpy(d + px * 4, &pal[s[px]], 4);
                        }
                        return true;
                }
                return false;
            }
            // 16-bit: keep the high (first) byte of every sample
            for (; px < w; ++px, d += 4) {
                const u8* q = s + px * bpp;
                switch (type) {
                    case 0: d[0] = d[1] = d[2] = q[0]; d[3] = 255; break;
                    case 2: d[0] = q[0]; d[1] = q[2]; d[2] = q[4]; d[3] = 255; break;
                    case 4: d[0] = d[1] = d[2] = q[0]; d[3] = q[2]; break;
                    case 6: d[0] = q[0]; d[1] = q[2]; d[2] = q[4]; d[3] = q[6]; break;
                }
            }
            return true;
        };

        // Finishes every complete row in the block; a full block is written out.
        auto finishRows = [&]() -> bool {
            while ((blockRows + 1) * stride <= blockFill) {
                if (rowsDone == h) return false;                // more rows than the image has
                u8* const r = block + size_t(blockRows) * stride;
                const u8* const above = (blockRows ? r - stride : prevRow) + 1;
                if (!png::unfilterRow(r[0], r + 1, above, rowBytes, bpp) ||
                    !convertRow(r + 1, outBuf + size_t(blockRows) * outRow)) return false;
                ++blockRows; ++rowsDone;
            }
            if (blockRows == kRows) {
                const size_t n = size_t(kRows) * outRow;
                if (wallDst) png::toWallpaper(outBuf, size_t(kRows) * w, wallDst + size_t(rowsDone - kRows) * w * wallBpp, fullColorWallpaper);
                if (fwrite(outBuf, 1, n, c.out) != n) return false;
                memcpy(prevRow, block + size_t(kRows - 1) * stride, stride);
                blockRows = blockFill = 0;
            }
            return true;
        };

        // Chunks after IHDR.
        while (!zEnd) {
            u8 ch[8];
            if (!get(ch, 8)) return fail();
            u32 len = be32(ch);
            if (len > 0x7FFFFFFFu) return fail();
            if (!memcmp(ch + 4, "IDAT", 4)) {
                while (len && !zEnd) {
                    if (!refill()) return fail();
                    const u32 n = std::min(len, inEnd - inPos);
                    c.zs.next_in = inBuf + inPos; c.zs.avail_in = n;
                    while (c.zs.avail_in && !zEnd) {
                        c.zs.next_out = block + blockFill; c.zs.avail_out = kRows * stride - blockFill;
                        const int r = inflate(&c.zs, Z_NO_FLUSH);
                        if (r != Z_OK && r != Z_STREAM_END) return fail();
                        blockFill = kRows * stride - c.zs.avail_out;
                        if (!finishRows()) return fail();
                        if (r == Z_STREAM_END) zEnd = true;
                    }
                    const u32 used = n - c.zs.avail_in;
                    inPos += used; len -= used;
                }
                if (len && !skip(len)) return fail();           // data after the stream end
            } else if (!memcmp(ch + 4, "PLTE", 4)) {
                if (len % 3 || len > 768 || type != 3) {
                    if (type == 3 || !skip(len)) return fail(); // PLTE in RGB files is a hint
                } else {
                    u8 t[768];
                    if (!get(t, len)) return fail();
                    palCount = len / 3;
                    for (u32 i = 0; i < palCount; ++i) memcpy(&pal[i], t + i * 3, 3);
                }
            } else if (!memcmp(ch + 4, "tRNS", 4) && type == 3) {
                if (len > 256) return fail();
                u8 t[256];
                if (!get(t, len)) return fail();
                for (u32 i = 0; i < len; ++i) reinterpret_cast<u8*>(&pal[i])[3] = t[i];
            } else if (!memcmp(ch + 4, "IEND", 4)) {
                break;
            } else if (!(ch[4] & 0x20)) {
                return fail();                                  // unknown critical chunk
            } else if (!skip(len)) {
                return fail();                                  // ancillary chunk: skipped
            }
            if (!skip(4)) return fail();                        // CRC
        }
        if (!zEnd || rowsDone != h || blockFill != blockRows * stride) return fail();
        const size_t tail = size_t(blockRows) * outRow;
        if (tail && fwrite(outBuf, 1, tail, c.out) != tail) return fail();
        if (tail && wallDst) png::toWallpaper(outBuf, size_t(blockRows) * w, wallDst + size_t(rowsDone - blockRows) * w * wallBpp, fullColorWallpaper);
        if (fclose(c.out) != 0) { c.out = nullptr; return fail(); }
        c.out = nullptr;
        std::remove(outPath.c_str());
        if (std::rename(tmpPath.c_str(), outPath.c_str()) != 0) return fail();
        return true;
    }
    

    // Sets a PNG as the wallpaper: decoded once into WALLPAPER_PATH (.rgba) and, when the
    // wallpaper is shown, straight into memory in the same pass, so the 1.29 MB file is
    // never read back. Same renderer handshake as reloadWallpaper(). Returns false if the
    // PNG can't be used, keeping the current wallpaper (file and memory).
    // The PNG checks convertPngToRgba makes before decoding anything: signature, IHDR,
    // size, a supported type / depth, not interlaced. Lets an unusable file be refused
    // before the current wallpaper is touched.
    static bool pngHeaderOk(const std::string& pngPath, u32 width, u32 height) {
        FILE* f = fopen(pngPath.c_str(), "rb");
        if (!f) return false;
        u8 h[8 + 8 + 13];
        const bool got = fread(h, 1, sizeof(h), f) == sizeof(h);
        fclose(f);
        static constexpr u8 kSig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        if (!got || memcmp(h, kSig, 8) != 0 || memcmp(h + 12, "IHDR", 4) != 0) return false;
        auto be32 = [](const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]); };
        const u8* ih = h + 16;
        const u8 depth = ih[8], type = ih[9];
        if (be32(h + 8) != 13 || be32(ih) != width || be32(ih + 4) != height || ih[10] || ih[11] || ih[12]) return false;
        if (type != 0 && type != 2 && type != 3 && type != 4 && type != 6) return false;
        return depth == 8 || (depth == 16 && type != 3);
    }

    bool setWallpaperFromPng(const std::string& pngPath) {
        if (!pngHeaderOk(pngPath, 448, 720)) return false;       // nothing touched
        if (glassReplacesWallpaper() || limitedMemory) {          // not shown: file only
            if (!convertPngToRgba(pngPath, WALLPAPER_PATH)) return false;
            reloadWallpaper();
            return true;
        }
        auto lock = beginWallpaperChange();
        freeWallpaperData();
        releaseGlassForWallpaper();
        // (no memory: as a refused PNG, files untouched)
        const bool ok = wallpaperData.resize(size_t(448) * 720 * (fullColorWallpaper ? 3 : 2)) &&
                        convertPngToRgba(pngPath, WALLPAPER_PATH, 448, 720, wallpaperData.data());
        if (ok) {
            setAdjustFromIni(getParsedDataFromIniFile(wallpaperIniPath()));   // this wallpaper's
            if (fullColorWallpaper) bakeLoadedWallpaper();
            else {                                        // 4-bit: adjusted while converting, from the file
                s8 t[WA_COUNT];
                wallpaperAdjustTarget(t);
                if (anyAdjustValues(t)) loadWallpaperFile(WALLPAPER_PATH);
                else setWallpaperBaked(kNoWallAdjust);
            }
            wallpaperGeneration.fetch_add(1, std::memory_order_acq_rel);
        } else {                                                  // files untouched: back to the
            freeWallpaperData();                                  // previous wallpaper
            loadCurrentWallpaper();   // still, else animated (wallpaper.ini) + black level
        }
        refreshWallpaper.store(false, std::memory_order_release);
        cv.notify_all();
        return ok;
    }
    #endif


    void loadWallpaperFileWhenSafe() {
        if (!limitedMemory && !inPlot.load(std::memory_order_acquire) && !refreshWallpaper.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(wallpaperMutex);
            cv.wait(lock, [] { return !inPlot.load(std::memory_order_acquire) && !refreshWallpaper.load(std::memory_order_acquire); });
            if (glassReplacesWallpaper()) {
                if (!wallpaperData.empty()) freeWallpaperData();
            } else if (wallpaperData.empty()) {
                loadCurrentWallpaper();   // still, else animated (wallpaper.ini) + black level
            }
        }
    }


    void reloadWallpaper() {
        auto lock = beginWallpaperChange();
        
        // Clear the current wallpaper data (and give the memory back)
        const bool hadWallpaper = !wallpaperData.empty();
        freeWallpaperData();
        // Freed for the glass (they share one block): keep the last frame up until the glass
        // draws (at most 500 ms) instead of drawing a frame with neither (see the overlay loop).
        if (hadWallpaper && glassReplacesWallpaper())
            glassStartPauseUntilNs.store(ult::nowNs() + 500'000'000ULL, std::memory_order_release);
        
        // Reload the wallpaper unless frosted glass replaces it. A still image wins;
        // otherwise an animated wallpaper (.uaw), if one is set.
        if (!glassReplacesWallpaper()) {
            loadCurrentWallpaper();   // still, else animated (wallpaper.ini) + black level
        }
        
        // Signal that wallpaper has finished refreshing
        refreshWallpaper.store(false, std::memory_order_release);
        
        // Notify any waiting threads
        cv.notify_all();
    }

    
    std::atomic<bool> themeIsInitialized(false); // for loading the theme once in OverlayFrame / HeaderOverlayFrame
    
    // Variables for touch commands
    std::atomic<bool> touchingBack(false);
    std::atomic<bool> touchingSelect(false);
    std::atomic<bool> touchingNextPage(false);
    std::atomic<bool> touchingMenu(false);
    #if IS_LAUNCHER_DIRECTIVE
    bool useHomeShortcut = true;
    #endif
    std::atomic<bool> shortTouchAndRelease(false);
    std::atomic<bool> longTouchAndRelease(false);
    std::atomic<bool> simulatedBack(false);
    std::atomic<bool> simulatedSelect(false);
    std::atomic<bool> simulatedNextPage(false);
    std::atomic<bool> simulatedMenu(false);
    std::atomic<bool> stillTouching(false);
    std::atomic<bool> interruptedTouch(false);
    std::atomic<bool> touchInBounds(false);
    
    
#if USING_WIDGET_DIRECTIVE
    // Battery implementation
    bool powerInitialized = false;
    bool powerCacheInitialized;
    uint32_t powerCacheCharge;
    bool powerCacheIsCharging;
    PsmSession powerSession;
    
    std::atomic<uint32_t> batteryCharge{0};
    std::atomic<bool> isCharging{false};
    
    bool powerGetDetails(uint32_t *_batteryCharge, bool *_isCharging) {
        static uint64_t last_call_ns = 0;
        
        // Ensure power system is initialized
        if (!powerInitialized) {
            return false;
        }
        
        // Get the current time in nanoseconds
        const uint64_t now_ns = nowNs();
        
        // 3 seconds in nanoseconds
        static constexpr uint64_t min_delay_ns = 3000000000ULL;
        
        // Check if enough time has elapsed or if cache is not initialized
        const bool useCache = (now_ns - last_call_ns <= min_delay_ns) && powerCacheInitialized;
        if (!useCache) {
            PsmChargerType charger = PsmChargerType_Unconnected;
            Result rc = psmGetBatteryChargePercentage(_batteryCharge);
            bool hwReadsSucceeded = R_SUCCEEDED(rc);
            
            if (hwReadsSucceeded) {
                rc = psmGetChargerType(&charger);
                hwReadsSucceeded &= R_SUCCEEDED(rc);
                *_isCharging = (charger != PsmChargerType_Unconnected);
                
                if (hwReadsSucceeded) {
                    // Update cache
                    powerCacheCharge = *_batteryCharge;
                    powerCacheIsCharging = *_isCharging;
                    powerCacheInitialized = true;
                    last_call_ns = now_ns; // Update last call time after successful hardware read
                    return true;
                }
            }
            
            // Use cached values if the hardware read fails
            if (powerCacheInitialized) {
                *_batteryCharge = powerCacheCharge;
                *_isCharging = powerCacheIsCharging;
                return hwReadsSucceeded; // Return false if hardware read failed but cache is valid
            }
            
            // Return false if cache is not initialized and hardware read failed
            return false;
        }
        
        // Use cached values if not enough time has passed
        *_batteryCharge = powerCacheCharge;
        *_isCharging = powerCacheIsCharging;
        return true; // Return true as cache is used
    }
    
    
    void powerInit(void) {
        uint32_t charge = 0;
        bool charging = false;
        
        powerCacheInitialized = false;
        powerCacheCharge = 0;
        powerCacheIsCharging = false;
        
        if (!powerInitialized) {
            Result rc = psmInitialize();
            if (R_SUCCEEDED(rc)) {
                rc = psmBindStateChangeEvent(&powerSession, 1, 1, 1);
                
                if (R_FAILED(rc))
                    psmExit();
                
                if (R_SUCCEEDED(rc)) {
                    powerInitialized = true;
                    ult::powerGetDetails(&charge, &charging);
                    ult::batteryCharge.store(charge, std::memory_order_release);
                    ult::isCharging.store(charging, std::memory_order_release);
                }
            }
        }
    }
    
    void powerExit(void) {
        if (powerInitialized) {
            psmUnbindStateChangeEvent(&powerSession);
            psmExit();
            powerInitialized = false;
            powerCacheInitialized = false;
        }
    }
#endif
    
    
    // Temperature Implementation
    std::atomic<float> PCB_temperature{0.0f};
    std::atomic<float> SOC_temperature{0.0f};
    
    /*
    I2cReadRegHandler was taken from Switch-OC-Suite source code made by KazushiMe
    Original repository link (Deleted, last checked 15.04.2023): https://github.com/KazushiMe/Switch-OC-Suite
    */
    
    Result I2cReadRegHandler(u8 reg, I2cDevice dev, u16 *out) {
        struct readReg {
            u8 send;
            u8 sendLength;
            u8 sendData;
            u8 receive;
            u8 receiveLength;
        };
        
        I2cSession _session;
        
        Result res = i2cOpenSession(&_session, dev);
        if (res)
            return res;
        
        u16 val;
        
        struct readReg readRegister = {
            .send = 0 | (I2cTransactionOption_Start << 6),
            .sendLength = sizeof(reg),
            .sendData = reg,
            .receive = 1 | (I2cTransactionOption_All << 6),
            .receiveLength = sizeof(val),
        };
        
        res = i2csessionExecuteCommandList(&_session, &val, sizeof(val), &readRegister, sizeof(readRegister));
        if (res) {
            i2csessionClose(&_session);
            return res;
        }
        
        *out = val;
        i2csessionClose(&_session);
        return 0;
    }
    
    
    // Common helper function to read temperature (integer and fractional parts)
    Result ReadTemperature(float *temperature, u8 integerReg, u8 fractionalReg, bool integerOnly) {
        u16 rawValue;
        u8 val;
        s32 integerPart = 0;
        float fractionalPart = 0.0f;  // Change this to a float to retain fractional precision
        
        // Read the integer part of the temperature
        Result res = I2cReadRegHandler(integerReg, I2cDevice_Tmp451, &rawValue);
        if (R_FAILED(res)) {
            return res;  // Error during I2C read
        }
        
        val = (u8)rawValue;  // Cast the value to an 8-bit unsigned integer
        integerPart = val;    // Integer part of temperature in Celsius
        
        if (integerOnly)
        {
            *temperature = static_cast<float>(integerPart);  // Ensure it's treated as a float
            return 0;  // Return only integer part if requested
        }
        
        // Read the fractional part of the temperature
        res = I2cReadRegHandler(fractionalReg, I2cDevice_Tmp451, &rawValue);
        if (R_FAILED(res)) {
            return res;  // Error during I2C read
        }
        
        val = (u8)rawValue;  // Cast the value to an 8-bit unsigned integer
        fractionalPart = static_cast<float>(val >> 4) * 0.0625f;  // Convert upper 4 bits into fractional part
        
        // Combine integer and fractional parts
        *temperature = static_cast<float>(integerPart) + fractionalPart;
        
        return 0;
    }
    
    // Function to get the SOC temperature
    Result ReadSocTemperature(float *temperature, bool integerOnly) {
        return ReadTemperature(temperature, TMP451_SOC_TEMP_REG, TMP451_SOC_TMP_DEC_REG, integerOnly);
    }
    
    // Function to get the PCB temperature
    Result ReadPcbTemperature(float *temperature, bool integerOnly) {
        return ReadTemperature(temperature, TMP451_PCB_TEMP_REG, TMP451_PCB_TMP_DEC_REG, integerOnly);
    }
    
    
    // Time implementation
    CONSTEXPR_STRING std::string DEFAULT_DT_FORMAT = "%a %T";
    std::string datetimeFormat = DEFAULT_DT_FORMAT;
    
    
    // Widget settings
    bool hideClock, hideBattery, hidePCBTemp, hideSOCTemp, dynamicWidgetColors, dynamicWidgetBorder;
    bool hideWidgetBackdrop, hideWidgetBorder, centerWidgetAlignment, extendedWidgetBackdrop;

    // Shared helper: single hash lookup instead of count() + at()
    static bool getBoolFromSection(const std::map<std::string, std::string>& section,
                                   const char* key, bool defaultValue = false) {
        const auto it = section.find(key);
        return (it != section.end()) ? (it->second != FALSE_STR) : defaultValue;
    }

    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeWidgetVars() {
        // Load INI data once instead of 8 separate file reads
        auto ultrahandSection = getKeyValuePairsFromSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME);
        
        hideClock             = getBoolFromSection(ultrahandSection, "hide_clock",               false);
        hideBattery           = getBoolFromSection(ultrahandSection, "hide_battery",             true);
        hideSOCTemp           = getBoolFromSection(ultrahandSection, "hide_soc_temp",            true);
        hidePCBTemp           = getBoolFromSection(ultrahandSection, "hide_pcb_temp",            true);
        dynamicWidgetColors   = getBoolFromSection(ultrahandSection, "dynamic_widget_colors",    true);
        dynamicWidgetBorder   = getBoolFromSection(ultrahandSection, "dynamic_widget_border",    false);
        hideWidgetBackdrop    = getBoolFromSection(ultrahandSection, "hide_widget_backdrop",     false);
        hideWidgetBorder      = getBoolFromSection(ultrahandSection, "hide_widget_border",       false);
        centerWidgetAlignment = getBoolFromSection(ultrahandSection, "center_widget_alignment",  true);
        extendedWidgetBackdrop= getBoolFromSection(ultrahandSection, "extended_widget_backdrop", true); // inverted: true = off
    }
    #endif
    
    bool cleanVersionLabels, hideOverlayVersions, hidePackageVersions, useLibultrahandTitles, useLibultrahandVersions, usePackageTitles, usePackageVersions, useInPackageTitles;
    


    
    // Helper function to convert MB to bytes
    u64 mbToBytes(u32 mb) {
        return static_cast<u64>(mb) * 0x100000;
    }
    
    // Helper function to convert bytes to MB
    u32 bytesToMB(u64 bytes) {
        return static_cast<u32>(bytes / 0x100000);
    }
    
   

    // Helper function to get version-appropriate default heap size
    static OverlayHeapSize getDefaultHeapSize() {
        if (hosversionAtLeast(21, 0, 0)) {
            return OverlayHeapSize::Size_4MB;  // HOS 21.0.0+
        } else if (hosversionAtLeast(20, 0, 0)) {
            return OverlayHeapSize::Size_6MB;  // HOS 20.0.0+
        } else {
            return OverlayHeapSize::Size_8MB;  // Older versions
        }
    }
    
    // Implementation
    OverlayHeapSize getCurrentHeapSize() {
        // Fast path: return cached value if already loaded
        if (heapSizeCache.initialized) {
            return heapSizeCache.cachedSize;
        }
        
        // Slow path: read from file (only happens once)
        FILE* f = fopen(ult::OVL_HEAP_CONFIG_PATH.c_str(), "rb");
        if (!f) {
            // No config file - use version-specific default
            heapSizeCache.cachedSize = getDefaultHeapSize();
            heapSizeCache.initialized = true;
            return heapSizeCache.cachedSize;
        }
        
        u64 size;
        if (fread(&size, sizeof(size), 1, f) == 1) {
            constexpr u64 twoMB = 0x200000;
            // Only accept multiples of 2MB, excluding 2MB itself
            if (size != twoMB && size % twoMB == 0) {
                heapSizeCache.cachedSize = static_cast<OverlayHeapSize>(size);
                fclose(f);
                heapSizeCache.initialized = true;
                return heapSizeCache.cachedSize;
            }
        }
        
        // Invalid or no data in config - use version-specific default
        fclose(f);
        heapSizeCache.cachedSize = getDefaultHeapSize();
        heapSizeCache.initialized = true;
        return heapSizeCache.cachedSize;
    }
    
    // Update the global default too
    HeapSizeCache heapSizeCache;
    OverlayHeapSize currentHeapSize = getDefaultHeapSize();
    
    bool setOverlayHeapSize(OverlayHeapSize heapSize) {
        ult::createDirectory(ult::NX_OVLLOADER_PATH);
        
        FILE* f = fopen(ult::OVL_HEAP_CONFIG_PATH.c_str(), "wb");
        if (!f) return false;
        
        const u64 size = static_cast<u64>(heapSize);
        const bool success = (fwrite(&size, sizeof(size), 1, f) == 1);
        fclose(f);
        
        // Update cache on successful write
        if (success) {
            heapSizeCache.cachedSize = heapSize;
            heapSizeCache.initialized = true;

            // Create reloading flag to indicate this was an intentional restart
            ult::createDirectory(ult::FLAGS_PATH);
            f = fopen(ult::RELOADING_FLAG_FILEPATH.c_str(), "wb");
            if (f) {
                fclose(f);  // Empty file, just needs to exist
            }
        }
        
        return success;
    }
    
    
    // Implementation
    bool requestOverlayExit() {
        ult::createDirectory(ult::NX_OVLLOADER_PATH);
        
        FILE* f = fopen(ult::OVL_EXIT_FLAG_PATH.c_str(), "wb");
        if (!f) return false;
        
        // Write a single byte (flag file just needs to exist)
        u8 flag = 1;
        bool success = (fwrite(&flag, 1, 1, f) == 1);
        fclose(f);
        
        deleteFileOrDirectory(NOTIFICATIONS_FLAG_FILEPATH);
        
        return success;
    }

    bool requestOverlayReload() {
        // Create reloading flag to indicate this was an intentional restart
        ult::createDirectory(ult::FLAGS_PATH);
        FILE* f = fopen(ult::RELOADING_FLAG_FILEPATH.c_str(), "wb");
        if (f) {
            fclose(f);  // Empty file, just needs to exist
        }

        ult::createDirectory(ult::NX_OVLLOADER_PATH);
        
        f = fopen(ult::OVL_RELOAD_FLAG_PATH.c_str(), "wb");
        if (!f) return false;
        
        u8 flag = 1;
        bool success = (fwrite(&flag, 1, 1, f) == 1);
        fclose(f);
        
        return success;
    }

    const std::string loaderInfo = envGetLoaderInfo();
    std::string loaderTitle = extractTitle(loaderInfo);

    bool expandedMemory = false;
    bool furtherExpandedMemory = false;
    bool limitedMemory = false;
    
    std::string versionLabel;
    
    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeVersionLabels() {
        // Load INI data once instead of 6 separate file reads
        auto ultrahandSection = getKeyValuePairsFromSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME);
        
        cleanVersionLabels  = getBoolFromSection(ultrahandSection, "clean_version_labels",  false);
        hideOverlayVersions = getBoolFromSection(ultrahandSection, "hide_overlay_versions", false);
        hidePackageVersions = getBoolFromSection(ultrahandSection, "hide_package_versions", false);
    }
    #endif
    
    
    // Number of renderer helper threads (tsl::gfx::Renderer::runParallel)
    constexpr unsigned numThreads = 4;
}