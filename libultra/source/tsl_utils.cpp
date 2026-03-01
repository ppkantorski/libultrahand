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

//#include <cstdlib>
extern "C" { // assertion override
    void __assert_func(const char *_file, int _line, const char *_func, const char *_expr ) {
        abort();
    }
}

namespace ult {

    double cos(double x) {
        static constexpr double PI = 3.14159265358979323846;
        static constexpr double TWO_PI = 6.28318530717958647692;
        static constexpr double HALF_PI = 1.57079632679489661923;
        
        x = x - TWO_PI * static_cast<int>(x * 0.159154943091895);
        if (x < 0) x += TWO_PI;
        
        int sign = 1;
        if (x > PI) {
           x -= PI;
           sign = -1;
        }
        if (x > HALF_PI) {
           x = PI - x;
           sign = -sign;
        }
        
        const double x2 = x * x;
        return sign * (1.0 + x2 * (-0.5 + x2 * (0.04166666666666666 + x2 * (-0.001388888888888889 + x2 * (0.0000248015873015873 - x2 * 0.0000002755731922398589)))));
    }

    bool correctFrameSize; // for detecting the correct Overlay display size

    u16 DefaultFramebufferWidth = 448;            ///< Width of the framebuffer
    u16 DefaultFramebufferHeight = 720;           ///< Height of the framebuffer

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
    
    
    u16 activeHeaderHeight = 97;

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
        if (R_FAILED(pmdmntGetApplicationProcessId(&pid)))
            return NULL_STR;
        
        if (R_FAILED(ldrDmntInitialize()))
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
    std::atomic<bool> internalTouchReleased(true);

    u32 layerEdge = 0;
    bool useRightAlignment = false;
    bool useSwipeToOpen = true;
    bool useLaunchCombos = true;
    bool useNotifications = true;
    bool useStartupNotification = true;
    bool useSoundEffects = true;
    bool useHapticFeedback = false;
    bool usePageSwap = false;
    bool useDynamicLogo = true;
    bool useSelectionBG = true;
    bool useSelectionText = true;
    bool useSelectionValue = false;

    std::atomic<bool> noClickableItems{false};
    
    #if IS_LAUNCHER_DIRECTIVE
    std::atomic<bool> overlayLaunchRequested{false};
    std::string requestedOverlayPath;
    std::string requestedOverlayArgs;
    std::mutex overlayLaunchMutex;
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


