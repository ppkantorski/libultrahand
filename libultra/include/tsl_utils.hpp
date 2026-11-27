/********************************************************************************
 * File: tsl_utils.hpp
 * Author: ppkantorski
 * Description: 
 *   'tsl_utils.hpp' is a central utility header for the Ultrahand Overlay project,
 *   containing a variety of functions and definitions related to system status,
 *   input handling, and application-specific behavior on the Nintendo Switch.
 *   This header provides essential utilities for interacting with the system,
 *   managing key input, and enhancing overlay functionality.
 *
 *   The utilities defined here are designed to operate independently, facilitating
 *   robust system interaction capabilities required for custom overlays.
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


#pragma once

#include <stdio.h>
#include <ultra.hpp>
#include <switch.h>
#include <arm_neon.h>

#include <stdlib.h>
#include <strings.h>

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <string>
#include <functional>
#include <type_traits>
#include <mutex>
#include <memory>

#include <list>
#include <stack>
#include <map>


struct OverlayCombo {
    std::string path;   // full overlay path
    std::string launchArg; // empty = use per-overlay launch_args key, otherwise a “mode” arg
};

struct SwapDepth {
    u32 value;
    explicit SwapDepth(u32 v) : value(v) {}
};

namespace ult {

    static inline u64 nowNs() {
        return armTicksToNs(armGetSystemTick());
    }

    void formatTimestamp(time_t t, char* buf, size_t bufSize);

    // math funcs
    double cos(double x);
    
    extern bool correctFrameSize; // for detecting the correct Overlay display size

    extern u16 DefaultFramebufferWidth;            ///< Width of the framebuffer
    extern u16 DefaultFramebufferHeight;           ///< Height of the framebuffer

    extern bool windowedLayerPixelPerfect;

    extern std::unordered_map<std::string, std::string> translationCache;

    extern std::unordered_map<u64, OverlayCombo> g_entryCombos;
    extern std::atomic<bool> launchingOverlay;
    extern std::atomic<bool> settingsInitialized;
    extern std::atomic<bool> currentForeground;


    bool readFileContent(const std::string& filePath, std::string& content);
    void parseJsonContent(const std::string& content, std::unordered_map<std::string, std::string>& result);
    bool parseJsonToMap(const std::string& filePath, std::unordered_map<std::string, std::string>& result);

    bool loadTranslationsFromJSON(const std::string& filePath);

    // Function to clear the translation cache
    void clearTranslationCache();

    extern u16 activeHeaderHeight;   // where the frame's list starts (y), 1 px below its header

    bool consoleIsDocked();
    
    std::string getBuildIdAsString();

    std::string getTitleIdAsString();
    
    extern std::string lastTitleID;
    extern std::atomic<bool> resetForegroundCheck;

    /**
     * Foreground re-assert burst anchor (system tick; 0 = inactive).
     *
     * Armed whenever the overlay intentionally acquires foreground
     * (hlp::requestForeground(true) with updateGlobalFlag) and on HOME
     * button presses while the overlay is open and holding foreground.
     *
     * Rationale: when a suspended title is resumed from the HOME menu
     * (HOME pressed again, or the game re-selected), am re-arms the
     * application's input focus as part of the resume transition. If the
     * overlay was shown via combo right around that transition, its
     * hidsysEnableAppletToGetInput(false, app) claim is clobbered a
     * moment later and input flows into BOTH the game and the overlay.
     * The title-ID poll (resetForegroundCheck) never fires here because
     * resuming the same game does not change the title ID.
     *
     * While armed, the background poller re-asserts the foreground claim
     * every FOREGROUND_REASSERT_INTERVAL_NS for
     * FOREGROUND_REASSERT_WINDOW_NS, so the overlay reclaims exclusive
     * input at most one interval after the system re-arms the game.
     */
    extern std::atomic<u64> foregroundReassertStartTick;


    //extern bool isLauncher;
    extern std::atomic<bool> internalTouchReleased;
    extern u32 layerEdge;
    extern bool useRightAlignment;
    extern bool useSwipeToOpen;
    extern bool useTextBorder;   // thin black ring around all text but the banner logo (ini text_border)
    // Optional deadzone calibration for swipe-to-open ("swipe_offset" in
    // config.ini). Shifts the swipe start zone this many pixels inward from
    // the screen edge (leftward when opening from the right edge). Read-only
    // — never written back to config.ini. Default 0.
    extern s32 swipeOffset;
    extern bool useLaunchCombos;
    //extern bool useLaunchRecall;
    //extern bool usePageRecall;
    extern bool useNotifications;
    extern bool useNotificationsHotkey;
    extern bool useSwitch2Notifications;
    extern bool useStartupNotification;
    extern bool silenceNotifications;
    extern bool useSoundEffects;
    extern bool useButtonHaptics;
    extern bool useTouchHaptics;
    extern bool useAutoNTPSync;
    extern bool useStickNavigation;
    extern bool usePageSwap;
    extern std::atomic<bool> noClickableItems;
    
    extern bool useSwitch2Style;
    extern bool useFrostedGlass;  // ini frosted_glass
    extern bool glassWithCheats;  // glass in a game held by Atmosphere's cheat engine (ini glass_with_cheats)
    extern std::atomic<bool> glassFallback;   // glass released for the wallpaper: another debugger holds the game / vi
    extern bool useGlassLayer;    // full-colour (RGBA8888) output with glass on >8 MB heaps (ini full_color_glass)
    extern u8 glassDarkness;      // 0..12: highlight compression of the glass (ini glass_darkness)
    extern u8 glassBlurRadius;    // 1..7: box radius of the 3-pass blur, in 1/8 px (ini glass_blur_radius)
    extern u8 glassRed, glassGreen, glassBlue;  // 0..15: glass tint color (ini glass_red / glass_green / glass_blue), 0xFF: theme background color
    // Live logging (live_logging.log, frame-time lines): only in builds with
    // -DUSING_LIVE_LOGGING_DIRECTIVE=1. Without it none of the logging exists, so every
    // use has to sit inside #if USING_LIVE_LOGGING_DIRECTIVE.
#if USING_LIVE_LOGGING_DIRECTIVE
    extern bool glassLog;
#endif
    extern bool useDynamicLogo;
    extern bool useUniformLogo;   // "hand" in the "Ultra" colour; no dynamic logo while on
    extern bool useDynamicTableColors;
    extern bool useSelectionBG;
    extern bool useSelectionText;
    extern bool useSelectionValue;
    extern bool useUltrahandVersion;   // version | nx-ovlloader line under the logo (ini ultrahand_version)
    extern bool useBottomLine;         // line above the Back / OK footer (ini bottom_line)
    extern bool useDividerLines;       // lines between list items / trackbars + widget|logo divider (ini divider_lines)
    extern bool useEdgeLine;           // 1-px line on the frame's outer edge (ini edge_line)

    
#if IS_LAUNCHER_DIRECTIVE
    extern std::atomic<bool> overlayLaunchRequested;
    extern std::string requestedOverlayPath;
    extern std::string requestedOverlayArgs;
    extern std::mutex overlayLaunchMutex;

    // Invoked synchronously (same call, same thread) right before an "open" package command
    // requests an overlay launch, so main.cpp can snapshot the current PackageMenu return
    // context to disk. Left null (and simply skipped) until main.cpp registers it in main().
    // Plain function pointer (not std::function) so it is constant-initialized to nullptr —
    // no static-initialization-order concerns when it's assigned during main.cpp's own init.
    extern void (*openCommandInvokedCallback)();