    std::array<KeyInfo, 18> KEYS_INFO = {{
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

    static std::unordered_map<std::string, std::string> createButtonCharMap() {
        std::unordered_map<std::string, std::string> map;
        for (const auto& keyInfo : KEYS_INFO) {
            map[keyInfo.name] = keyInfo.glyph;
        }
        return map;
    }
    
    std::unordered_map<std::string, std::string> buttonCharMap = createButtonCharMap();
    
    
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
        auto it = buttonCharMap.end();  // Initialize iterator once outside the loop
        
        // Iterate through the combo string and split by '+'
        for (size_t i = 0; i <= length; ++i) {
            if (i == length || combo[i] == '+') {
                // Get the current token (trimmed)
                end = i;  // Reuse the end variable
                while (start < end && std::isspace(combo[start])) start++;  // Trim leading spaces
                while (end > start && std::isspace(combo[end - 1])) end--;  // Trim trailing spaces
                
                token = combo.substr(start, end - start);  // Reuse the token variable
                it = buttonCharMap.find(token);  // Reuse the iterator
                
                if (it != buttonCharMap.end()) {
                    unicodeCombo += it->second;  // Append the mapped Unicode value
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
    std::string DYNAMIC_COLORS;
    std::string CENTER_ALIGNMENT;
    std::string EXTENDED_BACKDROP;
    std::string MISCELLANEOUS;
    std::string MENU_SETTINGS;
    std::string USER_GUIDE;
    std::string PACKAGES_MENU;
    std::string SHOW_HIDDEN;
    std::string SHOW_DELETE;
    std::string SHOW_UNSUPPORTED;
    std::string PAGE_SWAP;
    std::string RIGHT_SIDE_MODE;
    std::string OVERLAY_VERSIONS;
    std::string PACKAGE_VERSIONS;
    std::string CLEAN_VERSIONS;
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
    std::string SOUND_SUPPORT_DISABLED;
    std::string WALLPAPER_SUPPORT_ENABLED;
    std::string SOUND_SUPPORT_ENABLED;
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
    std::string LAUNCH_COMBOS;
    std::string STARTUP_NOTIFICATION;
    std::string EXTERNAL_NOTIFICATIONS;
    std::string HAPTIC_FEEDBACK;
    std::string OPAQUE_SCREENSHOTS;
    std::string PACKAGE_INFO;
    std::string _TITLE;
    std::string _VERSION;
    std::string _CREATOR;
    std::string _ABOUT;
    std::string _CREDITS;
    std::string USERGUIDE_OFFSET;
    std::string SETTINGS_MENU;
    std::string SCRIPT_OVERLAY;
    std::string STAR_FAVORITE;
    std::string APP_SETTINGS;
    std::string ON_MAIN_MENU;
    std::string ON_A_COMMAND;
    std::string ON_OVERLAY_PACKAGE;
    std::string FEATURES;
    std::string SWIPE_TO_OPEN;
    std::string THEME_SETTINGS;
    std::string DYNAMIC_LOGO;
    std::string SELECTION_BACKGROUND;
    std::string SELECTION_TEXT;
    std::string SELECTION_VALUE;
    std::string LIBULTRAHAND_TITLES;
    std::string LIBULTRAHAND_VERSIONS;
    std::string PACKAGE_TITLES;
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
    #endif

    std::string INCOMPATIBLE_WARNING;
    std::string SYSTEM_RAM;
    std::string FREE;
    std::string DEFAULT_CHAR_WIDTH;
    std::string UNAVAILABLE_SELECTION;
    std::string ON;
    std::string OFF;
    std::string OK;
    std::string BACK;
    std::string HIDE;
    std::string CANCEL;
    std::string GAP_1;
    std::string GAP_2;

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


    // --- Single source-of-truth table ---

    struct LangEntry { std::string* var; const char* key; const char* defaultVal; };

    static constexpr LangEntry LANG_TABLE[] = {
        #if IS_LAUNCHER_DIRECTIVE
        {&ENGLISH,                    "ENGLISH",                    "English"},
        {&SPANISH,                    "SPANISH",                    "Spanish"},
        {&FRENCH,                     "FRENCH",                     "French"},
        {&GERMAN,                     "GERMAN",                     "German"},
        {&JAPANESE,                   "JAPANESE",                   "Japanese"},
        {&KOREAN,                     "KOREAN",                     "Korean"},
        {&ITALIAN,                    "ITALIAN",                    "Italian"},
        {&DUTCH,                      "DUTCH",                      "Dutch"},
        {&PORTUGUESE,                 "PORTUGUESE",                 "Portuguese"},
        {&RUSSIAN,                    "RUSSIAN",                    "Russian"},
        {&UKRAINIAN,                  "UKRAINIAN",                  "Ukrainian"},
        {&POLISH,                     "POLISH",                     "Polish"},
        {&SIMPLIFIED_CHINESE,         "SIMPLIFIED_CHINESE",         "Simplified Chinese"},
        {&TRADITIONAL_CHINESE,        "TRADITIONAL_CHINESE",        "Traditional Chinese"},
        {&OVERLAYS,                   "OVERLAYS",                   "Overlays"},
        {&OVERLAYS_ABBR,              "OVERLAYS_ABBR",              "Overlays"},
        {&OVERLAY,                    "OVERLAY",                    "Overlay"},
        {&HIDDEN_OVERLAYS,            "HIDDEN_OVERLAYS",            "Hidden Overlays"},
        {&PACKAGES,                   "PACKAGES",                   "Packages"},
        {&PACKAGE,                    "PACKAGE",                    "Package"},
        {&HIDDEN_PACKAGES,            "HIDDEN_PACKAGES",            "Hidden Packages"},
        {&HIDDEN,                     "HIDDEN",                     "Hidden"},
        {&HIDE_OVERLAY,               "HIDE_OVERLAY",               "Hide Overlay"},
        {&HIDE_PACKAGE,               "HIDE_PACKAGE",               "Hide Package"},
        {&LAUNCH_ARGUMENTS,           "LAUNCH_ARGUMENTS",           "Launch Arguments"},
        {&FORCE_AMS110_SUPPORT,       "FORCE_AMS110_SUPPORT",       "Force AMS110+ Support"},
        {&QUICK_LAUNCH,               "QUICK_LAUNCH",               "Quick Launch"},
        {&BOOT_COMMANDS,              "BOOT_COMMANDS",              "Boot Commands"},
        {&EXIT_COMMANDS,              "EXIT_COMMANDS",              "Exit Commands"},
        {&ERROR_LOGGING,              "ERROR_LOGGING",              "Error Logging"},
        {&COMMANDS,                   "COMMANDS",                   "Commands"},
        {&SETTINGS,                   "SETTINGS",                   "Settings"},
        {&FAVORITE,                   "FAVORITE",                   "Favorite"},
        {&MAIN_SETTINGS,              "MAIN_SETTINGS",              "Main Settings"},
        {&UI_SETTINGS,                "UI_SETTINGS",                "UI Settings"},
        {&WIDGET,                     "WIDGET",                     "Widget"},
        {&WIDGET_ITEMS,               "WIDGET_ITEMS",               "Widget Items"},
        {&WIDGET_SETTINGS,            "WIDGET_SETTINGS",            "Widget Settings"},
        {&CLOCK,                      "CLOCK",                      "Clock"},
        {&BATTERY,                    "BATTERY",                    "Battery"},
        {&SOC_TEMPERATURE,            "SOC_TEMPERATURE",            "SOC Temperature"},
        {&PCB_TEMPERATURE,            "PCB_TEMPERATURE",            "PCB Temperature"},
        {&BACKDROP,                   "BACKDROP",                   "Backdrop"},
        {&DYNAMIC_COLORS,             "DYNAMIC_COLORS",             "Dynamic Colors"},
        {&CENTER_ALIGNMENT,           "CENTER_ALIGNMENT",           "Center Alignment"},
        {&EXTENDED_BACKDROP,          "EXTENDED_BACKDROP",          "Extended Backdrop"},
        {&MISCELLANEOUS,              "MISCELLANEOUS",              "Miscellaneous"},
        {&MENU_SETTINGS,              "MENU_SETTINGS",              "Menu Settings"},
        {&USER_GUIDE,                 "USER_GUIDE",                 "User Guide"},
        {&PACKAGES_MENU,              "PACKAGES_MENU",              "Packages Menu"},
        {&SHOW_HIDDEN,                "SHOW_HIDDEN",                "Show Hidden"},
        {&SHOW_DELETE,                "SHOW_DELETE",                "Show Delete"},
        {&SHOW_UNSUPPORTED,           "SHOW_UNSUPPORTED",           "Show Unsupported"},
        {&PAGE_SWAP,                  "PAGE_SWAP",                  "Page Swap"},
        {&RIGHT_SIDE_MODE,            "RIGHT_SIDE_MODE",            "Right-side Mode"},
        {&OVERLAY_VERSIONS,           "OVERLAY_VERSIONS",           "Overlay Versions"},
        {&PACKAGE_VERSIONS,           "PACKAGE_VERSIONS",           "Package Versions"},
        {&CLEAN_VERSIONS,             "CLEAN_VERSIONS",             "Clean Versions"},
        {&KEY_COMBO,                  "KEY_COMBO",                  "Key Combo"},
        {&MODE,                       "MODE",                       "Mode"},
        {&LAUNCH_MODES,               "LAUNCH_MODES",               "Launch Modes"},
        {&LANGUAGE,                   "LANGUAGE",                   "Language"},
        {&OVERLAY_INFO,               "OVERLAY_INFO",               "Overlay Info"},
        {&SOFTWARE_UPDATE,            "SOFTWARE_UPDATE",            "Software Update"},
        {&UPDATE_ULTRAHAND,           "UPDATE_ULTRAHAND",           "Update Ultrahand"},
        {&SYSTEM,                     "SYSTEM",                     "System"},
        {&DEVICE_INFO,                "DEVICE_INFO",                "Device Info"},
        {&FIRMWARE,                   "FIRMWARE",                   "Firmware"},
        {&BOOTLOADER,                 "BOOTLOADER",                 "Bootloader"},
        {&HARDWARE,                   "HARDWARE",                   "Hardware"},
        {&MEMORY,                     "MEMORY",                     "Memory"},
        {&VENDOR,                     "VENDOR",                     "Vendor"},
        {&MODEL,                      "MODEL",                      "Model"},
        {&STORAGE,                    "STORAGE",                    "Storage"},
        {&OVERLAY_MEMORY,             "OVERLAY_MEMORY",             "Overlay Memory"},
        {&NOT_ENOUGH_MEMORY,          "NOT_ENOUGH_MEMORY",          "Not enough memory."},
        {&WALLPAPER_SUPPORT_DISABLED, "WALLPAPER_SUPPORT_DISABLED", "Wallpaper support disabled."},
        {&SOUND_SUPPORT_DISABLED,     "SOUND_SUPPORT_DISABLED",     "Sound support disabled."},
        {&WALLPAPER_SUPPORT_ENABLED,  "WALLPAPER_SUPPORT_ENABLED",  "Wallpaper support enabled."},
        {&SOUND_SUPPORT_ENABLED,      "SOUND_SUPPORT_ENABLED",      "Sound support enabled."},
        {&EXIT_OVERLAY_SYSTEM,        "EXIT_OVERLAY_SYSTEM",        "Exit Overlay System"},
        {&ULTRAHAND_ABOUT,            "ULTRAHAND_ABOUT",            "Ultrahand Overlay is a customizable overlay ecosystem for overlays, commands, hotkeys, and advanced system interaction."},
        {&ULTRAHAND_CREDITS_START,    "ULTRAHAND_CREDITS_START",    "Special thanks to "},
        {&ULTRAHAND_CREDITS_END,      "ULTRAHAND_CREDITS_END",      " and many others. ♥"},
        {&LOCAL_IP,                   "LOCAL_IP",                   "Local IP"},
        {&WALLPAPER,                  "WALLPAPER",                  "Wallpaper"},
        {&THEME,                      "THEME",                      "Theme"},
        {&SOUNDS,                     "SOUNDS",                     "Sounds"},
        {&DEFAULT,                    "DEFAULT",                    "default"},
        {&ROOT_PACKAGE,               "ROOT_PACKAGE",               "Root Package"},
        {&SORT_PRIORITY,              "SORT_PRIORITY",              "Sort Priority"},
        {&OPTIONS,                    "OPTIONS",                    "Options"},
        {&FAILED_TO_OPEN,             "FAILED_TO_OPEN",             "Failed to open file"},
        {&LAUNCH_COMBOS,              "LAUNCH_COMBOS",              "Launch Combos"},
        {&STARTUP_NOTIFICATION,       "STARTUP_NOTIFICATION",       "Startup Notification"},
        {&EXTERNAL_NOTIFICATIONS,     "EXTERNAL_NOTIFICATIONS",     "External Notifications"},
        {&HAPTIC_FEEDBACK,            "HAPTIC_FEEDBACK",            "Haptic Feedback"},
        {&OPAQUE_SCREENSHOTS,         "OPAQUE_SCREENSHOTS",         "Opaque Screenshots"},
        {&PACKAGE_INFO,               "PACKAGE_INFO",               "Package Info"},
        {&_TITLE,                     "_TITLE",                     "Title"},
        {&_VERSION,                   "_VERSION",                   "Version"},
        {&_CREATOR,                   "_CREATOR",                   "Creator(s)"},
        {&_ABOUT,                     "_ABOUT",                     "About"},
        {&_CREDITS,                   "_CREDITS",                   "Credits"},
        {&USERGUIDE_OFFSET,           "USERGUIDE_OFFSET",           "177"},
        {&SETTINGS_MENU,              "SETTINGS_MENU",              "Settings Menu"},
        {&SCRIPT_OVERLAY,             "SCRIPT_OVERLAY",             "Script Overlay"},
        {&STAR_FAVORITE,              "STAR_FAVORITE",              "Star/Favorite"},
        {&APP_SETTINGS,               "APP_SETTINGS",               "App Settings"},
        {&ON_MAIN_MENU,               "ON_MAIN_MENU",               "on Main Menu"},
        {&ON_A_COMMAND,               "ON_A_COMMAND",               "on a command"},
        {&ON_OVERLAY_PACKAGE,         "ON_OVERLAY_PACKAGE",         "on overlay/package"},
        {&FEATURES,                   "FEATURES",                   "Features"},
        {&SWIPE_TO_OPEN,              "SWIPE_TO_OPEN",              "Swipe to Open"},
        {&THEME_SETTINGS,             "THEME_SETTINGS",             "Theme Settings"},
        {&DYNAMIC_LOGO,               "DYNAMIC_LOGO",               "Dynamic Logo"},
        {&SELECTION_BACKGROUND,       "SELECTION_BACKGROUND",       "Selection Background"},
        {&SELECTION_TEXT,             "SELECTION_TEXT",             "Selection Text"},
        {&SELECTION_VALUE,            "SELECTION_VALUE",            "Selection Value"},
        {&LIBULTRAHAND_TITLES,        "LIBULTRAHAND_TITLES",        "libultrahand Titles"},
        {&LIBULTRAHAND_VERSIONS,      "LIBULTRAHAND_VERSIONS",      "libultrahand Versions"},
        {&PACKAGE_TITLES,             "PACKAGE_TITLES",             "Package Titles"},
        {&ULTRAHAND_HAS_STARTED,      "ULTRAHAND_HAS_STARTED",      "Ultrahand has started."},
        {&ULTRAHAND_HAS_RESTARTED,    "ULTRAHAND_HAS_RESTARTED",    "Ultrahand has restarted."},
        {&NEW_UPDATE_IS_AVAILABLE,    "NEW_UPDATE_IS_AVAILABLE",    "New update is available!"},
        {&DELETE_PACKAGE,             "DELETE_PACKAGE",             "Delete Package"},
        {&DELETE_OVERLAY,             "DELETE_OVERLAY",             "Delete Overlay"},
        {&SELECTION_IS_EMPTY,         "SELECTION_IS_EMPTY",         "Selection is empty!"},
        {&FORCED_SUPPORT_WARNING,     "FORCED_SUPPORT_WARNING",     "Forcing support can be dangerous."},
        {&TASK_IS_COMPLETE,           "TASK_IS_COMPLETE",           "Task is complete!"},
        {&TASK_HAS_FAILED,            "TASK_HAS_FAILED",            "Task has failed."},
        {&REBOOT_TO,                  "REBOOT_TO",                  "Reboot To"},
        {&REBOOT,                     "REBOOT",                     "Reboot"},
        {&SHUTDOWN,                   "SHUTDOWN",                   "Shutdown"},
        {&BOOT_ENTRY,                 "BOOT_ENTRY",                 "Boot Entry"},
        #endif

        {&INCOMPATIBLE_WARNING,       "INCOMPATIBLE_WARNING",       "Incompatible on AMS v1.10+"},
        {&SYSTEM_RAM,                 "SYSTEM_RAM",                 "System RAM"},
        {&FREE,                       "FREE",                       "free"},
        {&DEFAULT_CHAR_WIDTH,         "DEFAULT_CHAR_WIDTH",         "0.33"},
        {&UNAVAILABLE_SELECTION,      "UNAVAILABLE_SELECTION",      "Not available"},
        {&ON,                         "ON",                         "On"},
        {&OFF,                        "OFF",                        "Off"},
        {&OK,                         "OK",                         "OK"},
        {&BACK,                       "BACK",                       "Back"},
        {&HIDE,                       "HIDE",                       "Hide"},
        {&CANCEL,                     "CANCEL",                     "Cancel"},
        {&GAP_1,                      "GAP_1",                      "     "},
        {&GAP_2,                      "GAP_2",                      "  "},

        #if USING_WIDGET_DIRECTIVE
        {&SUNDAY,                     "SUNDAY",                     "Sunday"},
        {&MONDAY,                     "MONDAY",                     "Monday"},
        {&TUESDAY,                    "TUESDAY",                    "Tuesday"},
        {&WEDNESDAY,                  "WEDNESDAY",                  "Wednesday"},
        {&THURSDAY,                   "THURSDAY",                   "Thursday"},
        {&FRIDAY,                     "FRIDAY",                     "Friday"},
        {&SATURDAY,                   "SATURDAY",                   "Saturday"},
        {&JANUARY,                    "JANUARY",                    "January"},
        {&FEBRUARY,                   "FEBRUARY",                   "February"},
        {&MARCH,                      "MARCH",                      "March"},
        {&APRIL,                      "APRIL",                      "April"},
        {&MAY,                        "MAY",                        "May"},
        {&JUNE,                       "JUNE",                       "June"},
        {&JULY,                       "JULY",                       "July"},
        {&AUGUST,                     "AUGUST",                     "August"},
        {&SEPTEMBER,                  "SEPTEMBER",                  "September"},
        {&OCTOBER,                    "OCTOBER",                    "October"},
        {&NOVEMBER,                   "NOVEMBER",                   "November"},
        {&DECEMBER,                   "DECEMBER",                   "December"},
        {&SUN,                        "SUN",                        "Sun"},
        {&MON,                        "MON",                        "Mon"},
        {&TUE,                        "TUE",                        "Tue"},
        {&WED,                        "WED",                        "Wed"},
        {&THU,                        "THU",                        "Thu"},
        {&FRI,                        "FRI",                        "Fri"},
        {&SAT,                        "SAT",                        "Sat"},
        {&JAN,                        "JAN",                        "Jan"},
        {&FEB,                        "FEB",                        "Feb"},
        {&MAR,                        "MAR",                        "Mar"},
        {&APR,                        "APR",                        "Apr"},
        {&MAY_ABBR,                   "MAY_ABBR",                   "May"},
        {&JUN,                        "JUN",                        "Jun"},
        {&JUL,                        "JUL",                        "Jul"},
        {&AUG,                        "AUG",                        "Aug"},
        {&SEP,                        "SEP",                        "Sep"},
        {&OCT,                        "OCT",                        "Oct"},
        {&NOV,                        "NOV",                        "Nov"},
        {&DEC,                        "DEC",                        "Dec"},
        #endif
    };

    static constexpr size_t LANG_TABLE_SIZE = sizeof(LANG_TABLE) / sizeof(LANG_TABLE[0]);


    // --- reinitializeLangVars: just a loop now ---
    void reinitializeLangVars() {
        for (size_t i = 0; i < LANG_TABLE_SIZE; ++i)
            *LANG_TABLE[i].var = LANG_TABLE[i].defaultVal;
    }


    // --- parseLanguage: same loop, different source ---
    void parseLanguage(const std::string& langFile) {
        std::unordered_map<std::string, std::string> jsonMap;
        if (!parseJsonToMap(langFile, jsonMap)) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to parse language file: " + langFile);
            #endif
            return;
        }
        for (size_t i = 0; i < LANG_TABLE_SIZE; ++i) {
            auto it = jsonMap.find(LANG_TABLE[i].key);
            *LANG_TABLE[i].var = (it != jsonMap.end()) ? it->second : LANG_TABLE[i].defaultVal;
        }
    }


    // --- localizeTimeStr and applyLangReplacements unchanged ---
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
                case 6:  if (text == "Reboot")    { text = REBOOT;    } break;
                case 8:  if (text == "Shutdown")  { text = SHUTDOWN;  } break;
                case 9:  if (text == "Reboot To") { text = REBOOT_TO; } break;
                case 10: if (text == "Boot Entry"){ text = BOOT_ENTRY;} break;
            }
        }
        #endif
    }
    
    
    // Prepare a map of default settings
    const ThemeDefault defaultThemeSettings[] = {
        // Must stay sorted alphabetically for binary search
        {"bad_ram_text_color",              "FF0000"},
        {"banner_version_text_color",       "AAAAAA"},
        {"battery_charging_color",          "00FF00"},
        {"battery_color",                   "ffff45"},
        {"battery_low_color",               "FF0000"},
        {"bg_alpha",                        "13"},
        {"bg_color",                        "000000"},
        {"bottom_button_color",             "FFFFFF"},
        {"bottom_separator_color",          "FFFFFF"},
        {"bottom_text_color",               "FFFFFF"},
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
        {"selection_text_color",            "9ed0ff"},
        {"selection_value_text_color",      "FF7777"},
        {"separator_alpha",                 "15"},
        {"separator_color",                 "404040"},
        {"star_color",                      "FFFFFF"},
        {"table_bg_alpha",                  "14"},
        {"table_bg_color",                  "2C2C2C"},
        {"table_info_text_color",           "9ed0ff"},
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
        {"ult_overlay_text_color",          "9ed0ff"},
        {"ult_overlay_version_text_color",  "00FFDD"},
        {"ult_package_text_color",          "9ed0ff"},
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
    
    
    bool isValidHexColor(const std::string& s) {
        if (s.size() != 6) return false;
        for (char c : s) if (!isxdigit(c)) return false;
        return true;
    }
    
    
    std::atomic<bool> refreshWallpaperNow(false);
    std::atomic<bool> refreshWallpaper(false);
    std::vector<u8> wallpaperData; 
    std::atomic<bool> inPlot(false);
    
    std::mutex wallpaperMutex;
    std::condition_variable cv;
    
    bool loadRGBA8888toRGBA4444(const std::string& filePath, u8* dst, size_t srcSize) {
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

    
    void loadWallpaperFile(const std::string& filePath, s32 width, s32 height) {
        const size_t srcSize = width * height * 4;
        wallpaperData.resize(srcSize / 2);
        if (!isFile(filePath) ||
            !loadRGBA8888toRGBA4444(filePath, wallpaperData.data(), srcSize))
            wallpaperData.clear();
    }
    

    void loadWallpaperFileWhenSafe() {
        if (expandedMemory && !inPlot.load(std::memory_order_acquire) && !refreshWallpaper.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(wallpaperMutex);
            cv.wait(lock, [] { return !inPlot.load(std::memory_order_acquire) && !refreshWallpaper.load(std::memory_order_acquire); });
            if (wallpaperData.empty() && isFile(WALLPAPER_PATH)) {
                loadWallpaperFile(WALLPAPER_PATH);
            }
        }
    }


    void reloadWallpaper() {
        // Signal that wallpaper is being refreshed
        refreshWallpaper.store(true, std::memory_order_release);
        
        // Lock the mutex for condition waiting
        std::unique_lock<std::mutex> lock(wallpaperMutex);
        
        // Wait for inPlot to be false before reloading the wallpaper
        cv.wait(lock, [] { return !inPlot.load(std::memory_order_acquire); });
        
        // Clear the current wallpaper data
        wallpaperData.clear();
        
        // Reload the wallpaper file
        if (isFile(WALLPAPER_PATH)) {
            loadWallpaperFile(WALLPAPER_PATH);
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
        const uint64_t now_ns = armTicksToNs(armGetSystemTick());
        
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
    bool hideClock, hideBattery, hidePCBTemp, hideSOCTemp, dynamicWidgetColors;
    bool hideWidgetBackdrop, centerWidgetAlignment, extendedWidgetBackdrop;

    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeWidgetVars() {
        // Load INI data once instead of 8 separate file reads
        auto ultrahandSection = getKeyValuePairsFromSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME);
        
        // Helper lambda to safely get boolean values with proper defaults
        auto getBoolValue = [&](const std::string& key, bool defaultValue = false) -> bool {
            if (ultrahandSection.count(key) > 0) {
                return (ultrahandSection.at(key) != FALSE_STR);
            }
            return defaultValue;
        };
        
        // Set all values from the loaded section with correct defaults (matching initialization)
        hideClock = getBoolValue("hide_clock", false);                           // FALSE_STR default
        hideBattery = getBoolValue("hide_battery", true);                        // TRUE_STR default
        hideSOCTemp = getBoolValue("hide_soc_temp", true);                       // TRUE_STR default  
        hidePCBTemp = getBoolValue("hide_pcb_temp", true);                       // TRUE_STR default
        dynamicWidgetColors = getBoolValue("dynamic_widget_colors", true);       // TRUE_STR default
        hideWidgetBackdrop = getBoolValue("hide_widget_backdrop", false);        // FALSE_STR default
        centerWidgetAlignment = getBoolValue("center_widget_alignment", true);   // TRUE_STR default
        extendedWidgetBackdrop = getBoolValue("extended_widget_backdrop", false); // FALSE_STR default
    }
    #endif
    
    bool cleanVersionLabels, hideOverlayVersions, hidePackageVersions, useLibultrahandTitles, useLibultrahandVersions, usePackageTitles, usePackageVersions;
    


    
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
        
        // Helper lambda to safely get boolean values with proper defaults
        auto getBoolValue = [&](const std::string& key, bool defaultValue = false) -> bool {
            if (ultrahandSection.count(key) > 0) {
                return (ultrahandSection.at(key) != FALSE_STR);
            }
            return defaultValue;
        };
        
        // Set all values from the loaded section with correct defaults (matching initialization)
        cleanVersionLabels = getBoolValue("clean_version_labels", false);        // FALSE_STR default
        hideOverlayVersions = getBoolValue("hide_overlay_versions", false);      // FALSE_STR default  
        hidePackageVersions = getBoolValue("hide_package_versions", false);      // FALSE_STR default
    }
    #endif
    
    
    // Number of renderer threads to use
    const unsigned numThreads = 4;//expandedMemory ? 4 : 0;
    std::vector<std::thread> renderThreads(numThreads);

    void InPlotBarrierCompletion::operator()() noexcept {
        inPlot.store(false, std::memory_order_release);
    }
    std::barrier<InPlotBarrierCompletion> inPlotBarrier(numThreads);

    const s32 bmpChunkSize = ((720 + numThreads - 1) / numThreads);
    std::atomic<s32> currentRow;
}