#endif
    
    
    
    // CUSTOM SECTION START
    extern std::atomic<float> backWidth;
    extern std::atomic<float> selectWidth;
    extern std::atomic<float> nextPageWidth;
    extern std::atomic<bool> inMainMenu;
    extern std::atomic<bool> inHiddenMode;
    extern std::atomic<bool> inSettingsMenu;
    extern std::atomic<bool> inSubSettingsMenu;
    #if IS_LAUNCHER_DIRECTIVE
    extern bool useHomeShortcut;   // Settings > Home Shortcut: + / the logo on the settings page press HOME
    #endif
    extern std::atomic<bool> inOverlaysPage;
    extern std::atomic<bool> inPackagesPage;
    
    extern std::atomic<bool> hasNextPageButton;

    extern bool firstBoot; // for detecting first boot
    extern bool reloadingBoot; // for detedting reloading boots
    
    // Define an atomic bool for interpreter completion
    extern std::atomic<bool> threadFailure;
    extern std::atomic<bool> runningInterpreter;
    extern std::atomic<bool> shakingProgress;
    
    extern std::atomic<bool> isHidden;
    extern std::atomic<bool> externalAbortCommands;
    
    extern bool disableTransparency;
    extern bool useOpaqueScreenshots;
    
    extern std::atomic<bool> onTrackBar;
    extern std::atomic<bool> allowSlide;
    extern std::atomic<bool> unlockedSlide;
    
    void atomicToggle(std::atomic<bool>& b);

    /**
     * @brief Shutdown modes for the Ultrahand-Overlay project.
     *
     * These macros define the shutdown modes used in the Ultrahand-Overlay project:
     * - `SpsmShutdownMode_Normal`: Normal shutdown mode.
     * - `SpsmShutdownMode_Reboot`: Reboot mode.
     */
    #define SpsmShutdownMode_Normal 0
    #define SpsmShutdownMode_Reboot 1
    
    /**
     * @brief Key mapping macros for button keys.
     *
     * These macros define button keys for the Ultrahand-Overlay project to simplify key mappings.
     * For example, `KEY_A` represents the `HidNpadButton_A` key.
     */
    #define KEY_A HidNpadButton_A
    #define KEY_B HidNpadButton_B
    #define KEY_X HidNpadButton_X
    #define KEY_Y HidNpadButton_Y
    #define KEY_L HidNpadButton_L
    #define KEY_R HidNpadButton_R
    #define KEY_ZL HidNpadButton_ZL
    #define KEY_ZR HidNpadButton_ZR
    #define KEY_PLUS HidNpadButton_Plus
    #define KEY_MINUS HidNpadButton_Minus
    #define KEY_DUP HidNpadButton_Up
    #define KEY_DDOWN HidNpadButton_Down
    #define KEY_DLEFT HidNpadButton_Left
    #define KEY_DRIGHT HidNpadButton_Right
    #define KEY_SL HidNpadButton_AnySL
    #define KEY_SR HidNpadButton_AnySR
    #define KEY_LSTICK HidNpadButton_StickL
    #define KEY_RSTICK HidNpadButton_StickR
    #define KEY_UP    (ult::useStickNavigation ? HidNpadButton_AnyUp    : HidNpadButton_Up)
    #define KEY_DOWN  (ult::useStickNavigation ? HidNpadButton_AnyDown  : HidNpadButton_Down)
    #define KEY_LEFT  (ult::useStickNavigation ? HidNpadButton_AnyLeft  : HidNpadButton_Left)
    #define KEY_RIGHT (ult::useStickNavigation ? HidNpadButton_AnyRight : HidNpadButton_Right)
    
    #define SCRIPT_KEY HidNpadButton_Minus
    #define SYSTEM_SETTINGS_KEY HidNpadButton_Plus
    #define SETTINGS_KEY HidNpadButton_Y
    #define STAR_KEY HidNpadButton_X

    
    // Define a mask with all possible key flags
    #define ALL_KEYS_MASK (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_SL | KEY_SR | KEY_LSTICK | KEY_RSTICK | KEY_PLUS | KEY_MINUS)
    
    
    extern bool updateMenuCombos;
    
    /**
     * @brief Ultrahand-Overlay Input Macros
     *
     * This block of code defines macros for handling input in the Ultrahand-Overlay project.
     * These macros simplify the mapping of input events to corresponding button keys and
     * provide aliases for touch and joystick positions.
     *
     * The macros included in this block are:
     *
     * - `touchPosition`: An alias for a constant `HidTouchState` pointer.
     * - `touchInput`: An alias for `&touchPos`, representing touch input.
     * - `JoystickPosition`: An alias for `HidAnalogStickState`, representing joystick input.
     *
     * These macros are utilized within the Ultrahand-Overlay project to manage and interpret
     * user input, including touch and joystick events.
     */
    #define touchPosition const HidTouchState
    #define touchInput &touchPos
    #define JoystickPosition HidAnalogStickState
    
    //void convertComboToUnicode(std::string& combo);

    /**
     * @brief Combo key mapping
     */
    struct KeyInfo {
        u64 key;
        const char* name;
        const char* glyph;
    };

    /**
     * @brief Combo key mappings
     *
     * Ordered as they should be displayed
     */
    extern const std::array<KeyInfo, 18> KEYS_INFO;
    
    
    void convertComboToUnicode(std::string& combo);
    
    
    // English string definitions
    
    extern const std::string whiteColor;
    extern const std::string blackColor;
    extern const std::string greyColor;

    extern std::atomic<bool> languageWasChanged;
    
    inline constexpr double _M_PI = 3.14159265358979323846;  // For double precision
    inline constexpr double RAD_TO_DEG = 180.0f / _M_PI;
    
    #if IS_LAUNCHER_DIRECTIVE
    extern std::string ENGLISH;
    extern std::string SPANISH;
    extern std::string FRENCH;
    extern std::string GERMAN;
    extern std::string JAPANESE;
    extern std::string KOREAN;
    extern std::string ITALIAN;
    extern std::string DUTCH;
    extern std::string PORTUGUESE;
    extern std::string RUSSIAN;
    extern std::string UKRAINIAN;
    extern std::string POLISH;
    extern std::string SIMPLIFIED_CHINESE;
    extern std::string TRADITIONAL_CHINESE;

    extern std::string OVERLAYS; //defined in libTesla now
    extern std::string OVERLAYS_ABBR;
    extern std::string OVERLAY;
    extern std::string HIDDEN_OVERLAYS;
    extern std::string PACKAGES; //defined in libTesla now
    extern std::string PACKAGE;
    extern std::string HIDDEN_PACKAGES;
    extern std::string HIDDEN;
    extern std::string HIDE_OVERLAY;
    extern std::string HIDE_PACKAGE;
    extern std::string LAUNCH_ARGUMENTS;
    extern std::string FORCE_AMS110_SUPPORT;
    extern std::string QUICK_LAUNCH;
    extern std::string BOOT_COMMANDS;
    extern std::string EXIT_COMMANDS;
    extern std::string ERROR_LOGGING;
    extern std::string COMMANDS;
    extern std::string SETTINGS;
    extern std::string FAVORITE;
    extern std::string MAIN_SETTINGS;
    extern std::string UI_SETTINGS;

    extern std::string WIDGET;
    extern std::string WIDGET_ITEMS;
    extern std::string WIDGET_SETTINGS;
    extern std::string CLOCK;
    extern std::string BATTERY;
    extern std::string SOC_TEMPERATURE;
    extern std::string PCB_TEMPERATURE;
    extern std::string BACKDROP;
    extern std::string BORDER;
    extern std::string DYNAMIC_BORDER;
    extern std::string DYNAMIC_TEMPS;
    extern std::string CENTER_ALIGNMENT;
    extern std::string EXTENDED_BACKDROP;
    extern std::string MISCELLANEOUS;

    extern std::string INPUT_SETTINGS;
    extern std::string LAUNCH_COMBOS;
    extern std::string SWIPE_TO_OPEN;
    extern std::string BUTTON_HAPTICS;
    extern std::string TOUCH_HAPTICS;
    #if IS_LAUNCHER_DIRECTIVE
    extern std::string HOME_SHORTCUT;
    extern std::string NO_CONTROLLER_SLOT;
    #endif
    extern std::string STICK_NAVIGATION;
    extern std::string HOLD_DURATION;

    extern std::string FEATURE_SETTINGS;
    extern std::string OPAQUE_SCREENSHOTS;
    extern std::string RIGHT_SIDE_MODE;
    extern std::string NTP_SYNC_DOWNLOADS;

    extern std::string MENU_SETTINGS;
    extern std::string PACKAGES_MENU;
    extern std::string USER_GUIDE;
    extern std::string SHOW_HIDDEN;
    extern std::string SHOW_DELETE;
    extern std::string SHOW_UNSUPPORTED;
    extern std::string PAGE_SWAP;
    extern std::string OVERLAY_VERSIONS;
    extern std::string ULTRAHAND_VERSION;
    extern std::string PACKAGE_VERSIONS;
    extern std::string CLEAN_VERSIONS;

    extern std::string THEME_SETTINGS;
    extern std::string VERSION_SETTINGS;
    extern std::string LABEL_COLORS;
    extern std::string SWITCH_2_STYLE;
    extern std::string UNIFORM_LOGO;
    extern std::string TEXT_BORDER;
    extern std::string LIGHT;
    extern std::string COLOR;
    extern std::string EXPOSURE;
    extern std::string BRIGHTNESS;
    extern std::string HIGHLIGHTS;
    extern std::string SHADOWS;
    extern std::string CONTRAST;
    extern std::string BLACK_POINT;
    extern std::string SATURATION;
    extern std::string VIBRANCE;
    extern std::string WARMTH;
    extern std::string TINT;
    extern std::string TRANSPARENCY;
    extern std::string PRESETS;
    extern std::string TOOLS;
    extern std::string RESTORE_DEFAULTS;
    extern std::string RESTORE_DEFAULTS_INFO;
    extern std::string RESTORE_ROOT_PKG;
    extern std::string RESTORE_ROOT_PKG_INFO;
    extern std::string CLEAN_INIS;
    extern std::string CLEAN_INIS_INFO;
    extern std::string DOT_CLEAN_SD;
    extern std::string DOT_CLEAN_SD_INFO;
    extern std::string NTP_SYNC_TIME;
    extern std::string NTP_SYNC_TIME_INFO;
    extern std::string BLUR;
    extern std::string DARKNESS;
    extern std::string RED;
    extern std::string GREEN;
    extern std::string BLUE;
    extern std::string ENABLE;
    extern std::string USE_BACKGROUND_COLOR;
    extern std::string RESET;
    extern std::string ALL;
    extern std::string IMAGES;
    extern std::string VIDEOS;
    extern std::string FROSTED_GLASS;
    extern std::string GLASS_WITH_CHEATS;
    extern std::string GLASS_CHEATS_WARNING;
    extern std::string DYNAMIC_LOGO;
    extern std::string DYNAMIC_TABLES;
    extern std::string SELECTION_BACKGROUND;
    extern std::string SELECTION_TEXT;
    extern std::string SELECTION_VALUE;
    extern std::string BOTTOM_LINE;
    extern std::string DIVIDER_LINES;
    extern std::string EDGE_LINE;
    extern std::string LIBULTRAHAND_TITLES;
    extern std::string LIBULTRAHAND_VERSIONS;
    extern std::string PACKAGE_TITLES;
    extern std::string IN_PACKAGE_TITLES;

    extern std::string KEY_COMBO;
    extern std::string MODE;
    extern std::string LAUNCH_MODES;
    extern std::string LANGUAGE;
    extern std::string OVERLAY_INFO;
    extern std::string SOFTWARE_UPDATE;
    extern std::string UPDATE_ULTRAHAND;
    extern std::string SYSTEM;
    extern std::string DEVICE_INFO;
    extern std::string FIRMWARE;
    extern std::string BOOTLOADER;
    extern std::string HARDWARE;
    extern std::string MEMORY;
    extern std::string VENDOR;
    extern std::string MODEL;
    extern std::string STORAGE;

    extern std::string OVERLAY_MEMORY;
    extern std::string NOT_ENOUGH_MEMORY;
    extern std::string WALLPAPER_SUPPORT_DISABLED;
    //extern std::string SOUND_SUPPORT_DISABLED;
    extern std::string WALLPAPER_SUPPORT_ENABLED;
    extern std::string SOUND_SUPPORT_ENABLED;
    extern std::string FULL_COLOR_SUPPORT_ENABLED;
    extern std::string FULL_COLOR_SUPPORT_DISABLED;
    extern std::string EXIT_OVERLAY_SYSTEM;

    extern std::string ULTRAHAND_ABOUT;
    extern std::string ULTRAHAND_CREDITS_START;
    extern std::string ULTRAHAND_CREDITS_END;

    extern std::string LOCAL_IP;
    extern std::string WALLPAPER;
    extern std::string THEME;
    extern std::string SOUNDS;
    extern std::string DEFAULT;
    extern std::string ROOT_PACKAGE;
    extern std::string SORT_PRIORITY;
    extern std::string OPTIONS;
    extern std::string FAILED_TO_OPEN;

    extern std::string NOTIFICATIONS;
    extern std::string NOTIFICATION_SETTINGS;
    extern std::string SILENCE_NOTIFICATIONS;
    extern std::string STARTUP_NOTIFICATION;
    extern std::string MAX_SLOTS;
    extern std::string API_NOTIFICATIONS;
    extern std::string API_TOGGLE_HOTKEY;
    extern std::string DISMISS_NOTIFICATION;
    extern std::string API_TOGGLE;
    extern std::string CLICK;
    extern std::string TAP;
    extern std::string HOLD_FOR_4S;

    extern std::string PACKAGE_INFO;
    extern std::string _TITLE;
    extern std::string _VERSION;
    extern std::string _CREATOR;
    extern std::string _ABOUT;
    extern std::string _CREDITS;

    extern std::string SETTINGS_MENU;
    extern std::string SCRIPT_OVERLAY;
    extern std::string STAR_FAVORITE;
    extern std::string APP_SETTINGS;
    extern std::string ON_MAIN_MENU;
    extern std::string ON_A_COMMAND;
    extern std::string ON_OVERLAY_PACKAGE;

    extern std::string ULTRAHAND_HAS_STARTED;
    extern std::string ULTRAHAND_HAS_RESTARTED;
    extern std::string NEW_UPDATE_IS_AVAILABLE;

    extern std::string DELETE_PACKAGE;
    extern std::string DELETE_OVERLAY;
    extern std::string SELECTION_IS_EMPTY;
    extern std::string FORCED_SUPPORT_WARNING;

    extern std::string TASK_IS_COMPLETE;
    extern std::string TASK_HAS_FAILED;

    extern std::string REBOOT_TO;
    extern std::string REBOOT;
    extern std::string SHUTDOWN;
    extern std::string BOOT_ENTRY;
    extern std::string INI_ENTRY;
    #endif

    extern std::string INCOMPATIBLE_WARNING;
    extern std::string OVERLAY_DOES_NOT_EXIST;
    extern std::string SYSTEM_RAM;
    extern std::string FREE;
    
    extern std::string UNAVAILABLE_SELECTION;

    extern std::string ON;
    extern std::string OFF;

    extern std::string OK;
    extern std::string BACK;
    extern std::string HIDE;
    extern std::string CANCEL;

    extern std::string GAP_1;
    extern std::string GAP_2;
    extern std::string API_NOTIFICATIONS_ENABLED;    // the notifications hotkey (any overlay)
    extern std::string API_NOTIFICATIONS_DISABLED;
    extern std::atomic<float> halfGap;

    #if USING_WIDGET_DIRECTIVE
    extern std::string SUNDAY;
    extern std::string MONDAY;
    extern std::string TUESDAY;
    extern std::string WEDNESDAY;
    extern std::string THURSDAY;
    extern std::string FRIDAY;
    extern std::string SATURDAY;
    
    extern std::string JANUARY;
    extern std::string FEBRUARY;
    extern std::string MARCH;
    extern std::string APRIL;
    extern std::string MAY;
    extern std::string JUNE;
    extern std::string JULY;
    extern std::string AUGUST;
    extern std::string SEPTEMBER;
    extern std::string OCTOBER;
    extern std::string NOVEMBER;
    extern std::string DECEMBER;
    
    extern std::string SUN;
    extern std::string MON;
    extern std::string TUE;
    extern std::string WED;
    extern std::string THU;
    extern std::string FRI;
    extern std::string SAT;
    
    extern std::string JAN;
    extern std::string FEB;
    extern std::string MAR;
    extern std::string APR;
    extern std::string MAY_ABBR;
    extern std::string JUN;
    extern std::string JUL;
    extern std::string AUG;
    extern std::string SEP;
    extern std::string OCT;
    extern std::string NOV;
    extern std::string DEC;
    #endif
    
    // Constant string definitions (English)
    void reinitializeLangVars();
    
    
    void parseLanguage(const std::string& langFile);
    
    #if USING_WIDGET_DIRECTIVE
    void localizeTimeStr(char* timeStr);
    #endif

    // Unified function to apply replacements
    void applyLangReplacements(std::string& text, bool isValue = false);
    
    
    // Predefined hexMap
    inline constexpr std::array<u8, 256> hexMap = [] {
        std::array<u8, 256> map = {0};
        map['0'] = 0; map['1'] = 1; map['2'] = 2; map['3'] = 3; map['4'] = 4;
        map['5'] = 5; map['6'] = 6; map['7'] = 7; map['8'] = 8; map['9'] = 9;
        map['A'] = 10; map['B'] = 11; map['C'] = 12; map['D'] = 13; map['E'] = 14; map['F'] = 15;
        map['a'] = 10; map['b'] = 11; map['c'] = 12; map['d'] = 13; map['e'] = 14; map['f'] = 15;
        return map;
    }();
            
    
    extern std::atomic<bool> refreshWallpaperNow;
    extern std::atomic<bool> refreshWallpaper;
    extern std::atomic<bool> refreshCombos;
    // The wallpaper's pixels, used like a std::vector<u8> (clear() keeps the memory). Unlike it,
    // resize() reports a failed allocation (size 0, false) instead of aborting the overlay, growing
    // doesn't keep the old bytes (every caller fills it afresh), and on 6 MB and up the memory is
    // part of the block shared with the frosted glass (glass::sharedAlloc).
    class WallpaperBuffer {
    public:
        WallpaperBuffer() = default;
        WallpaperBuffer(const WallpaperBuffer&) = delete;
        WallpaperBuffer& operator=(const WallpaperBuffer&) = delete;
        ~WallpaperBuffer() { release(); }

        u8*       data()        { return m_data; }
        const u8* data()  const { return m_data; }
        size_t    size()  const { return m_size; }
        bool      empty() const { return m_size == 0; }
        void      clear()       { m_size = 0; }
        bool      resize(size_t n);                // new bytes are zero, as with a vector
        void      release();                       // memory freed (or back to the shared block)
    private:
        u8*    m_data = nullptr;
        size_t m_size = 0, m_cap = 0;
    };
    extern WallpaperBuffer wallpaperData;
    extern bool fullColorWallpaper;               // full-colour output: wallpaper kept as 8-bit pre-multiplied RGB
    extern std::atomic<u32> wallpaperGeneration;  // bumped on every wallpaper (re)load / free
    extern std::atomic<bool> inPlot;
    
    extern std::mutex wallpaperMutex;
    extern std::condition_variable cv;
    
    
    
    // Function to load the RGBA file into memory and modify wallpaperData directly
    struct WallAdj;   // image_fx.hpp
    // adj (optional): the wallpaper adjustments, applied to the 8-bit colour before it is
    // cut to 4 bits (wallpapers at 6 / 8 MB).
    bool loadRGBA8888toRGBA4444(const std::string& filePath, u8* dst, size_t srcSize, const WallAdj* adj = nullptr);
    void loadWallpaperFile(const std::string& filePath, s32 width = 448, s32 height = 720);
    void loadWallpaperFileWhenSafe();
    // wallpaper.ini (config/ultrahand): [wallpaper] animated=<.uaw path> (the active animated
    // wallpaper, empty for none) and per wallpaper [<name>] adjustments (keys below, -100..100),
    // the Light and Color adjustments of the Ultrahand Wallpaper designer. Drawn in full colour
    // (8 MB heap and up) only.
    const std::string& wallpaperIniPath();
    enum WallpaperAdjust : u8 {
        WA_EXPOSURE, WA_BRIGHTNESS, WA_HIGHLIGHTS, WA_SHADOWS, WA_CONTRAST, WA_BLACK_POINT,   // Light
        WA_SATURATION, WA_VIBRANCE, WA_WARMTH, WA_TINT,                                         // Color
        WA_COUNT
    };
    extern const char* const kWallpaperAdjustKeys[WA_COUNT];   // wallpaper.ini keys
    extern std::string currentWallpaperName;          // config.ini current_wallpaper (parseOverlaySettings)
    extern std::atomic<s8> wallpaperAdjust[WA_COUNT]; // current wallpaper's adjustments (all 0 = none)
    // Current wallpaper's own background alpha, 0..15 (wallpaper.ini "alpha"): replaces the
    // theme's bg alpha under the wallpaper, with the same rules (fade, opaque screenshots).
    // -1 means no named wallpaper (e.g. an overlay's own UI-override one): use the theme's.
    extern std::atomic<s8> wallpaperAlpha;
    inline constexpr s8 kDefaultWallpaperAlpha = 13;   // the default theme's bg_alpha
    #if IS_LAUNCHER_DIRECTIVE
    // Decodes a PNG (width x height, non-interlaced) into a plain RGBA8888 file at outPath
    // (written to outPath.tmp, then renamed) and, with wallDst, into the in-memory
    // wallpaper format too. False, with outPath untouched, on any error.
    bool convertPngToRgba(const std::string& pngPath, const std::string& outPath, u32 width = 448, u32 height = 720, u8* wallDst = nullptr);
    // Sets a PNG as the wallpaper in one pass (file + memory); false keeps the current one.
    bool setWallpaperFromPng(const std::string& pngPath);
    #endif

    void reloadWallpaper();
    // Frosted glass switched on with a wallpaper shown: the wallpaper is freed first (glass and
    // wallpaper share one block of memory, glass::sharedAlloc). Until this time, or the first glass
    // frame, the overlay loop presents nothing, so the last frame with the wallpaper stays up
    // instead of a blank one.
    extern std::atomic<u64> glassStartPauseUntilNs;
    
    
    extern std::atomic<bool> themeIsInitialized;

    // Variables for touch commands
    extern std::atomic<bool> touchingBack;
    extern std::atomic<bool> touchingSelect;
    extern std::atomic<bool> touchingNextPage;
    extern std::atomic<bool> touchingMenu;
    extern std::atomic<bool> shortTouchAndRelease;
    extern std::atomic<bool> longTouchAndRelease;
    extern std::atomic<bool> simulatedBack;
    extern std::atomic<bool> simulatedSelect;
    extern std::atomic<bool> simulatedNextPage;
    extern std::atomic<bool> simulatedMenu;
    extern std::atomic<bool> stillTouching;
    extern std::atomic<bool> interruptedTouch;
    extern std::atomic<bool> touchInBounds;
    
    
#if USING_WIDGET_DIRECTIVE
    // Battery implementation
    extern bool powerInitialized;
    extern bool powerCacheInitialized;
    extern uint32_t powerCacheCharge;
    extern bool powerCacheIsCharging;
    extern PsmSession powerSession;
    
    extern std::atomic<uint32_t> batteryCharge;
    extern std::atomic<bool> isCharging;
    
    bool powerGetDetails(uint32_t *_batteryCharge, bool *_isCharging);
    
    void powerInit(void);
    
    void powerExit(void);
#endif
    
    // Temperature Implementation
    extern std::atomic<float> PCB_temperature;
    extern std::atomic<float> SOC_temperature;
    
    /*
    I2cReadRegHandler was taken from Switch-OC-Suite source code made by KazushiMe
    Original repository link (Deleted, last checked 15.04.2023): https://github.com/KazushiMe/Switch-OC-Suite
    */
    
    Result I2cReadRegHandler(u8 reg, I2cDevice dev, u16 *out);
    
    
    #define TMP451_SOC_TEMP_REG 0x01  // Register for SOC temperature integer part
    #define TMP451_SOC_TMP_DEC_REG 0x10  // Register for SOC temperature decimal part
    #define TMP451_PCB_TEMP_REG 0x00  // Register for PCB temperature integer part
    #define TMP451_PCB_TMP_DEC_REG 0x15  // Register for PCB temperature decimal part
    
    // Common helper function to read temperature (integer and fractional parts)
    Result ReadTemperature(float *temperature, u8 integerReg, u8 fractionalReg, bool integerOnly);
    
    // Function to get the SOC temperature
    Result ReadSocTemperature(float *temperature, bool integerOnly = true);
    
    // Function to get the PCB temperature
    Result ReadPcbTemperature(float *temperature, bool integerOnly = true);
    
    
    
    // Time implementation
    
    extern const std::string DEFAULT_DT_FORMAT;
    extern std::string datetimeFormat;
    
    
    // Widget settings
    extern bool hideClock, hideBattery, hidePCBTemp, hideSOCTemp, dynamicWidgetColors, dynamicWidgetBorder;
    extern bool hideWidgetBackdrop, hideWidgetBorder, centerWidgetAlignment, extendedWidgetBackdrop;

    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeWidgetVars();
    #endif
    
    extern bool cleanVersionLabels, hideOverlayVersions, hidePackageVersions, useLibultrahandTitles, useLibultrahandVersions, usePackageTitles, usePackageVersions, useInPackageTitles;
    


    // nx-ovlloader settings
    enum class OverlayHeapSize : u64 {
        Size_4MB = 0x400000,
        Size_6MB = 0x600000,
        Size_8MB = 0x800000
    };
    
    // Static cache
    struct HeapSizeCache {
        bool initialized = false;
        OverlayHeapSize cachedSize = OverlayHeapSize::Size_6MB;
        u64 customSizeMB = 0;
    };
    extern HeapSizeCache heapSizeCache;
    

    // Helper function to convert MB to bytes
    extern u64 mbToBytes(u32 mb);
    
    // Helper function to convert bytes to MB
    extern u32 bytesToMB(u64 bytes);

    // Implementation
    OverlayHeapSize getCurrentHeapSize();
    
    extern OverlayHeapSize currentHeapSize;
    
    bool setOverlayHeapSize(OverlayHeapSize heapSize);
    
    // Implementation
    bool requestOverlayExit();

    bool requestOverlayReload();

    extern const std::string loaderInfo;
    extern std::string loaderTitle;
    extern bool expandedMemory;
    extern bool furtherExpandedMemory;
    extern bool limitedMemory;
    
    extern std::string versionLabel;
    
    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeVersionLabels();
    #endif
    
    
    // Number of renderer helper threads (tsl::gfx::Renderer::runParallel)
    extern const unsigned numThreads;
    

    void initializeThemeVars();
    
    void initializeUltrahandSettings();


}