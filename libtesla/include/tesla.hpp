/********************************************************************************
 * Custom Fork Information
 * 
 * File: tesla.hpp
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

/**
 * Copyright (C) 2020 werwolv
 *
 * This file is part of libtesla.
 *
 * libtesla is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * libtesla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libtesla.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"


// ── Tesla targeted optimizations ─────────────────────────────
#ifdef TESLA_TARGETED_SPEED
    #define TESLA_OPT_SPEED_PUSH _Pragma("GCC push_options") _Pragma("GCC optimize(\"O3\")")
    #define TESLA_OPT_SPEED_POP  _Pragma("GCC pop_options")
#else
    #define TESLA_OPT_SPEED_PUSH
    #define TESLA_OPT_SPEED_POP
#endif

#ifdef TESLA_TARGETED_SIZE
    #define TESLA_OPT_SIZE_PUSH _Pragma("GCC push_options") _Pragma("GCC optimize(\"Os\")")
    #define TESLA_OPT_SIZE_POP  _Pragma("GCC pop_options")
#else
    #define TESLA_OPT_SIZE_PUSH
    #define TESLA_OPT_SIZE_POP
#endif


#include <ultra.hpp>
#include <switch.h>
#include <arm_neon.h>

#include <strings.h>
#include <math.h>

#if !IS_LAUNCHER_DIRECTIVE
#include <filesystem> // unused, but preserved for projects that might need it
#endif

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <string>
#include <functional>
#include <type_traits>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <list>
#include <stack>
#include <map>



// Define this makro before including tesla.hpp in your main file. If you intend
// to use the tesla.hpp header in more than one source file, only define it once!

#ifdef TESLA_INIT_IMPL
    #define STB_TRUETYPE_IMPLEMENTATION
#endif
#include "stb_truetype.h"


#define ELEMENT_BOUNDS(elem) elem->getX(), elem->getY(), elem->getWidth(), elem->getHeight()

#define ASSERT_EXIT(x) if (R_FAILED(x)) std::exit(1)
#define ASSERT_FATAL(x) if (Result res = x; R_FAILED(res)) fatalThrow(res)

#define PACKED __attribute__((packed))
#define ALWAYS_INLINE inline __attribute__((always_inline))

/// Evaluates an expression that returns a result, and returns the result if it would fail.
#define TSL_R_TRY(resultExpr)           \
    ({                                  \
        const auto result = resultExpr; \
        if (R_FAILED(result)) {         \
            return result;              \
        }                               \
    })

using namespace std::literals::string_literals;
using namespace std::literals::chrono_literals; // potentially unused, restored for softare compatibility

#if IS_STATUS_MONITOR_DIRECTIVE

struct KeyPairHash {
    std::size_t operator()(const std::pair<int, float>& key) const {
        // Combine hashes of both components
        union returnValue {
            char c[8];
            std::size_t s;
        } value;
        memcpy(&value.c[0], &key.first, 4);
        memcpy(&value.c[4], &key.second, 4);
        return value.s;
    }
};

// Custom equality comparison for int-float pairs
struct KeyPairEqual {
    bool operator()(const std::pair<int, float>& lhs, const std::pair<int, float>& rhs) const {
        return lhs.first == rhs.first && 
            std::abs(lhs.second - rhs.second) < 0.00001f;
    }
};

inline u8 TeslaFPS = 60;
inline std::atomic<bool> triggerExitNow{false};
inline std::atomic<bool> isRendering{false};
inline std::atomic<bool> delayUpdate{false};
inline std::atomic<bool> pendingExit{false};
inline std::atomic<bool> wasRendering{false};

inline LEvent renderingStopEvent;
inline bool FullMode = true;
inline bool deactivateOriginalFooter = false;
inline bool disableJumpTo = false;

inline std::string lastMode;
inline std::set<std::string> overlayModes = {"full", "mini", "micro", "fps_graph", "fps_counter", "game_resolutions"};

inline bool isValidOverlayMode() {
    return overlayModes.count(lastMode) > 0;
}

#endif

#if USING_FPS_INDICATOR_DIRECTIVE
inline float fps = 0.0;
inline int frameCount = 0;
inline double elapsedTime = 0.0;
#endif


// Custom variables
inline std::atomic<bool> jumpToTop{false};
inline std::atomic<bool> jumpToBottom{false};
inline std::atomic<bool> skipUp{false};
inline std::atomic<bool> skipDown{false};
inline u32 offsetWidthVar = 112;
inline std::string lastOverlayFilename;
inline std::string lastOverlayMode;

static inline std::string returnOverlayPath{ult::OVERLAY_PATH + "ovlmenu.ovl"};
inline bool skipRumbleDoubleClick{false};
inline bool skipRumbleClick{false};

inline std::mutex jumpItemMutex;
inline std::string jumpItemName;
inline std::string jumpItemValue;
inline std::atomic<bool> jumpItemExactMatch{true};

inline std::atomic<bool> s_onLeftPage{false};
inline std::atomic<bool> s_onRightPage{false};
inline std::atomic<bool> screenshotsAreDisabled{false};
inline std::atomic<bool> screenshotsAreForceDisabled{false};

inline bool hideHidden = false;
inline bool usingUnfocusedColor = true;
inline bool bypassUnfocused = false;

inline std::atomic<bool> mainComboHasTriggered{false};
inline std::atomic<bool> launchComboHasTriggered{false};


// Signal helpers.
inline LEvent hapticsEvent;   // wakes the dedicated haptics thread
inline LEvent soundEvent;     // wakes the dedicated sound thread
inline void signalHaptics()  { leventSignal(&hapticsEvent); }
inline void signalSound()    { leventSignal(&soundEvent); }
inline void signalFeedback() { leventSignal(&hapticsEvent); leventSignal(&soundEvent); }

inline std::atomic<bool> feedbackPollerStop{false};
inline std::atomic<bool> hidReinitInProgress{false};

// Sound triggering variables
inline std::atomic<bool> triggerNavigationSound{false};
inline std::atomic<bool> triggerEnterSound{false};
inline std::atomic<bool> triggerExitSound{false};
inline std::atomic<bool> triggerWallSound{false};
inline std::atomic<bool> triggerOnSound{false};
inline std::atomic<bool> triggerOffSound{false};
inline std::atomic<bool> triggerSettingsSound{false};
inline std::atomic<bool> triggerMoveSound{false};
inline std::atomic<bool> triggerNotificationSound{false};
inline std::atomic<bool> disableSound{false};
inline std::atomic<bool> disableHaptics{false};
inline std::atomic<bool> reloadIfDockedChangedNow{false};
inline std::atomic<bool> reloadSoundCacheNow{false};

// Haptic triggering variables
inline std::atomic<bool> triggerInitHaptics{false};
inline std::atomic<bool> triggerRumbleClick{false};
inline std::atomic<bool> triggerRumbleDoubleClick{false};


__attribute__((noinline)) static void triggerFeedbackImpl(
        std::atomic<bool>& rumble, std::atomic<bool>& sound) {
    rumble.store(true, std::memory_order_release);
    sound.store(true, std::memory_order_release);
    signalFeedback();
}
inline void triggerNavigationFeedback()                   { triggerFeedbackImpl(triggerRumbleClick, triggerNavigationSound); }
inline void triggerWallFeedback(bool doubleClick = false) { triggerFeedbackImpl(!doubleClick ? triggerRumbleClick : triggerRumbleDoubleClick, triggerWallSound); }
inline void triggerEnterFeedback()                        { triggerFeedbackImpl(triggerRumbleClick, triggerEnterSound); }
inline void triggerExitFeedback()                         { triggerFeedbackImpl(triggerRumbleDoubleClick, triggerExitSound); }
inline void triggerOnFeedback()                           { triggerFeedbackImpl(triggerRumbleClick, triggerOnSound); }
inline void triggerOffFeedback(bool doubleClick = false)  { triggerFeedbackImpl(!doubleClick ? triggerRumbleClick : triggerRumbleDoubleClick, triggerOffSound); }
inline void triggerSettingsFeedback()                     { triggerFeedbackImpl(triggerRumbleClick, triggerSettingsSound); }
inline void triggerMoveFeedback(bool doubleClick = false) { triggerFeedbackImpl(!doubleClick ? triggerRumbleClick : triggerRumbleDoubleClick, triggerMoveSound); }

// Rumble-only helpers (no paired sound) — store + signal in one call so the
// poller is always woken immediately regardless of its current sleep timeout.
inline void triggerRumbleClickFeedback() {
    triggerRumbleClick.store(true, std::memory_order_release);
    signalHaptics();
}
inline void triggerRumbleDoubleClickFeedback() {
    triggerRumbleDoubleClick.store(true, std::memory_order_release);
    signalHaptics();
}


/**
 * @brief Checks if an NRO file uses new libnx (has LNY2 tag).
 *
 * @param filePath The path to the NRO file.
 * @return true if the file uses new libnx (LNY2 present), false otherwise.
 * Defined in tesla.cpp — file I/O + malloc body is too large to inline.
 */
bool usingLNY2(const std::string& filePath);

/**
 * @brief Checks if the current AMS version is at least the specified version.
 *
 * @param major Minimum major version required
 * @param minor Minimum minor version required  
 * @param patch Minimum patch version required
 * @return true if current AMS version >= specified version, false otherwise
 */
inline bool amsVersionAtLeast(uint8_t major, uint8_t minor, uint8_t patch) {
    u64 packed_version;
    if (R_FAILED(splGetConfig((SplConfigItem)65000, &packed_version))) {
        return false;
    }
    
    return ((packed_version >> 40) & 0xFFFFFF) >= static_cast<u32>((major << 16) | (minor << 8) | patch);
}

inline bool requiresLNY2 = false;


namespace tsl {

    // Shared static specialChars vectors — avoids duplicate static init at each call site
    inline const std::vector<std::string> s_dividerSpecialChars = {ult::DIVIDER_SYMBOL};
    inline const std::vector<std::string> s_footerSpecialChars  = {"\uE0E1","\uE0E0","\uE0ED","\uE0EE","\uE0E5"};

    // Windowed-mode notification Y offset in touch space.
    // Set to (g_win_pos_y * 2/3) by windowed overlay; 0 in normal mode.
    // X is handled by ult::layerEdge which already exists for this purpose.
    inline s32 layerEdgeY = 0;

    // Booleans
    inline std::atomic<bool> clearGlyphCacheNow(false);

    // Constants
    namespace cfg {
        
        constexpr u32 ScreenWidth = 1920;       ///< Width of the Screen
        constexpr u32 ScreenHeight = 1080;      ///< Height of the Screen
        constexpr u32 LayerMaxWidth = 1280;
        constexpr u32 LayerMaxHeight = 720;
        
        extern u16 LayerWidth;                  ///< Width of the Tesla layer
        extern u16 LayerHeight;                 ///< Height of the Tesla layer
        extern u16 LayerPosX;                   ///< X position of the Tesla layer
        extern u16 LayerPosY;                   ///< Y position of the Tesla layer
        extern u16 FramebufferWidth;            ///< Width of the framebuffer
        extern u16 FramebufferHeight;           ///< Height of the framebuffer
        extern u64 launchCombo;                 ///< Overlay activation key combo
        extern u64 launchCombo2;                ///< Overlay activation key combo
        
    }
    
    /**
     * @brief RGBA4444 Color structure
     */
    struct Color {
        union {
            struct {
                u16 r: 4, g: 4, b: 4, a: 4;
            } PACKED;
            u16 rgba;
        };
        
        constexpr inline Color() : rgba(0) {}
        constexpr inline Color(u16 raw) : rgba(raw) {}
        constexpr inline Color(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a) {}
    };
    
    // Ultra-fast version - zero variables, optimized calculations
    inline constexpr Color GradientColor(float temperature) {
        if (temperature <= 35.0f) return Color(7, 7, 15, 0xF);
        if (temperature >= 65.0f) return Color(15, 0, 0, 0xF);
        
        if (temperature < 45.0f) {
            // Single calculation, avoid repetition
            const float factor = (temperature - 35.0f) * 0.1f;
            return Color(7 - 7 * factor, 7 + 8 * factor, 15 - 15 * factor, 0xF);
        }
        
        if (temperature < 55.0f) {
            return Color(15 * (temperature - 45.0f) * 0.1f, 15, 0, 0xF);
        }
        
        return Color(15, 15 - 15 * (temperature - 55.0f) * 0.1f, 0, 0xF);
    }


    // Ultra-fast version - single variable, minimal branching
    inline Color RGB888(const std::string& hexColor, size_t alpha = 15, const std::string& defaultHexColor = ult::whiteColor) {
        const char* h = hexColor.size() == 6 ? hexColor.data() :
                        hexColor.size() == 7 && hexColor[0] == '#' ? hexColor.data() + 1 :
                        defaultHexColor.data();
        
        return Color(
            (ult::hexMap[h[0]] << 4 | ult::hexMap[h[1]]) >> 4,
            (ult::hexMap[h[2]] << 4 | ult::hexMap[h[3]]) >> 4,
            (ult::hexMap[h[4]] << 4 | ult::hexMap[h[5]]) >> 4,
            alpha
        );
    }
    
    inline Color lerpColor(const Color& c1, const Color& c2, float t) {
        return {
            static_cast<u8>((c1.r - c2.r) * t + c2.r + 0.5f),
            static_cast<u8>((c1.g - c2.g) * t + c2.g + 0.5f),
            static_cast<u8>((c1.b - c2.b) * t + c2.b + 0.5f),
            0xF
        };
    }

    
    namespace style {
        constexpr u32 ListItemDefaultHeight         = 70;       ///< Standard list item height
        constexpr u32 MiniListItemDefaultHeight     = 40;       ///< Mini list item height
        constexpr u32 TrackBarDefaultHeight         = 83;       ///< Standard track bar height
        constexpr u8  ListItemHighlightSaturation   = 7;        ///< Maximum saturation of Listitem highlights
        constexpr u8  ListItemHighlightLength       = 22;       ///< Maximum length of Listitem highlights
        
        namespace color {
            constexpr Color ColorFrameBackground  = { 0x0, 0x0, 0x0, 0xD };   ///< Overlay frame background color
            constexpr Color ColorTransparent      = { 0x0, 0x0, 0x0, 0x0 };   ///< Transparent color
            constexpr Color ColorHighlight        = { 0x0, 0xF, 0xD, 0xF };   ///< Greenish highlight color
            constexpr Color ColorFrame            = { 0x7, 0x7, 0x7, 0x7 };   ///< Outer boarder color
            constexpr Color ColorHandle           = { 0x5, 0x5, 0x5, 0xF };   ///< Track bar handle color
            constexpr Color ColorText             = { 0xF, 0xF, 0xF, 0xF };   ///< Standard text color
            constexpr Color ColorDescription      = { 0xA, 0xA, 0xA, 0xF };   ///< Description text color
            constexpr Color ColorHeaderBar        = { 0xC, 0xC, 0xC, 0xF };   ///< Category header rectangle color
            constexpr Color ColorClickAnimation   = { 0x0, 0x2, 0x2, 0xF };   ///< Element click animation color
        }
    }

    inline bool overrideBackButton = false; // for properly overriding the automatic "go back" functionality of KEY_B button presses
    inline bool disableHiding = false; // for manually disabling the hide overlay functionality
    inline std::atomic<bool> homeButtonPressedInGame{false}; ///< Set by the background poller when home fires during a game session (disableHiding=true); consumed each frame by the player GUI to release foreground.

    // Theme color variable definitions — defined once in tesla.cpp
    extern Color logoColor1;
    extern Color logoColor2;

    extern size_t defaultBackgroundAlpha;

    extern Color defaultBackgroundColor;
    extern Color defaultTextColor;
    extern Color notificationTextColor;
    extern Color notificationTitleColor;
    extern Color notificationTimeColor;
    extern Color headerTextColor;
    extern Color headerSeparatorColor;
    extern Color starColor;
    extern Color selectionStarColor;
    extern Color buttonColor;
    extern Color bottomTextColor;
    extern Color bottomSeparatorColor;
    extern Color unfocusedColor;
    extern Color topSeparatorColor;

    extern Color defaultOverlayColor;
    extern Color defaultPackageColor;
    extern Color defaultScriptColor;
    extern Color clockColor;
    extern Color temperatureColor;
    extern Color batteryColor;
    extern Color batteryChargingColor;
    extern Color batteryLowColor;
    extern size_t widgetBackdropAlpha;
    extern Color widgetBackdropColor;

    extern Color overlayTextColor;
    extern Color ultOverlayTextColor;
    extern Color packageTextColor;
    extern Color ultPackageTextColor;

    extern Color bannerVersionTextColor;
    extern Color overlayVersionTextColor;
    extern Color ultOverlayVersionTextColor;
    extern Color packageVersionTextColor;
    extern Color ultPackageVersionTextColor;
    extern Color onTextColor;
    extern Color offTextColor;

    //#if IS_LAUNCHER_DIRECTIVE
    extern Color dynamicLogoRGB1;
    extern Color dynamicLogoRGB2;
    //#endif

    extern bool invertBGClickColor;

    extern size_t selectionBGAlpha;
    extern Color selectionBGColor;

    extern Color highlightColor1;
    extern Color highlightColor2;
    extern Color highlightColor3;
    extern Color highlightColor4;

    extern Color s_highlightColor;

    extern size_t clickAlpha;
    extern Color clickColor;

    extern size_t progressAlpha;
    extern Color progressColor;

    extern Color scrollBarColor;
    extern Color scrollBarWallColor;

    extern size_t separatorAlpha;
    extern Color separatorColor;
    extern const Color edgeSeparatorColor;

    extern Color textSeparatorColor;

    extern Color selectedTextColor;
    extern Color selectedValueTextColor;
    extern Color inprogressTextColor;
    extern Color invalidTextColor;
    extern Color clickTextColor;

    extern size_t tableBGAlpha;
    extern Color tableBGColor;
    extern Color sectionTextColor;
    extern Color infoTextColor;
    extern Color warningTextColor;

    extern Color healthyRamTextColor;
    extern Color neutralRamTextColor;
    extern Color badRamTextColor;

    extern Color trackBarSliderColor;
    extern Color trackBarSliderBorderColor;
    extern Color trackBarSliderMalleableColor;
    extern Color trackBarFullColor;
    extern Color trackBarEmptyColor;

    // Prepare a map of default settings
    struct ThemeDefault { const char* key; const char* value; };
    extern const ThemeDefault defaultThemeSettings[];
    extern const size_t defaultThemeSettingsCount;
    const char* getThemeDefault(const char* key);
    
    bool isValidHexColor(std::string_view hexColor);

    // Defined in tesla.cpp — reads theme INI and populates all color vars above
    void initializeThemeVars();

    void initializeTheme(const std::string& themeIniPath = ult::THEME_CONFIG_INI_PATH);

    extern std::vector<std::string> wrapText(
        const std::string& text,
        float maxWidth,
        const std::string& wrappingMode,
        bool useIndent,
        const std::string& indent,
        float indentWidth,
        size_t fontSize
    );
        
    // Declarations
    
    /**
     * @brief Direction in which focus moved before landing on
     *        the currently focused element
     */
    enum class FocusDirection {
        None,                       ///< Focus was placed on the element programatically without user input
        Up,                         ///< Focus moved upwards
        Down,                       ///< Focus moved downwards
        Left,                       ///< Focus moved from left to rigth
        Right                       ///< Focus moved from right to left
    };
    
    /**
     * @brief Current input controll mode
     *
     */
    enum class InputMode {
        Controller,                 ///< Input from controller
        Touch,                      ///< Touch input
        TouchScroll                 ///< Moving/scrolling touch input
    };
    
    class Overlay;
    namespace elm { class Element; }
    
    void shiftItemFocus(elm::Element* element); // forward declare

    namespace impl {
        
        /**
         * @brief Overlay launch parameters
         */
        enum class LaunchFlags : u8 {
            None = 0,                       ///< Do nothing special at launch
            CloseOnExit        = BIT(0)     ///< Close the overlay the last Gui gets poped from the stack
        };
        
        static constexpr LaunchFlags operator|(LaunchFlags lhs, LaunchFlags rhs) {
            return static_cast<LaunchFlags>(u8(lhs) | u8(rhs));
        }
        
        
        
    }
    
    void goBack(u32 count = 1);

    void pop(u32 count = 1);

    void setNextOverlay(const std::string& ovlPath, std::string args = "");
    
    template<typename TOverlay, impl::LaunchFlags launchFlags = impl::LaunchFlags::CloseOnExit>
    int loop(int argc, char** argv);
    
    // Helpers
    
    namespace hlp {

        /**
         * @brief Wrapper for service initialization
         *
         * @param f wrapped function
         */
        template<typename F>
        static inline void doWithSmSession(F f) {
            smInitialize();
            f();
            smExit();
        }
        
        /**
         * @brief Wrapper for sd card access using stdio
         * @note Consider using raw fs calls instead as they are faster and need less space
         *
         * @param f wrapped function
         */
        template<typename F>
        static inline void doWithSDCardHandle(F f) {
            fsdevMountSdmc();
            f();
            fsdevUnmountDevice("sdmc");
        }
        
        /**
         * @brief Guard that will execute a passed function at the end of the current scope
         *
         * @param f wrapped function
         */
        template<typename F>
        class ScopeGuard {
        public:
            ScopeGuard(const ScopeGuard&) = delete;
            ScopeGuard& operator=(const ScopeGuard&) = delete;
        
            ALWAYS_INLINE explicit ScopeGuard(F func) : f(std::move(func)) { }
            ALWAYS_INLINE ~ScopeGuard() { if (!canceled) { f(); } }
            void dismiss() { canceled = true; }
        
        private:
            F f;
            bool canceled = false;
        };
        
        /**
         * @brief libnx hid:sys shim that gives or takes away frocus to or from the process with the given aruid
         *
         * @param enable Give focus or take focus
         * @param aruid Aruid of the process to focus/unfocus
         * @return Result Result
         */
        static Result hidsysEnableAppletToGetInput(bool enable, u64 aruid) {
            const struct {
                u8 permitInput;
                u64 appletResourceUserId;
            } in = { enable != 0, aruid };
            
            return serviceDispatchIn(hidsysGetServiceSession(), 503, in);
        }
        
        static Result viAddToLayerStack(ViLayer *layer, ViLayerStack stack) {
            const struct {
                u32 stack;
                u64 layerId;
            } in = { stack, layer->layer_id };
            
            return serviceDispatchIn(viGetSession_IManagerDisplayService(), 6000, in);
        }

        /**
         * @brief Remove layer from layer stack
         */
        static Result viRemoveFromLayerStack(ViLayer *layer, ViLayerStack stack) {
            const struct {
                u32 stack;
                u64 layerId;
            } in = { stack, layer->layer_id };
            
            // Service command 6001 is commonly used for remove operations
            // If this doesn't work, try 6002, 6010, or other nearby values
            return serviceDispatchIn(viGetSession_IManagerDisplayService(), 6001, in);
        }
        
        /**
         * @brief Toggles focus between the Tesla overlay and the rest of the system
         *
         * @param enabled Focus Tesla?
         */
        void requestForeground(bool enabled, bool updateGlobalFlag = true);


        // Deprecated code, no longer used but preserved for consistency
        namespace ini {
            
            /**
             * @brief Ini file type
             */
            using IniData = std::map<std::string, std::map<std::string, std::string>>;
            
            /**
             * @brief Parses a ini string
             * 
             * @param str String to parse
             * @return Parsed data
             * // Modified to be "const std" instead of just "std"
             */
            static IniData parseIni(const std::string &str) {
                return ult::parseIni(str);
            }
            
            /**
             * @brief Unparses ini data into a string
             *
             * @param iniData Ini data
             * @return Ini string
             */
            std::string unparseIni(const IniData &iniData);

            
            /**
             * @brief Read Tesla settings file
             *
             * @return Settings data
             */
            static IniData readOverlaySettings(auto& CONFIG_FILE) {
                /* Open Sd card filesystem. */
                FsFileSystem fsSdmc;
                if (R_FAILED(fsOpenSdCardFileSystem(&fsSdmc)))
                    return {};
                hlp::ScopeGuard fsGuard([&] { fsFsClose(&fsSdmc); });
                
                /* Open config file. */
                FsFile fileConfig;
                if (R_FAILED(fsFsOpenFile(&fsSdmc, CONFIG_FILE, FsOpenMode_Read, &fileConfig)))
                    return {};
                hlp::ScopeGuard fileGuard([&] { fsFileClose(&fileConfig); });
                
                /* Get config file size. */
                s64 configFileSize;
                if (R_FAILED(fsFileGetSize(&fileConfig, &configFileSize)))
                    return {};
                
                /* Read and parse config file. */
                std::string configFileData(configFileSize, '\0');
                u64 readSize;
                Result rc = fsFileRead(&fileConfig, 0, configFileData.data(), configFileSize, FsReadOption_None, &readSize);
                if (R_FAILED(rc) || readSize != static_cast<u64>(configFileSize))
                    return {};
                
                return ult::parseIni(configFileData);
            }
            
            /**
             * @brief Replace Tesla settings file with new data
             *
             * @param iniData new data
             */
            static void writeOverlaySettings(IniData const &iniData, auto& CONFIG_FILE) {
                /* Open Sd card filesystem. */
                FsFileSystem fsSdmc;
                if (R_FAILED(fsOpenSdCardFileSystem(&fsSdmc)))
                    return;
                hlp::ScopeGuard fsGuard([&] { fsFsClose(&fsSdmc); });
                
                /* Open config file. */
                FsFile fileConfig;
                if (R_FAILED(fsFsOpenFile(&fsSdmc, CONFIG_FILE, FsOpenMode_Write, &fileConfig)))
                    return;
                hlp::ScopeGuard fileGuard([&] { fsFileClose(&fileConfig); });
                
                const std::string iniString = unparseIni(iniData);
                
                fsFileWrite(&fileConfig, 0, iniString.c_str(), iniString.length(), FsWriteOption_Flush);
            }
            
            /**
             * @brief Merge and save changes into Tesla settings file
             *
             * @param changes setting values to add or update
             */
            static void updateOverlaySettings(IniData const &changes, auto& CONFIG_FILE) {
                hlp::ini::IniData iniData = hlp::ini::readOverlaySettings(CONFIG_FILE);
                for (auto &section : changes) {
                    for (auto &keyValue : section.second) {
                        iniData[section.first][keyValue.first] = keyValue.second;
                    }
                }
                writeOverlaySettings(iniData, CONFIG_FILE);
            }
            
        }
        
        /**
         * @brief Decodes a key string into it's key code
         *
         * @param value Key string
         * @return Key code
         */
        static u64 stringToKeyCode(const std::string& value) {
            for (const auto& keyInfo : ult::KEYS_INFO) {
                if (strcasecmp(value.c_str(), keyInfo.name) == 0)
                    return keyInfo.key;
            }
            return 0;
        }
        
        
        /**
         * @brief Decodes a combo string into key codes
         *
         * @param value Combo string
         * @return Key codes
         */
        static u64 comboStringToKeys(const std::string &value) {
            u64 keyCombo = 0x00;
            for (const auto& key : ult::split(ult::removeWhiteSpaces(value), '+')) {
                keyCombo |= hlp::stringToKeyCode(key);
            }
            return keyCombo;
        }
        
        /**
         * @brief Encodes key codes into a combo string
         *
         * @param keys Key codes
         * @return Combo string
         */
        std::string keysToComboString(u64 keys);

        inline static std::mutex comboMutex;

        // Function to load key combo mappings from both overlays.ini and packages.ini
        void loadEntryKeyCombos();
        
        // Function to check if a key combination matches any overlay key combo
        static OverlayCombo getEntryForKeyCombo(u64 keys) {
            std::lock_guard<std::mutex> lock(comboMutex);
            if (auto it = ult::g_entryCombos.find(keys); it != ult::g_entryCombos.end())
                return it->second;
            return { "", "" };
        }

    }
    


    // Renderer
    
    namespace gfx {
        TESLA_OPT_SPEED_PUSH

        extern "C" u64 __nx_vi_layer_id;
        

        struct ScissoringConfig {
            u32 x, y, x_max, y_max;  // w and h never read back — only precomputed x_max/y_max are used
        };
        

        // Forward declarations
        class Renderer;
        
        inline static std::shared_mutex s_translationCacheMutex;
        
        class FontManager {
        public:
            struct Glyph {
                stbtt_fontinfo *currFont;
                float currFontSize;
                int bounds[4];
                int xAdvance;
                u8 *glyphBmp;
                int width, height;
        
                ~Glyph() {
                    if (glyphBmp) {
                        stbtt_FreeBitmap(glyphBmp, nullptr);
                        glyphBmp = nullptr;
                    }
                }
        
                Glyph(const Glyph&) = delete;
                Glyph& operator=(const Glyph&) = delete;
        
                Glyph(Glyph&& other) noexcept
                    : currFont(other.currFont), currFontSize(other.currFontSize)
                    , xAdvance(other.xAdvance), glyphBmp(other.glyphBmp)
                    , width(other.width), height(other.height) {
                    memcpy(bounds, other.bounds, sizeof(bounds));
                    other.glyphBmp = nullptr;
                }
        
                Glyph& operator=(Glyph&& other) noexcept {
                    if (this != &other) {
                        if (glyphBmp) stbtt_FreeBitmap(glyphBmp, nullptr);
                        currFont     = other.currFont;
                        currFontSize = other.currFontSize;
                        xAdvance     = other.xAdvance;
                        glyphBmp     = other.glyphBmp;
                        width        = other.width;
                        height       = other.height;
                        memcpy(bounds, other.bounds, sizeof(bounds));
                        other.glyphBmp = nullptr;
                    }
                    return *this;
                }
        
                Glyph() : currFont(nullptr), currFontSize(0.0f), xAdvance(0),
                          glyphBmp(nullptr), width(0), height(0) {
                    std::memset(bounds, 0, sizeof(bounds));
                }
            };
        
            struct FontMetrics {
                int ascent, descent, lineGap;
                int lineHeight;
                stbtt_fontinfo* font;
                float fontSize;
        
                FontMetrics() : ascent(0), descent(0), lineGap(0), lineHeight(0), font(nullptr), fontSize(0.0f) {}
        
                FontMetrics(stbtt_fontinfo* f, float size) : font(f), fontSize(size) {
                    if (font) {
                        stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
                        const float scale = stbtt_ScaleForPixelHeight(font, fontSize);
                        ascent     = static_cast<int>(ascent  * scale);
                        descent    = static_cast<int>(descent * scale);
                        lineGap    = static_cast<int>(lineGap * scale);
                        lineHeight = ascent - descent + lineGap;
                    } else {
                        ascent = descent = lineGap = lineHeight = 0;
                    }
                }
            };
        
            enum class CacheType { Regular, Notification };
        
        private:
            inline static std::shared_mutex s_cacheMutex;
            inline static std::mutex        s_initMutex;
        
            inline static std::unordered_map<u64, std::shared_ptr<Glyph>> s_sharedGlyphCache;
            inline static std::unordered_map<u64, std::shared_ptr<Glyph>> s_notificationGlyphCache;
            inline static std::unordered_map<u64, FontMetrics>            s_fontMetricsCache;
        
            static constexpr size_t MAX_CACHE_SIZE              = 600;
            static constexpr size_t CLEANUP_THRESHOLD           = 500;
            static constexpr size_t MAX_NOTIFICATION_CACHE_SIZE = 200;
        
            inline static stbtt_fontinfo* s_stdFont     = nullptr;
            inline static stbtt_fontinfo* s_localFont   = nullptr;
            inline static stbtt_fontinfo* s_localFontCN = nullptr;
            inline static stbtt_fontinfo* s_localFontTW = nullptr;
            inline static stbtt_fontinfo* s_localFontKO = nullptr;
            inline static stbtt_fontinfo* s_extFont     = nullptr;
            inline static bool s_hasLocalFont = false;
            inline static bool s_initialized  = false;
        
            static u64 generateCacheKey(u32 character, bool monospace, u32 fontSize) {
                u64 key = (static_cast<u64>(character) << 32) | static_cast<u64>(fontSize);
                if (monospace) key |= (1ULL << 63);
                return key;
            }
        
            static u64 generateFontMetricsCacheKey(stbtt_fontinfo* font, u32 fontSize) {
                return (reinterpret_cast<uintptr_t>(font) << 32) | static_cast<u64>(fontSize);
            }
        
            // Consolidated trim — replaces cleanupOldEntries + cleanupNotificationCache
            static void trimCache(std::unordered_map<u64, std::shared_ptr<Glyph>>& cache, size_t target) {
                if (cache.size() <= target) return;
                size_t toRemove = cache.size() - target;
                for (auto it = cache.begin(); toRemove-- && it != cache.end();)
                    it = cache.erase(it);
            }
        
            // Single helper to clear + release bucket memory — used everywhere
            template<typename V>
            static void clearMap(std::unordered_map<u64, V>& m) {
                m.clear();
                m.rehash(0);
            }
        
            // Assumes lock already held by caller
            static void clearAllUnsafe() {
                clearMap(s_sharedGlyphCache);
                clearMap(s_notificationGlyphCache);
                clearMap(s_fontMetricsCache);
            }
        
            static std::shared_ptr<Glyph> getOrCreateGlyphInternal(u32 character, bool monospace, u32 fontSize, CacheType cacheType) {
                const u64 key = generateCacheKey(character, monospace, fontSize);
                auto& targetCache = (cacheType == CacheType::Notification)
                                  ? s_notificationGlyphCache : s_sharedGlyphCache;
        
                {
                    std::shared_lock<std::shared_mutex> readLock(s_cacheMutex);
                    if (!s_initialized) return nullptr;
                    auto it = targetCache.find(key);
                    if (it != targetCache.end()) return it->second;
                }
        
                std::unique_lock<std::shared_mutex> writeLock(s_cacheMutex);
                if (!s_initialized) return nullptr;
        
                // Double-checked
                auto it = targetCache.find(key);
                if (it != targetCache.end()) return it->second;
        
                if (cacheType == CacheType::Regular)
                    trimCache(s_sharedGlyphCache, CLEANUP_THRESHOLD);
                else
                    trimCache(s_notificationGlyphCache, MAX_NOTIFICATION_CACHE_SIZE / 2);
        
                auto glyph = std::make_shared<Glyph>();
                glyph->currFont = selectFontForCharacterUnsafe(character);
                if (!glyph->currFont) return nullptr;
        
                glyph->currFontSize = stbtt_ScaleForPixelHeight(glyph->currFont, fontSize);
        
                stbtt_GetCodepointBitmapBoxSubpixel(glyph->currFont, character,
                    glyph->currFontSize, glyph->currFontSize, 0, 0,
                    &glyph->bounds[0], &glyph->bounds[1], &glyph->bounds[2], &glyph->bounds[3]);
        
                s32 yAdvance = 0;
                stbtt_GetCodepointHMetrics(glyph->currFont, monospace ? 'W' : character,
                                           &glyph->xAdvance, &yAdvance);
        
                glyph->glyphBmp = stbtt_GetCodepointBitmap(glyph->currFont,
                    glyph->currFontSize, glyph->currFontSize, character,
                    &glyph->width, &glyph->height, nullptr, nullptr);
        
                targetCache[key] = glyph;
                return glyph;
            }
        
            static stbtt_fontinfo* selectFontForCharacterUnsafe(u32 character) {
                if (!s_initialized) return nullptr;
                if (stbtt_FindGlyphIndex(s_extFont, character))                          return s_extFont;
                if (character == 0x00B0)                                                 return s_stdFont;
                if (s_hasLocalFont && stbtt_FindGlyphIndex(s_localFont,   character))   return s_localFont;
                if (stbtt_FindGlyphIndex(s_stdFont,     character))                      return s_stdFont;
                if (stbtt_FindGlyphIndex(s_localFontCN, character))                      return s_localFontCN;
                if (stbtt_FindGlyphIndex(s_localFontTW, character))                      return s_localFontTW;
                if (stbtt_FindGlyphIndex(s_localFontKO, character))                      return s_localFontKO;
                return s_stdFont;
            }
        
        public:
            static void initializeFonts(stbtt_fontinfo* stdFont,
                                        stbtt_fontinfo* localFont,
                                        stbtt_fontinfo* localFontCN,
                                        stbtt_fontinfo* localFontTW,
                                        stbtt_fontinfo* localFontKO,
                                        stbtt_fontinfo* extFont,
                                        bool hasLocalFont) {
                std::lock_guard<std::mutex>          initLock(s_initMutex);
                std::unique_lock<std::shared_mutex>  cacheLock(s_cacheMutex);
                s_stdFont     = stdFont;
                s_localFont   = localFont;
                s_localFontCN = localFontCN;
                s_localFontTW = localFontTW;
                s_localFontKO = localFontKO;
                s_extFont     = extFont;
                s_hasLocalFont = hasLocalFont;
                s_initialized  = true;
            }
        
            static stbtt_fontinfo* selectFontForCharacter(u32 character) {
                std::shared_lock<std::shared_mutex> lock(s_cacheMutex);
                return selectFontForCharacterUnsafe(character);
            }
        
            static FontMetrics getFontMetrics(stbtt_fontinfo* font, u32 fontSize) {
                if (!font) return FontMetrics();
                const u64 key = generateFontMetricsCacheKey(font, fontSize);
                {
                    std::shared_lock<std::shared_mutex> readLock(s_cacheMutex);
                    auto it = s_fontMetricsCache.find(key);
                    if (it != s_fontMetricsCache.end()) return it->second;
                }
                std::unique_lock<std::shared_mutex> writeLock(s_cacheMutex);
                auto it = s_fontMetricsCache.find(key);
                if (it != s_fontMetricsCache.end()) return it->second;
                FontMetrics metrics(font, static_cast<float>(fontSize));
                s_fontMetricsCache[key] = metrics;
                return metrics;
            }
        
            static FontMetrics getFontMetricsForCharacter(u32 character, u32 fontSize) {
                return getFontMetrics(selectFontForCharacter(character), fontSize);
            }
        
            [[nodiscard]] static std::shared_ptr<Glyph> getOrCreateGlyph(u32 character, bool monospace, u32 fontSize) {
                return getOrCreateGlyphInternal(character, monospace, fontSize, CacheType::Regular);
            }
        
            [[nodiscard]] static std::shared_ptr<Glyph> getOrCreateNotificationGlyph(u32 character, bool monospace, u32 fontSize) {
                return getOrCreateGlyphInternal(character, monospace, fontSize, CacheType::Notification);
            }
        
            static void clearNotificationCache() {
                std::unique_lock<std::shared_mutex> lock(s_cacheMutex);
                clearMap(s_notificationGlyphCache);
            }
        
            static void clearCache() {
                std::unique_lock<std::shared_mutex> lock(s_cacheMutex);
                clearMap(s_sharedGlyphCache);
                clearMap(s_fontMetricsCache);
            }
        
            static void clearAllCaches() {
                std::unique_lock<std::shared_mutex> lock(s_cacheMutex);
                clearAllUnsafe();
            }
        
            static void cleanup() {
                std::lock_guard<std::mutex>         initLock(s_initMutex);
                std::unique_lock<std::shared_mutex> cacheLock(s_cacheMutex);
                clearAllUnsafe();
                s_initialized  = false;
                s_stdFont      = nullptr;
                s_localFont    = nullptr;
                s_localFontCN  = nullptr;
                s_localFontTW  = nullptr;
                s_localFontKO  = nullptr;
                s_extFont      = nullptr;
                s_hasLocalFont = false;
            }
        
            static bool isInitialized() {
                std::shared_lock<std::shared_mutex> lock(s_cacheMutex);
                return s_initialized;
            }

        #ifndef NDEBUG
            static size_t getCacheSize() {
                std::shared_lock<std::shared_mutex> lock(s_cacheMutex);
                return s_sharedGlyphCache.size();
            }
        
            static size_t getNotificationCacheSize() {
                std::shared_lock<std::shared_mutex> lock(s_cacheMutex);
                return s_notificationGlyphCache.size();
            }
        
            static size_t getFontMetricsCacheSize() {
                std::shared_lock<std::shared_mutex> lock(s_cacheMutex);
                return s_fontMetricsCache.size();
            }
        
            static size_t getMemoryUsage() {
                std::shared_lock<std::shared_mutex> lock(s_cacheMutex);
                size_t total = 0;
                auto countCache = [&](const std::unordered_map<u64, std::shared_ptr<Glyph>>& cache) {
                    for (const auto& [k, g] : cache)
                        if (g && g->glyphBmp) total += g->width * g->height;
                };
                countCache(s_sharedGlyphCache);
                countCache(s_notificationGlyphCache);
                return total;
            }
        #endif
        };
        
        // Updated thread-safe calculateStringWidth function
        float calculateStringWidth(const std::string& originalString, const float fontSize, const bool monospace = false);

        static std::pair<int, int> getUnderscanPixels();

        /**
         * @brief Manages the Tesla layer and draws raw data to the screen
         */
        class Renderer final {
        public:

            using Glyph = FontManager::Glyph;

            Renderer& operator=(Renderer&) = delete;
            
            friend class tsl::Overlay;
            
            /**
             * @brief Gets the renderer instance
             *
             * @return Renderer
             */
            inline static Renderer& get() {
                static Renderer renderer;
                
                return renderer;
            }
            
            stbtt_fontinfo m_stdFont, m_extFont;
            stbtt_fontinfo m_localFont;           // Primary font based on system language
            stbtt_fontinfo m_localFontCN;         // Chinese Simplified - always loaded
            stbtt_fontinfo m_localFontTW;         // Chinese Traditional - always loaded
            stbtt_fontinfo m_localFontKO;         // Korean - always loaded
            bool m_hasLocalFont = false;          // Whether primary local font is valid

            static inline float s_opacity = 1.0F;

            /**
             * @brief Handles opacity of drawn colors for fadeout. Pass all colors through this function in order to apply opacity properly
             *
             * @param c Original color
             * @return Color with applied opacity
             */
            static inline Color a(const Color& c) {
                const u8 opacity_limit = static_cast<u8>(0xF * Renderer::s_opacity);
                return (c.rgba & 0x0FFF) | (static_cast<u16>(
                    ult::disableTransparency
                        ? (ult::useOpaqueScreenshots
                               ? 0xF                       // fully opaque when both flags on
                               : (c.a > 0xE ? c.a : 0xE)) // clamp to 14, keep lower values
                        : (c.a < opacity_limit ? c.a : opacity_limit) // normal fade logic
                ) << 12);
            }

            static inline Color aWithOpacity(const Color& c) {
                const u8 opacity_limit = static_cast<u8>(0xF * Renderer::s_opacity);
                return (c.rgba & 0x0FFF) | (static_cast<u16>(
                    ult::disableTransparency
                        ? 0xF                       // fully opaque when both flags on
                        : (c.a < opacity_limit ? c.a : opacity_limit) // normal fade logic
                ) << 12);
            }

            static inline Color a2(const Color& c) {
                if (!ult::disableTransparency)
                    return c;
                const u8 a = ult::useOpaqueScreenshots ? 0xF : (c.a > 0xE ? c.a : 0xE);
                return (c.rgba & 0x0FFF) | (static_cast<u16>(a) << 12);
            }
            
            /**
             * @brief Enables scissoring, discarding of any draw outside the given boundaries
             *
             * @param x x pos
             * @param y y pos
             * @param w Width
             * @param h Height
             */
            inline void enableScissoring(const u32 x, const u32 y, const u32 w, const u32 h) {
                this->m_scissorStack[this->m_scissorDepth++] = {x, y, x + w, y + h};
            }
            
            inline void disableScissoring() {
                --this->m_scissorDepth;
            }
            
            
            // Drawing functions
            
            /**
             * @brief Draw a single pixel onto the screen
             *
             * @param x X pos
             * @param y Y pos
             * @param color Color
             */
            inline void setPixel(const u32 x, const u32 y, const Color& color) {
                const u32 offset = this->getPixelOffset(x, y);
                if (offset != UINT32_MAX) [[likely]] {
                    Color* framebuffer = static_cast<Color*>(this->m_currentFramebuffer);
                    framebuffer[offset] = color;
                }
            }

            inline void setPixelAtOffset(const u32 offset, const Color& color) {
                Color* framebuffer = static_cast<Color*>(this->m_currentFramebuffer);
                framebuffer[offset] = color;
            }


            
            /**
             * @brief Blends two colors
             *
             * @param src Source color
             * @param dst Destination color
             * @param alpha Opacity
             * @return Blended color
             */
            // inv_alpha_table removed — (15 - alpha) is a single SUB instruction,
            // faster than a load and lets the compiler dead-strip the 16-byte table.
            inline u8 __attribute__((always_inline)) blendColor(const u8 src, const u8 dst, const u8 alpha) {
                return ((src * (15u - alpha)) + (dst * alpha)) >> 4;
            }
            
            /**
             * @brief Draws a single source blended pixel onto the screen
             *
             * @param x X pos
             * @param y Y pos
             * @param color Color
             */
            inline void setPixelBlendSrc(const u32 x, const u32 y, const Color& color) {
                const u32 offset = this->getPixelOffset(x, y);
                if (offset == UINT32_MAX) [[unlikely]]
                    return;
                
                Color* framebuffer = static_cast<Color*>(this->m_currentFramebuffer);
                const Color src = framebuffer[offset];
                
                // Direct write instead of calling setPixel
                framebuffer[offset] = Color(
                    blendColor(src.r, color.r, color.a),
                    blendColor(src.g, color.g, color.a),
                    blendColor(src.b, color.b, color.a),
                    src.a
                );
            }
            

            // Compromise version - keep framebuffer lookup but inline the rest
            inline void setPixelBlendDst(const u32 x, const u32 y, const Color& color) {
                const u32 offset = this->getPixelOffset(x, y);
                if (offset == UINT32_MAX) [[unlikely]]
                    return;
                
                Color* framebuffer = static_cast<Color*>(this->m_currentFramebuffer);
                const Color src = framebuffer[offset];
                
                // Direct write instead of calling setPixel
                framebuffer[offset] = Color(
                    blendColor(src.r, color.r, color.a),
                    blendColor(src.g, color.g, color.a),
                    blendColor(src.b, color.b, color.a),
                    (color.a + (src.a * (0xF - color.a) >> 4))
                );
            }

            inline void drawRect(const s32 x, const s32 y, const s32 w, const s32 h, const Color& color) {
                if (w <= 0 || h <= 0) [[unlikely]] return;
                
                // Calculate clipped bounds
                const s32 x_start = x < 0 ? 0 : x;
                const s32 y_start = y < 0 ? 0 : y;
                const s32 x_end = (x + w > cfg::FramebufferWidth) ? cfg::FramebufferWidth : x + w;
                const s32 y_end = (y + h > cfg::FramebufferHeight) ? cfg::FramebufferHeight : y + h;
                
                // Early exit if completely outside bounds
                if (x_start >= x_end || y_start >= y_end) [[unlikely]] return;
                

                this->processRectChunk(x_start, x_end, y_start, y_end, color);
            }

            /**
             * @brief Worker function for multithreaded rectangle drawing
             * @param x_start Start X coordinate
             * @param x_end End X coordinate  
             * @param y_start Start Y coordinate for this thread
             * @param y_end End Y coordinate for this thread
             * @param color Color to draw
             */
            inline void processRectChunk(const s32 x_start, const s32 x_end, const s32 y_start, const s32 y_end, const Color& color) {
                // Clamp to active scissor rectangle (mirrors getPixelOffset scissor check)
                s32 xs = x_start, xe = x_end, ys = y_start, ye = y_end;
                if (this->m_scissorDepth != 0) [[unlikely]] {
                    const auto& sc = this->m_scissorStack[this->m_scissorDepth - 1];
                    xs = std::max(xs, static_cast<s32>(sc.x));
                    xe = std::min(xe, static_cast<s32>(sc.x_max));
                    ys = std::max(ys, static_cast<s32>(sc.y));
                    ye = std::min(ye, static_cast<s32>(sc.y_max));
                    if (xs >= xe || ys >= ye) return;
                }
                u16* fb16 = reinterpret_cast<u16*>(this->m_currentFramebuffer);
                for (s32 yi = ys; yi < ye; ++yi) {
                    const u32 rowBase = blockLinearYPart(static_cast<u32>(yi), offsetWidthVar);
                    fillRowSpanNEON(fb16, rowBase, xs, xe, color);
                }
            }
        

            /**
             * @brief Draws a rectangle of given sizes (Multi-threaded)
             *
             * @param x X pos
             * @param y Y pos
             * @param w Width
             * @param h Height
             * @param color Color
             */
            inline void drawRectMultiThreaded(const s32 x, const s32 y, const s32 w, const s32 h, const Color& color) {
                // Early exit for invalid dimensions
                if (w <= 0 || h <= 0) return;
                
                // Calculate clipped bounds
                const s32 x_start = x < 0 ? 0 : x;
                const s32 y_start = y < 0 ? 0 : y;
                const s32 x_end = (x + w > cfg::FramebufferWidth) ? cfg::FramebufferWidth : x + w;
                const s32 y_end = (y + h > cfg::FramebufferHeight) ? cfg::FramebufferHeight : y + h;
                
                // Early exit if completely outside bounds
                if (x_start >= x_end || y_start >= y_end) return;
                
                // Calculate visible dimensions
                const s32 visibleHeight = y_end - y_start;
                
                // Ceiling division: floor(h/n) silently drops up to (n-1) rows at the
                // bottom of any rect whose height is not a multiple of numThreads.
                const s32 n = static_cast<s32>(ult::numThreads);
                const s32 chunkSize = std::max(1, (visibleHeight + n - 1) / n);
                unsigned launched = 0u;
                for (unsigned i = 0; i < static_cast<unsigned>(n); ++i) {
                    const s32 startRow = y_start + static_cast<s32>(i) * chunkSize;
                    if (startRow >= y_end) break;
                    const s32 endRow = std::min(startRow + chunkSize, y_end);
                    ult::renderThreads[launched++] = std::thread(&Renderer::processRectChunk, this,
                                                                 x_start, x_end, startRow, endRow, color);
                }
                for (unsigned i = 0; i < launched; ++i)
                    ult::renderThreads[i].join();
            }

            inline void drawRectAdaptive(s32 x, s32 y, s32 w, s32 h, const Color& color) {
                if (ult::expandedMemory)
                    drawRectMultiThreaded(x, y, w, h, color);
                else
                    drawRect(x, y, w, h, color);
            }


            /**
             * @brief Draws a rectangle of given sizes with empty filling
             * 
             * @param x X pos 
             * @param y Y pos
             * @param w Width
             * @param h Height
             * @param color Color
             */
            inline void drawEmptyRect(s32 x, s32 y, s32 w, s32 h, Color color) {
                const s32 x_end = x + w - 1;
                const s32 y_end = y + h - 1;

                if (x_end < 0 || y_end < 0 ||
                    x >= cfg::FramebufferWidth ||
                    y >= cfg::FramebufferHeight) [[unlikely]] return;

                const s32 line_x_start = x < 0 ? 0 : x;
                const s32 line_x_end   = x_end >= cfg::FramebufferWidth
                                         ? cfg::FramebufferWidth - 1 : x_end;

                u16* const fb16 = reinterpret_cast<u16*>(this->m_currentFramebuffer);
                const u32  owv  = offsetWidthVar;
                const u8   alpha = color.a;

                // ── Horizontal lines — NEON row fill (xe is exclusive) ────────────
                if (y >= 0 && y < cfg::FramebufferHeight)
                    fillRowSpanNEON(fb16,
                        blockLinearYPart(static_cast<u32>(y), owv),
                        line_x_start, line_x_end + 1, color);

                if (h > 1 && y_end >= 0 && y_end < cfg::FramebufferHeight)
                    fillRowSpanNEON(fb16,
                        blockLinearYPart(static_cast<u32>(y_end), owv),
                        line_x_start, line_x_end + 1, color);

                // ── Vertical lines — precompute x-column offset once per side ─────
                // blockLinearOffset(px, yPart) = yPart + xPart(px).
                // xPart depends only on px, so hoist it out of the row loop.
                if (h > 2) {
                    const s32 line_y_start = (y + 1) < 0 ? 0 : (y + 1);
                    const s32 line_y_end   = (y_end - 1) >= cfg::FramebufferHeight
                                             ? cfg::FramebufferHeight - 1 : (y_end - 1);

                    if (line_y_start <= line_y_end) {
                        if (x >= 0 && x < cfg::FramebufferWidth) {
                            const u32 px = static_cast<u32>(x);
                            const u32 xPartL = ((px >> 5u) << 12u) | ((px & 16u) << 3u)
                                             | ((px & 8u)  <<  1u) |  (px & 7u);
                            for (s32 yi = line_y_start; yi <= line_y_end; ++yi)
                                blendPixelDirect(fb16,
                                    blockLinearYPart(static_cast<u32>(yi), owv) + xPartL,
                                    color, alpha);
                        }
                        if (w > 1 && x_end >= 0 && x_end < cfg::FramebufferWidth) {
                            const u32 px = static_cast<u32>(x_end);
                            const u32 xPartR = ((px >> 5u) << 12u) | ((px & 16u) << 3u)
                                             | ((px & 8u)  <<  1u) |  (px & 7u);
                            for (s32 yi = line_y_start; yi <= line_y_end; ++yi)
                                blendPixelDirect(fb16,
                                    blockLinearYPart(static_cast<u32>(yi), owv) + xPartR,
                                    color, alpha);
                        }
                    }
                }
            }

            /**
             * @brief Draws a line
             * 
             * @param x0 Start X pos 
             * @param y0 Start Y pos
             * @param x1 End X pos
             * @param y1 End Y pos
             * @param color Color
             */
            inline void drawLine(s32 x0, s32 y0, s32 x1, s32 y1, Color color) {
                // Early exit for single point
                if (x0 == x1 && y0 == y1) {
                    if (x0 >= 0 && y0 >= 0 && x0 < cfg::FramebufferWidth && y0 < cfg::FramebufferHeight) {
                        this->setPixelBlendDst(x0, y0, color);
                    }
                    return;
                }
                
                // Calculate deltas
                const s32 dx = x1 - x0;
                const s32 dy = y1 - y0;
                
                // Calculate absolute deltas and steps
                const s32 abs_dx = dx < 0 ? -dx : dx;
                const s32 abs_dy = dy < 0 ? -dy : dy;
                const s32 step_x = dx < 0 ? -1 : 1;
                const s32 step_y = dy < 0 ? -1 : 1;
                
                // Bresenham's algorithm
                s32 x = x0, y = y0;
                s32 error = abs_dx - abs_dy;
                s32 error2;

                while (true) {
                    // Bounds check and draw pixel
                    if (x >= 0 && y >= 0 && x < cfg::FramebufferWidth && y < cfg::FramebufferHeight) {
                        this->setPixelBlendDst(x, y, color);
                    }
                    
                    // Check if we've reached the end point
                    if (x == x1 && y == y1) break;
                    
                    // Calculate error and step
                    error2 = error << 1;  // error * 2
                    
                    if (error2 > -abs_dy) {
                        error -= abs_dy;
                        x += step_x;
                    }
                    if (error2 < abs_dx) {
                        error += abs_dx;
                        y += step_y;
                    }
                }
            }

            /**
             * @brief Draws a dashed line
             * 
             * @param x0 Start X pos 
             * @param y0 Start Y pos
             * @param x1 End X pos
             * @param y1 End Y pos
             * @param line_width How long one line can be
             * @param color Color
             */
            inline void drawDashedLine(s32 x0, s32 y0, s32 x1, s32 y1, s32 line_width, Color color) {
                // Source of formula: https://www.cc.gatech.edu/grads/m/Aaron.E.McClennen/Bresenham/code.html

                const s32 x_min = std::min(x0, x1);
                const s32 x_max = std::max(x0, x1);
                const s32 y_min = std::min(y0, y1);
                const s32 y_max = std::max(y0, y1);

                if (x_min < 0 || y_min < 0 || x_min >= cfg::FramebufferWidth || y_min >= cfg::FramebufferHeight)
                    return;

                const s32 dx = x_max - x_min;
                const s32 dy = y_max - y_min;
                s32 d = 2 * dy - dx;

                const s32 incrE = 2*dy;
                const s32 incrNE = 2*(dy - dx);

                this->setPixelBlendDst(x_min, y_min, color);

                s32 x = x_min;
                s32 y = y_min;
                s32 rendered = 0;

                while(x < x1) {
                    if (d <= 0) {
                        d += incrE;
                        x++;
                    }
                    else {
                        d += incrNE;
                        x++;
                        y++;
                    }
                    rendered++;
                    if (x < 0 || y < 0 || x >= cfg::FramebufferWidth || y >= cfg::FramebufferHeight)
                        continue;
                    if (x <= x_max && y <= y_max) {
                        if (rendered > 0 && rendered < line_width) {
                            this->setPixelBlendDst(x, y, color);
                        }
                        else if (rendered > 0 && rendered >= line_width) {
                            rendered *= -1;
                        }
                    }
                } 
                    
            }
                        
            inline void drawCircle(const s32 centerX, const s32 centerY, const u16 radius, const bool filled, const Color& color) {
                // Use Bresenham-style algorithm for small radii
                if (radius <= 3) {
                    s32 x = radius;
                    s32 y = 0;
                    s32 radiusError = 0;
                    s32 xChange = 1 - (radius << 1);
                    s32 yChange = 0;
                    
                    while (x >= y) {
                        if (filled) {
                            for (s32 i = centerX - x; i <= centerX + x; i++) {
                                this->setPixelBlendDst(i, centerY + y, color);
                                this->setPixelBlendDst(i, centerY - y, color);
                            }
                            for (s32 i = centerX - y; i <= centerX + y; i++) {
                                this->setPixelBlendDst(i, centerY + x, color);
                                this->setPixelBlendDst(i, centerY - x, color);
                            }
                            y++;
                            radiusError += yChange;
                            yChange += 2;
                            if (((radiusError << 1) + xChange) > 0) {
                                x--;
                                radiusError += xChange;
                                xChange += 2;
                            }
                        } else {
                            this->setPixelBlendDst(centerX + x, centerY + y, color);
                            this->setPixelBlendDst(centerX + y, centerY + x, color);
                            this->setPixelBlendDst(centerX - y, centerY + x, color);
                            this->setPixelBlendDst(centerX - x, centerY + y, color);
                            this->setPixelBlendDst(centerX - x, centerY - y, color);
                            this->setPixelBlendDst(centerX - y, centerY - x, color);
                            this->setPixelBlendDst(centerX + y, centerY - x, color);
                            this->setPixelBlendDst(centerX + x, centerY - y, color);
                            if (radiusError <= 0) {
                                y++;
                                radiusError += 2 * y + 1;
                            } else {
                                x--;
                                radiusError -= 2 * x + 1;
                            }
                        }
                    }
                    return;
                }
                
                // Original supersampling algorithm for larger radii
                const float r_f = static_cast<float>(radius);
                const float r2 = r_f * r_f;
                const u8 base_a = color.a;
                
                const s32 bound = radius + 2;
                s32 clip_left   = std::max(0, centerX - bound);
                s32 clip_right  = std::min(static_cast<s32>(cfg::FramebufferWidth),  centerX + bound);
                s32 clip_top    = std::max(0, centerY - bound);
                s32 clip_bottom = std::min(static_cast<s32>(cfg::FramebufferHeight), centerY + bound);
                if (this->m_scissorDepth != 0) [[unlikely]] {
                    const auto& sc = this->m_scissorStack[this->m_scissorDepth - 1];
                    clip_left   = std::max(clip_left,   static_cast<s32>(sc.x));
                    clip_right  = std::min(clip_right,  static_cast<s32>(sc.x_max));
                    clip_top    = std::max(clip_top,    static_cast<s32>(sc.y));
                    clip_bottom = std::min(clip_bottom, static_cast<s32>(sc.y_max));
                    if (clip_left >= clip_right || clip_top >= clip_bottom) return;
                }
                
                static constexpr float kSamples[8][2] = {
                    {-0.353553f, -0.353553f}, { 0.353553f, -0.353553f},
                    {-0.353553f,  0.353553f}, { 0.353553f,  0.353553f},
                    {-0.5f,  0.0f}, { 0.5f,  0.0f},
                    { 0.0f, -0.5f}, { 0.0f,  0.5f}
                };

                u16* fb16 = reinterpret_cast<u16*>(this->m_currentFramebuffer);

                // Loop-invariant thresholds — hoisted explicitly so -Os doesn't rematerialize.
                const float inner_thresh = r2 - r_f;  // d² ≤ this → definitely inside
                const float outer_thresh = r2 + r_f;  // d² > this → definitely outside

                if (filled) {
                    // Per-pixel x-scan: accumulate consecutive "definitely inside" pixels
                    // into a span and flush via NEON; border pixels get 8-sample AA.
                    // No sqrtf — the span boundary emerges naturally from the d²<r²-r test.
                    for (s32 yc = clip_top; yc < clip_bottom; ++yc) {
                        const float py    = static_cast<float>(yc - centerY) + 0.5f;
                        const float py_sq = py * py;
                        if (py_sq > outer_thresh) continue;
                        const u32 rowBase = blockLinearYPart(static_cast<u32>(yc), offsetWidthVar);

                        s32 span_xl = -1;
                        for (s32 xc = clip_left; xc < clip_right; ++xc) {
                            const float px = static_cast<float>(xc - centerX) + 0.5f;
                            const float d2 = px*px + py_sq;
                            if (d2 <= inner_thresh) {
                                if (span_xl < 0) span_xl = xc;  // start span
                            } else {
                                if (span_xl >= 0) {
                                    fillRowSpanNEON(fb16, rowBase, span_xl, xc, color);
                                    span_xl = -1;
                                }
                                if (d2 <= outer_thresh) {
                                    u32 cnt = 0;
                                    for (u32 s = 0; s < 8; ++s) {
                                        const float sx = px + kSamples[s][0], sy = py + kSamples[s][1];
                                        if (sx*sx + sy*sy <= r2) ++cnt;
                                    }
                                    if (cnt) blendPixelDirect(fb16, blockLinearOffset((u32)xc, rowBase),
                                                              color, (u8)((base_a * cnt + 4) / 8));
                                }
                            }
                        }
                        if (span_xl >= 0)
                            fillRowSpanNEON(fb16, rowBase, span_xl, clip_right, color);
                    }
                } else {
                    // Unfilled (outline) — per-pixel, band between (r-1)² and r²
                    const float inner_r2  = (r_f - 1.0f) * (r_f - 1.0f);
                    const float ring_hi   = inner_r2 + r_f;  // solid ring inner boundary
                    const float ring_lo   = inner_r2 - r_f;  // AA ring inner boundary
                    for (s32 yc = clip_top; yc < clip_bottom; ++yc) {
                        const float py    = static_cast<float>(yc - centerY) + 0.5f;
                        const float py_sq = py * py;
                        if (py_sq > outer_thresh) continue;  // row entirely outside circle
                        const u32 rowBase = blockLinearYPart(static_cast<u32>(yc), offsetWidthVar);
                        for (s32 xc = clip_left; xc < clip_right; ++xc) {
                            const float px = static_cast<float>(xc - centerX) + 0.5f;
                            const float d2 = px*px + py_sq;
                            if (d2 >= ring_hi && d2 <= inner_thresh) {
                                blendPixelDirect(fb16, blockLinearOffset((u32)xc, rowBase), color, base_a);
                            } else if (d2 >= ring_lo && d2 <= outer_thresh) {
                                u32 cnt = 0;
                                for (u32 s = 0; s < 8; ++s) {
                                    const float sx = px + kSamples[s][0], sy = py + kSamples[s][1];
                                    const float sd2 = sx*sx + sy*sy;
                                    if (sd2 >= inner_r2 && sd2 <= r2) ++cnt;
                                }
                                if (cnt) blendPixelDirect(fb16, blockLinearOffset((u32)xc, rowBase),
                                                          color, (u8)((base_a * cnt + 4) / 8));
                            }
                        }
                    }
                }
            }
            
            inline void drawBorderedRoundedRect(const s32 x, const s32 y, const s32 width, const s32 height, const s32 thickness, const s32 radius, const Color& highlightColor) {
                const s32 startX = x + 4;
                const s32 startY = y;
                const s32 adjustedWidth = width - 12;
                const s32 adjustedHeight = height + 1;
                
                // Pre-calculate corner positions
                const s32 leftCornerX = startX;
                const s32 rightCornerX = x + width - 9;
                const s32 topCornerY = startY;
                const s32 bottomCornerY = startY + height;
                
                // Draw borders
                this->drawRect(startX, startY - thickness, adjustedWidth, thickness, highlightColor);
                this->drawRect(startX, startY + adjustedHeight, adjustedWidth, thickness, highlightColor);
                this->drawRect(startX - thickness, startY, thickness, adjustedHeight, highlightColor);
                this->drawRect(startX + adjustedWidth, startY, thickness, adjustedHeight, highlightColor);
                
                // Pre-calculate AA colors once
                const Color aaColor1 = {highlightColor.r, highlightColor.g, highlightColor.b, static_cast<u8>(highlightColor.a >> 1)};  // 50%
                const Color aaColor2 = {highlightColor.r, highlightColor.g, highlightColor.b, static_cast<u8>(highlightColor.a >> 2)};  // 25%
                
                // ── Arc spans: hoist blockLinearYPart out of pixel loops ──────────────
                // The original code called setPixelBlendDst per pixel, which recomputed
                // the blockLinear y-contribution and checked scissor on every iteration.
                // hspan computes rowBase once per y-value and uses blendPixelDirect
                // (tiny: ~4 instructions for opaque colours).  No fillRowSpanNEON here —
                // arc spans are ≤radius pixels wide, so the NEON loop would never fire
                // and the function setup would inflate code size without benefit.
                // Without always_inline, -Os outlines hspan to one copy, called 8× per step.
                u16* const fb16 = reinterpret_cast<u16*>(this->m_currentFramebuffer);
                const u32  owv  = offsetWidthVar;
                const u8   ha   = highlightColor.a;

                // Scissor clip bounds — default to full framebuffer when no scissor.
                const s32 fbW = static_cast<s32>(cfg::FramebufferWidth);
                const s32 fbH = static_cast<s32>(cfg::FramebufferHeight);
                s32 sc_x = 0, sc_xe = fbW, sc_y = 0, sc_ye = fbH;
                if (this->m_scissorDepth != 0) [[unlikely]] {
                    const auto& sc = this->m_scissorStack[this->m_scissorDepth - 1];
                    sc_x  = static_cast<s32>(sc.x);
                    sc_xe = static_cast<s32>(sc.x_max);
                    sc_y  = static_cast<s32>(sc.y);
                    sc_ye = static_cast<s32>(sc.y_max);
                }

                // Draw span [xs, xe) at row yr. xe is exclusive.
                auto hspan = [&](s32 yr, s32 xs, s32 xe) {
                    if (yr < sc_y || yr >= sc_ye) return;
                    const u32 rb = blockLinearYPart(static_cast<u32>(yr), owv);
                    const s32 x0c = std::max(sc_x, xs), x1c = std::min(sc_xe, xe);
                    for (s32 i = x0c; i < x1c; ++i)
                        blendPixelDirect(fb16, blockLinearOffset(static_cast<u32>(i), rb), highlightColor, ha);
                };

                // Circle drawing with AA - optimized Bresenham
                s32 cx = radius;
                s32 cy = 0;
                s32 radiusError = 0;
                const s32 diameter = radius << 1;
                s32 xChange = 1 - diameter;
                s32 yChange = 0;
                s32 lastCx = cx;
                
                while (cx >= cy) {
                    // Pre-calculate Y coordinates (hoist invariants)
                    const s32 topY1    = topCornerY    - cy;
                    const s32 topY2    = topCornerY    - cx;
                    const s32 bottomY1 = bottomCornerY + cy;
                    const s32 bottomY2 = bottomCornerY + cx;
                    
                    // X span bounds. Left spans: [start, leftCornerX); right: [start, end+1).
                    const s32 leftX1Start  = leftCornerX  - cx;
                    const s32 leftX2Start  = leftCornerX  - cy;
                    const s32 rightX1Start = rightCornerX + 1;
                    const s32 rightX1End   = rightCornerX + cx;
                    const s32 rightX2End   = rightCornerX + cy;

                    // Eight spans across four rows.  hspan computes rowBase once per call.
                    hspan(topY1,    leftX1Start,  leftCornerX);
                    hspan(topY1,    rightX1Start, rightX1End + 1);
                    hspan(topY2,    leftX2Start,  leftCornerX);
                    hspan(topY2,    rightX1Start, rightX2End + 1);
                    hspan(bottomY1, leftX1Start,  leftCornerX);
                    hspan(bottomY1, rightX1Start, rightX1End + 1);
                    hspan(bottomY2, leftX2Start,  leftCornerX);
                    hspan(bottomY2, rightX1Start, rightX2End + 1);
                    
                    // AA pixels at step transitions — rare single-pixel writes.
                    if (__builtin_expect(cx != lastCx && cy > 0, 0)) {
                        const s32 cxAA = cx + 1;
                        
                        // Upper-left AA
                        this->setPixelBlendDst(leftCornerX - cxAA, topY1,     aaColor1);
                        this->setPixelBlendDst(leftCornerX - cxAA, topY1 + 1, aaColor2);
                        this->setPixelBlendDst(leftX2Start,         topY2 - 1, aaColor1);
                        this->setPixelBlendDst(leftX2Start + 1,     topY2 - 1, aaColor2);
                        
                        // Upper-right AA
                        this->setPixelBlendDst(rightCornerX + cxAA, topY1,         aaColor1);
                        this->setPixelBlendDst(rightCornerX + cxAA, topY1 + 1,     aaColor2);
                        this->setPixelBlendDst(rightX2End,           topY2 - 1,     aaColor1);
                        this->setPixelBlendDst(rightX2End - 1,       topY2 - 1,     aaColor2);
                        
                        // Lower-left AA
                        this->setPixelBlendDst(leftCornerX - cxAA, bottomY1,     aaColor1);
                        this->setPixelBlendDst(leftCornerX - cxAA, bottomY1 - 1, aaColor2);
                        this->setPixelBlendDst(leftX2Start,         bottomY2 + 1, aaColor1);
                        this->setPixelBlendDst(leftX2Start + 1,     bottomY2 + 1, aaColor2);
                        
                        // Lower-right AA
                        this->setPixelBlendDst(rightCornerX + cxAA, bottomY1,     aaColor1);
                        this->setPixelBlendDst(rightCornerX + cxAA, bottomY1 - 1, aaColor2);
                        this->setPixelBlendDst(rightX2End,           bottomY2 + 1, aaColor1);
                        this->setPixelBlendDst(rightX2End - 1,       bottomY2 + 1, aaColor2);
                    }
                    
                    lastCx = cx;
                    
                    // Bresenham iteration - optimized
                    cy++;
                    radiusError += yChange;
                    yChange += 2;
                    
                    if (__builtin_expect(((radiusError << 1) + xChange) > 0, 0)) {
                        cx--;
                        radiusError += xChange;
                        xChange += 2;
                    }
                }
            }
            
            ALWAYS_INLINE static void blendPixelDirect(u16* fb16, u32 off, const Color& color, u8 a) noexcept {
                if (a == 0xFu) {
                    reinterpret_cast<Color*>(fb16)[off] = color;
                } else {
                    const u8 invA = static_cast<u8>(15u - a);
                    const Color src = reinterpret_cast<const Color*>(fb16)[off];
                    reinterpret_cast<Color*>(fb16)[off] = Color(
                        static_cast<u8>(((src.r * invA) + (color.r * a)) >> 4u),
                        static_cast<u8>(((src.g * invA) + (color.g * a)) >> 4u),
                        static_cast<u8>(((src.b * invA) + (color.b * a)) >> 4u),
                        a + static_cast<u8>((src.a * invA) >> 4u));
                }
            }

            // dy1sq = (py2+1-cy2)^2 and dy2sq = (py2-1-cy2)^2 are constant across an
            // entire row's arc loop (py2 and cy2 don't change per pixel).  The caller
            // precomputes them once and passes them in, saving 2 multiplications per pixel.
            static inline void sampleAndBlendArcPixel(u16* fb16, s32 xp, u32 rowBase,
                                                      int px2, int cx2,
                                                      long long dy1sq, long long dy2sq,
                                                      long long r2_scaled,
                                                      const Color& color, u8 base_a) noexcept {
                const long long dx1 = px2 + 1 - cx2,  dx2 = px2 - 1 - cx2;
                const long long dx1sq = dx1*dx1, dx2sq = dx2*dx2;
                const int hits = (int)(dx1sq+dy1sq <= r2_scaled)
                               + (int)(dx1sq+dy2sq <= r2_scaled)
                               + (int)(dx2sq+dy1sq <= r2_scaled)
                               + (int)(dx2sq+dy2sq <= r2_scaled);
                if (hits == 0) return;
                const u8 a = (hits == 4) ? color.a : static_cast<u8>((base_a * hits + 2) >> 2);
                if (a == 0u) return;
                blendPixelDirect(fb16, blockLinearOffset(static_cast<u32>(xp), rowBase), color, a);
            }

            ALWAYS_INLINE static void fillRowSpanNEON(u16* fb16, const u32 rowBase,
                                                      const s32 xs, const s32 xe,
                                                      const Color& color) {
                const s32 span = xe - xs;
                if (span <= 0) return;
                const u32 bpX     = static_cast<u32>(xs);
                const s32 prologue = static_cast<s32>((8u - (bpX & 7u)) & 7u);
                const s32 pe      = std::min(prologue, span);

                if (color.a == 0xFu) {
                    // ── Full-opacity: zero reads, pure stores ──────────────────────
                    const uint16x8_t vColor = vdupq_n_u16(color.rgba);
                    s32 i = 0;
                    for (; i < pe; ++i)
                        fb16[blockLinearOffset(bpX + static_cast<u32>(i), rowBase)] = color.rgba;
                    for (; i + 8 <= span; i += 8)
                        vst1q_u16(fb16 + blockLinearOffset(bpX + static_cast<u32>(i), rowBase), vColor);
                    for (; i < span; ++i)
                        fb16[blockLinearOffset(bpX + static_cast<u32>(i), rowBase)] = color.rgba;
                } else {
                    // ── Partial-opacity: NEON blend ────────────────────────────────
                    const u8  alpha  = color.a;
                    const u8  invA   = static_cast<u8>(15u - alpha);
                    const uint16x8_t vAlpha = vdupq_n_u16(alpha);
                    const uint16x8_t vInvA  = vdupq_n_u16(invA);
                    const uint16x8_t vDstR  = vdupq_n_u16(color.r);
                    const uint16x8_t vDstG  = vdupq_n_u16(color.g);
                    const uint16x8_t vDstB  = vdupq_n_u16(color.b);
                    const uint16x8_t vMask4 = vdupq_n_u16(0x000Fu);

                    auto scalarBlend = [&](s32 i) {
                        const u32 off = blockLinearOffset(bpX + static_cast<u32>(i), rowBase);
                        const Color src = reinterpret_cast<const Color*>(fb16)[off];
                        reinterpret_cast<Color*>(fb16)[off] = Color(
                            static_cast<u8>(((src.r * invA) + (color.r * alpha)) >> 4u),
                            static_cast<u8>(((src.g * invA) + (color.g * alpha)) >> 4u),
                            static_cast<u8>(((src.b * invA) + (color.b * alpha)) >> 4u),
                            alpha + static_cast<u8>((src.a * invA) >> 4u));
                    };

                    s32 i = 0;
                    for (; i < pe; ++i) scalarBlend(i);
                    for (; i + 8 <= span; i += 8) {
                        const u32 off = blockLinearOffset(bpX + static_cast<u32>(i), rowBase);
                        const uint16x8_t src16 = vld1q_u16(fb16 + off);
                        const uint16x8_t src_r = vandq_u16(src16, vMask4);
                        const uint16x8_t src_g = vandq_u16(vshrq_n_u16(src16,  4), vMask4);
                        const uint16x8_t src_b = vandq_u16(vshrq_n_u16(src16,  8), vMask4);
                        const uint16x8_t src_a =            vshrq_n_u16(src16, 12);
                        const uint16x8_t r_out = vshrq_n_u16(vmlaq_u16(vmulq_u16(src_r, vInvA), vDstR, vAlpha), 4);
                        const uint16x8_t g_out = vshrq_n_u16(vmlaq_u16(vmulq_u16(src_g, vInvA), vDstG, vAlpha), 4);
                        const uint16x8_t b_out = vshrq_n_u16(vmlaq_u16(vmulq_u16(src_b, vInvA), vDstB, vAlpha), 4);
                        const uint16x8_t a_out = vaddq_u16(vAlpha, vshrq_n_u16(vmulq_u16(src_a, vInvA), 4));
                        vst1q_u16(fb16 + off,
                            vorrq_u16(r_out,
                            vorrq_u16(vshlq_n_u16(g_out,  4),
                            vorrq_u16(vshlq_n_u16(b_out,  8),
                                      vshlq_n_u16(a_out, 12)))));
                    }
                    for (; i < span; ++i) scalarBlend(i);
                }
            }
            
            // --- Optimized rounded rectangle chunk processor ---
            static void processRoundedRectChunk(Renderer* self,
                                                const s32 x, const s32 y,
                                                const s32 w, const s32 h,
                                                const s32 radius,
                                                const Color& color,
                                                const s32 startRow, const s32 endRow)
            {
                if (radius <= 0) return;
        
                const s32 x_end = x + w;
                const s32 y_end = y + h;
        
                // Clamp x/y extents to both framebuffer AND active scissor rectangle
                s32 clip_x     = std::max(0, x);
                s32 clip_x_end = std::min<s32>(cfg::FramebufferWidth, x_end);
                s32 clip_y     = std::max(0, y);
                s32 clip_y_end = std::min<s32>(cfg::FramebufferHeight, y_end);
                if (self->m_scissorDepth != 0) [[unlikely]] {
                    const auto& sc = self->m_scissorStack[self->m_scissorDepth - 1];
                    clip_x     = std::max(clip_x,     static_cast<s32>(sc.x));
                    clip_x_end = std::min(clip_x_end, static_cast<s32>(sc.x_max));
                    clip_y     = std::max(clip_y,     static_cast<s32>(sc.y));
                    clip_y_end = std::min(clip_y_end, static_cast<s32>(sc.y_max));
                    if (clip_x >= clip_x_end || clip_y >= clip_y_end) return;
                }
        
                const s32 left_arc_end    = x + radius - 1;
                const s32 right_arc_start = x_end - radius;
                const s32 top_arc_end     = y + radius - 1;
                const s32 bottom_arc_start = y_end - radius;

                const long long r2_scaled = 4LL * radius * radius;
                const long long reject_sq  = (2LL*radius + 2)*(2LL*radius + 2);

                const s32 cx2_left  = 2 * (x + radius);
                const s32 cx2_right = 2 * (x_end - radius);

                const u8 base_a = color.a;
                u16* fb16 = reinterpret_cast<u16*>(self->m_currentFramebuffer);

                const int cy2_top = 2 * (y + radius);
                const int cy2_bot = 2 * (y_end - radius);

                for (s32 yc = startRow; yc < endRow; ++yc) {
                    if (yc < clip_y || yc >= clip_y_end) continue;

                    const u32 rowBase = blockLinearYPart(static_cast<u32>(yc), offsetWidthVar);
                    const bool is_top = (yc <= top_arc_end);
                    const bool in_arc_rows = is_top || (yc >= bottom_arc_start);

                    if (!in_arc_rows) {
                        fillRowSpanNEON(fb16, rowBase,
                                        std::max(clip_x, x), std::min(clip_x_end, x_end), color);
                        continue;
                    }

                    const int cy2 = is_top ? cy2_top : cy2_bot;
                    const int py2 = 2*yc + 1;
                    const long long dy = py2 - cy2;
                    if (dy*dy > reject_sq) continue;

                    // dy1 = dy+1, dy2 = dy-1 — both constant for this row.
                    // Precompute their squares here so sampleAndBlendArcPixel
                    // only needs to compute the per-pixel dx terms.
                    const long long dy1sq = (dy + 1) * (dy + 1);
                    const long long dy2sq = (dy - 1) * (dy - 1);

                    const s32 xe = std::min(clip_x_end, x_end);

                    // Left corner arc
                    for (s32 xp = std::max(clip_x, x); xp <= left_arc_end && xp < xe; ++xp)
                        sampleAndBlendArcPixel(fb16, xp, rowBase,
                                               2*xp + 1, cx2_left, dy1sq, dy2sq,
                                               r2_scaled, color, base_a);

                    // Middle flat span — NEON fill
                    fillRowSpanNEON(fb16, rowBase,
                                    std::max(clip_x, left_arc_end + 1),
                                    std::min(xe, right_arc_start), color);

                    // Right corner arc
                    for (s32 xp = std::max(clip_x, right_arc_start); xp < xe; ++xp)
                        sampleAndBlendArcPixel(fb16, xp, rowBase,
                                               2*xp + 1, cx2_right, dy1sq, dy2sq,
                                               r2_scaled, color, base_a);
                }
            }


            /**
             * @brief Draws a rounded rectangle of given sizes and corner radius (Multi-threaded)
             *
             * @param x X pos
             * @param y Y pos
             * @param w Width
             * @param h Height
             * @param radius Corner radius
             * @param color Color
             */
            inline void drawRoundedRectMultiThreaded(const s32 x, const s32 y, const s32 w, const s32 h, const s32 radius, const Color& color) {
                if (w <= 0 || h <= 0) return;
                
                // Calculate clipped bounds for early exit check
                const s32 clampedX = std::max(0, x);
                const s32 clampedY = std::max(0, y);
                const s32 clampedXEnd = std::min(static_cast<s32>(cfg::FramebufferWidth), x + w);
                const s32 clampedYEnd = std::min(static_cast<s32>(cfg::FramebufferHeight), y + h);
                
                // Early exit if nothing to draw after clamping
                if (clampedX >= clampedXEnd || clampedY >= clampedYEnd) return;
                
                // Calculate visible dimensions
                const s32 visibleHeight = clampedYEnd - clampedY;
                
                // Dynamic chunk size based on visible rectangle height
                const s32 chunkSize = std::max(1, visibleHeight / (static_cast<s32>(ult::numThreads) * 2));
                std::atomic<s32> currentRow(clampedY);
                
                auto threadTask = [&]() {
                    s32 startRow, endRow;
                    while ((startRow = currentRow.fetch_add(chunkSize)) < clampedYEnd) {
                        endRow = std::min(startRow + chunkSize, clampedYEnd);
                        processRoundedRectChunk(this, x, y, w, h, radius, color, startRow, endRow);
                    }
                };
                
                // Launch threads using ult::renderThreads array
                for (unsigned i = 0; i < static_cast<unsigned>(ult::numThreads); ++i) {
                    ult::renderThreads[i] = std::thread(threadTask);
                }
                for (unsigned i = 0; i < static_cast<unsigned>(ult::numThreads); ++i)
                    ult::renderThreads[i].join();
            }
            
            /**
             * @brief Draws a rounded rectangle of given sizes and corner radius (Single-threaded)
             *
             * @param x X pos
             * @param y Y pos
             * @param w Width
             * @param h Height
             * @param radius Corner radius
             * @param color Color
             */
            inline void drawRoundedRectSingleThreaded(s32 x, s32 y, s32 w, s32 h, s32 radius, const Color& color) {
                if (w <= 0 || h <= 0) return;
            
                const s32 clampedY = std::max(0, y);
                const s32 clampedYEnd = std::min(static_cast<s32>(cfg::FramebufferHeight), y + h);
            
                // Early exit if nothing to draw after clamping
                if (x + w <= 0 || x >= static_cast<s32>(cfg::FramebufferWidth) || clampedY >= clampedYEnd)
                    return;
            
                processRoundedRectChunk(this, x, y, w, h, radius, color, clampedY, clampedYEnd);
            }
            
            inline void drawRoundedRect(s32 x, s32 y, s32 w, s32 h, s32 radius, Color color) {
                if (ult::expandedMemory)
                    drawRoundedRectMultiThreaded(x, y, w, h, radius, color);
                else
                    drawRoundedRectSingleThreaded(x, y, w, h, radius, color);
            }
            
                                                
            inline void drawUniformRoundedRect(const s32 x, const s32 y, const s32 w, const s32 h, const Color& color) {
                const s32 radius = h >> 1;
                // Clamp to framebuffer bounds
                s32 clip_left   = std::max(0, x);
                s32 clip_top    = std::max(0, y);
                s32 clip_right  = std::min(static_cast<s32>(cfg::FramebufferWidth),  x + w);
                s32 clip_bottom = std::min(static_cast<s32>(cfg::FramebufferHeight), y + h);

                // Also clamp to active scissor rectangle
                if (this->m_scissorDepth != 0) [[unlikely]] {
                    const auto& sc = this->m_scissorStack[this->m_scissorDepth - 1];
                    clip_left   = std::max(clip_left,   static_cast<s32>(sc.x));
                    clip_right  = std::min(clip_right,  static_cast<s32>(sc.x_max));
                    clip_top    = std::max(clip_top,    static_cast<s32>(sc.y));
                    clip_bottom = std::min(clip_bottom, static_cast<s32>(sc.y_max));
                }

                if (clip_left >= clip_right || clip_top >= clip_bottom) return;

                const s32 x_end          = x + w;
                const s32 corner_x_left  = x + radius;
                const s32 corner_x_right = x_end - radius - 1;
                const s32 corner_y_top   = y + radius;
                const s32 corner_y_bot   = y + h - radius - 1;
                const float r_f          = static_cast<float>(radius);
                const float r2           = r_f * r_f;
                const float aa_thresh    = r2 + 2.0f * r_f + 1.0f;
                const u8  base_a         = color.a;

                u16* const fb16 = reinterpret_cast<u16*>(this->m_currentFramebuffer);

                for (s32 yc = clip_top; yc < clip_bottom; ++yc) {
                    const u32 rowBase = blockLinearYPart(static_cast<u32>(yc), offsetWidthVar);
                    const bool in_corners = (yc < corner_y_top) || (yc > corner_y_bot);

                    if (!in_corners) {
                        // ── Full-width flat row — NEON fill, zero getPixelOffset calls ──
                        const s32 span_start = std::max(x, clip_left);
                        const s32 span_end   = std::min(x_end, clip_right);
                        fillRowSpanNEON(fb16, rowBase, span_start, span_end, color);
                        continue;
                    }

                    // ── Corner rows ────────────────────────────────────────────────────
                    const float dy      = (yc < corner_y_top) ? static_cast<float>(corner_y_top - yc)
                                                               : static_cast<float>(yc - corner_y_bot);
                    const float dy_sq   = dy * dy;
                    if (dy_sq > aa_thresh) continue;

                    const float dy_half    = dy - 0.5f;
                    const float dy_half_sq = dy_half * dy_half;

                    const s32 span_start = std::max(x,   clip_left);
                    const s32 span_end   = std::min(x_end, clip_right);
                    s32 xc = span_start;

                    // Shared arc-pixel blender: blend one pixel at xc given its dx from arc centre.
                    auto arcPixel = [&](s32 xc_, float dx) __attribute__((always_inline)) {
                        const float dx_sq = dx * dx;
                        const float d2    = dx_sq + dy_sq;
                        const u32 off = blockLinearOffset(static_cast<u32>(xc_), rowBase);
                        if (d2 <= r2) {
                            blendPixelDirect(fb16, off, color, base_a);
                        } else if (d2 <= aa_thresh) {
                            const float dx_half    = dx - 0.5f;
                            const float dx_half_sq = dx_half * dx_half;
                            const u32 cov = (u32)(dx_sq      + dy_sq      <= r2)
                                          + (u32)(dx_half_sq + dy_sq      <= r2)
                                          + (u32)(dx_sq      + dy_half_sq <= r2)
                                          + (u32)(dx_half_sq + dy_half_sq <= r2);
                            if (cov)
                                blendPixelDirect(fb16, off, color,
                                    static_cast<u8>((base_a * cov + 2) >> 2));
                        }
                    };

                    // Left corner arc
                    const s32 left_end = std::min(corner_x_left + 1, span_end);
                    for (; xc < left_end; ++xc)
                        arcPixel(xc, static_cast<float>(corner_x_left - xc));

                    // Middle section of corner row — NEON fill
                    const s32 mid_end = std::min(corner_x_right, span_end);
                    fillRowSpanNEON(fb16, rowBase, xc, mid_end, color);
                    xc = mid_end;

                    // Right corner arc
                    for (; xc < span_end; ++xc)
                        arcPixel(xc, static_cast<float>(xc - corner_x_right));
                }
            }

            ALWAYS_INLINE static u32 blockLinearYPart(u32 y, u32 owv) noexcept {
                return ((((y & 127u) >> 4u) + ((y >> 7u) * owv)) << 9u)
                     + ((y & 8u) << 5u) + ((y & 6u) << 4u) + ((y & 1u) << 3u);
            }

            ALWAYS_INLINE static u32 blockLinearOffset(u32 px, u32 yPart) noexcept {
                return yPart + ((px >> 5u) << 12u) + ((px & 16u) << 3u) + ((px & 8u) << 1u) + (px & 7u);
            }

            template<typename Fn>
            static void dispatchRowChunks(u32 totalRows, Fn fn) {
                const u32 n     = ult::numThreads;
                const u32 chunk = (totalRows + n - 1u) / n;
                u32 launched = 0u;
                for (u32 t = 0u; t < n; ++t) {
                    const u32 s = t * chunk;
                    if (s >= totalRows) break;
                    const u32 e = std::min(s + chunk, totalRows);
                    ult::renderThreads[launched++] = std::thread([&fn, s, e]() { fn(s, e); });
                }
                for (u32 t = 0u; t < launched; ++t) ult::renderThreads[t].join();
            }

            inline void processBMPChunk(const u32 x, const u32 y, const s32 imageW, const u8* preprocessedData,
                                        const s32 startRow, const s32 endRow, const u8 globalAlphaLimit,
                                        const bool useBarrier = true, const bool preserveAlpha = false)
            {
                const s32 bytesPerRow = imageW * 2;

                void* const rawFB        = this->m_currentFramebuffer;
                u16* const fb16          = reinterpret_cast<u16*>(rawFB);
                Color* const framebuffer = static_cast<Color*>(rawFB);

                const bool hasScissor = Renderer::get().m_scissorDepth != 0;
                const auto scissor    = hasScissor ? Renderer::get().m_scissorStack[Renderer::get().m_scissorDepth - 1] : ScissoringConfig{};
                const u32 owv = offsetWidthVar;

                // Prologue length: pixels from x until first 8-aligned destination column.
                // For all NEON groups, dst x is 8-aligned so 8 pixels are contiguous in fb.
                //
                // neonEnd must be computed relative to prologueLen, NOT simply imageW & ~7.
                // imageW & ~7 is wrong when prologueLen > 0: the NEON groups start at
                // prologueLen, so their endpoints are prologueLen+8, prologueLen+16, etc.
                // Using imageW & ~7 causes the last group to either read past the end of
                // preprocessedData (OOB) or overlap with the epilogue (double-write).
                const s32 prologueLen = static_cast<s32>((8u - (x & 7u)) & 7u);
                const s32 neonEnd     = (imageW > prologueLen)
                    ? prologueLen + static_cast<s32>((static_cast<u32>(imageW - prologueLen)) & ~7u)
                    : prologueLen;

                for (s32 y1 = startRow; y1 < endRow; ++y1) {
                    const u32 baseY = y + static_cast<u32>(y1);
                    if (hasScissor && (baseY < scissor.y || baseY >= scissor.y_max)) [[unlikely]] continue;

                    const u32 yPart = blockLinearYPart(baseY, owv);
                    const u8* const rowPtr = preprocessedData + (y1 * bytesPerRow);

                    if (__builtin_expect(!hasScissor, 1)) {
                        // ── Fast path: no scissor ─────────────────────────────────────────
                        // NEON constants live here, not at function entry: the scissor path
                        // is scalar-only and never uses them.  Hoisting them unconditionally
                        // wasted 5 vdup instructions on every scissored row.
                        const uint8x8_t alpha_limit_vec = vdup_n_u8(globalAlphaLimit);
                        const uint8x8_t mask_low8 = vdup_n_u8(0x0Fu);
                        const uint8x8_t vZero8    = vdup_n_u8(0u);
                        const uint16x8_t vMask4   = vdupq_n_u16(0x0Fu);
                        const uint16x8_t v15_16   = vdupq_n_u16(15u);

                        // Scalar pixel writer for prologue and epilogue.
                        auto writePixelBMP = [&](s32 x1) {
                            const u8 p1 = rowPtr[x1 << 1], p2 = rowPtr[(x1 << 1) + 1];
                            const u8 av = std::min<u8>(p2 & 0x0Fu, globalAlphaLimit);
                            if (av == 0) return;
                            const u32 off = blockLinearOffset(x + static_cast<u32>(x1), yPart);
                            Color& dst = framebuffer[off];
                            dst.r = blendColor(dst.r, p1 >> 4,    av);
                            dst.g = blendColor(dst.g, p1 & 0x0Fu, av);
                            dst.b = blendColor(dst.b, p2 >> 4,    av);
                            if (!preserveAlpha)
                                dst.a = static_cast<u8>(av + ((dst.a * (0xF - av)) >> 4));
                        };

                        // Scalar prologue — pixels until first 8-aligned dst column
                        const s32 p_end = std::min(prologueLen, imageW);
                        for (s32 x1 = 0; x1 < p_end; ++x1)
                            writePixelBMP(x1);

                        // ── NEON 8-pixel blend loop ───────────────────────────────────────
                        // 8 consecutive pixels at an 8-aligned dst-x are contiguous in the
                        // block-linear framebuffer, so vld1q_u16 / vst1q_u16 apply directly.
                        for (s32 x1 = p_end; x1 < neonEnd; x1 += 8) {
                            const uint8x8x2_t packed = vld2_u8(rowPtr + (x1 << 1));
                            const uint8x8_t sa = vmin_u8(vand_u8(packed.val[1], mask_low8), alpha_limit_vec);

                            // Skip group if all 8 pixels are transparent
                            if (vget_lane_u64(vreinterpret_u64_u8(sa), 0) == 0u) continue;

                            const u32 off = blockLinearOffset(x + static_cast<u32>(x1), yPart);
                            const uint16x8_t dst16 = vld1q_u16(fb16 + off);

                            const uint16x8_t va16  = vmovl_u8(sa);
                            const uint16x8_t via16 = vsubq_u16(v15_16, va16);

                            const uint16x8_t sr16 = vmovl_u8(vshr_n_u8(packed.val[0], 4));
                            const uint16x8_t sg16 = vmovl_u8(vand_u8(packed.val[0], mask_low8));
                            const uint16x8_t sb16 = vmovl_u8(vshr_n_u8(packed.val[1], 4));

                            const uint16x8_t dr16 = vandq_u16(dst16, vMask4);
                            const uint16x8_t dg16 = vandq_u16(vshrq_n_u16(dst16, 4), vMask4);
                            const uint16x8_t db16 = vandq_u16(vshrq_n_u16(dst16, 8), vMask4);
                            const uint16x8_t da16 = vshrq_n_u16(dst16, 12);

                            const uint16x8_t r_out = vshrq_n_u16(vmlaq_u16(vmulq_u16(dr16, via16), sr16, va16), 4);
                            const uint16x8_t g_out = vshrq_n_u16(vmlaq_u16(vmulq_u16(dg16, via16), sg16, va16), 4);
                            const uint16x8_t b_out = vshrq_n_u16(vmlaq_u16(vmulq_u16(db16, via16), sb16, va16), 4);
                            const uint16x8_t a_out = preserveAlpha
                                ? da16
                                : vaddq_u16(va16, vshrq_n_u16(vmulq_u16(da16, via16), 4));

                            uint16x8_t result = vorrq_u16(r_out,
                                vorrq_u16(vshlq_n_u16(g_out, 4),
                                vorrq_u16(vshlq_n_u16(b_out, 8),
                                          vshlq_n_u16(a_out, 12))));

                            // Preserve original dst for fully-transparent pixels
                            const uint16x8_t zeroMask = vreinterpretq_u16_s16(
                                vmovl_s8(vreinterpret_s8_u8(vceq_u8(sa, vZero8))));
                            result = vbslq_u16(zeroMask, dst16, result);

                            vst1q_u16(fb16 + off, result);
                        }

                        // Scalar epilogue — remaining pixels after last full NEON group
                        for (s32 x1 = neonEnd; x1 < imageW; ++x1)
                            writePixelBMP(x1);
                    } else {
                        // ── Scissor path: scalar only ─────────────────────────────────────
                        // Same pixel writer — the only addition is the x-range guard.
                        auto writePixelBMP = [&](s32 x1) {
                            const u8 p1 = rowPtr[x1 << 1], p2 = rowPtr[(x1 << 1) + 1];
                            const u8 av = std::min<u8>(p2 & 0x0Fu, globalAlphaLimit);
                            if (av == 0) return;
                            const u32 off = blockLinearOffset(x + static_cast<u32>(x1), yPart);
                            Color& dst = framebuffer[off];
                            dst.r = blendColor(dst.r, p1 >> 4,    av);
                            dst.g = blendColor(dst.g, p1 & 0x0Fu, av);
                            dst.b = blendColor(dst.b, p2 >> 4,    av);
                            if (!preserveAlpha)
                                dst.a = static_cast<u8>(av + ((dst.a * (0xF - av)) >> 4));
                        };
                        for (s32 x1 = 0; x1 < imageW; ++x1) {
                            const u32 px = x + static_cast<u32>(x1);
                            if (px < scissor.x || px >= scissor.x_max) continue;
                            writePixelBMP(x1);
                        }
                    }
                }

                if (useBarrier) ult::inPlotBarrier.arrive_and_wait();
            }
            
            // --- Draw bitmap RGBA4444 ---
            inline void drawBitmapRGBA4444(const u32 x, const u32 y, const u32 imageW, const u32 imageH,
                                           const u8* preprocessedData, float opacity = 1.0f, bool preserveAlpha = false)
            {
                const u8 globalAlphaLimit = static_cast<u8>(0xF * opacity);

                // Narrow images: single-threaded to avoid thread-spawn overhead.
                if (imageW < 448u) {
                    processBMPChunk(x, y, imageW, preprocessedData, 0, imageH, globalAlphaLimit, false, preserveAlpha);
                    return;
                }

                dispatchRowChunks(imageH, [=, this](u32 s, u32 e) {
                    processBMPChunk(x, y, imageW, preprocessedData, s, e, globalAlphaLimit, true, preserveAlpha);
                });
            }
            

            template<bool kDark>
            ALWAYS_INLINE static void drawWallpaperRows(
                const u32 rowStart,
                const u32 rowEnd,
                tsl::Color* const framebuffer,
                const u8* const src_base,
                const u32* const s_yParts,
                const u32* const s_xGroupParts,
                const u8 globalAlphaLimit,
                const u8 bg_r,
                const u8 bg_g,
                const u8 bg_b,
                const u8 bg_a)
            {
                static constexpr u32 kW  = 448u;
                static constexpr u32 kGW = kW / 8u; // 56 groups
            
                // ── NEON constants — hoisted once per worker ─────────────────────────────
                const uint8x8_t  v_mask4     = vdup_n_u8(0x0Fu);
                const uint8x8_t  v_alpha_lim = vdup_n_u8(globalAlphaLimit);
                const uint8x8_t  v_bg_a4     = vdup_n_u8(static_cast<u8>(bg_a << 4));
            
                // Only used in !kDark path; compiler should dead-strip in kDark=true instantiation.
                const uint8x8_t  v15         = vdup_n_u8(15u);
                const uint16x8_t v_bg_r16    = vdupq_n_u16(bg_r);
                const uint16x8_t v_bg_g16    = vdupq_n_u16(bg_g);
                const uint16x8_t v_bg_b16    = vdupq_n_u16(bg_b);
            
                const auto do_pair = [&](const u32 base, const u8* rs, const u32 g) {
                    const uint8x16x2_t raw = vld2q_u8(rs + (g << 4u));
            
                    // Low half = group g
                    const uint8x8_t sr0 = vshr_n_u8(vget_low_u8(raw.val[0]), 4);
                    const uint8x8_t sg0 = vand_u8   (vget_low_u8(raw.val[0]), v_mask4);
                    const uint8x8_t sb0 = vshr_n_u8(vget_low_u8(raw.val[1]), 4);
                    const uint8x8_t sa0 = vmin_u8(vand_u8(vget_low_u8(raw.val[1]), v_mask4), v_alpha_lim);
            
                    // High half = group g+1
                    const uint8x8_t sr1 = vshr_n_u8(vget_high_u8(raw.val[0]), 4);
                    const uint8x8_t sg1 = vand_u8   (vget_high_u8(raw.val[0]), v_mask4);
                    const uint8x8_t sb1 = vshr_n_u8(vget_high_u8(raw.val[1]), 4);
                    const uint8x8_t sa1 = vmin_u8(vand_u8(vget_high_u8(raw.val[1]), v_mask4), v_alpha_lim);
            
                    uint8x8_t or0, og0, ob0, or1, og1, ob1;
            
                    if constexpr (kDark) {
                        or0 = vshrn_n_u16(vmull_u8(sr0, sa0), 4);
                        og0 = vshrn_n_u16(vmull_u8(sg0, sa0), 4);
                        ob0 = vshrn_n_u16(vmull_u8(sb0, sa0), 4);
            
                        or1 = vshrn_n_u16(vmull_u8(sr1, sa1), 4);
                        og1 = vshrn_n_u16(vmull_u8(sg1, sa1), 4);
                        ob1 = vshrn_n_u16(vmull_u8(sb1, sa1), 4);
                    } else {
                        const uint16x8_t ia0 = vmovl_u8(vsub_u8(v15, sa0));
                        or0 = vshrn_n_u16(vaddq_u16(vmulq_u16(ia0, v_bg_r16), vmull_u8(sr0, sa0)), 4);
                        og0 = vshrn_n_u16(vaddq_u16(vmulq_u16(ia0, v_bg_g16), vmull_u8(sg0, sa0)), 4);
                        ob0 = vshrn_n_u16(vaddq_u16(vmulq_u16(ia0, v_bg_b16), vmull_u8(sb0, sa0)), 4);
            
                        const uint16x8_t ia1 = vmovl_u8(vsub_u8(v15, sa1));
                        or1 = vshrn_n_u16(vaddq_u16(vmulq_u16(ia1, v_bg_r16), vmull_u8(sr1, sa1)), 4);
                        og1 = vshrn_n_u16(vaddq_u16(vmulq_u16(ia1, v_bg_g16), vmull_u8(sg1, sa1)), 4);
                        ob1 = vshrn_n_u16(vaddq_u16(vmulq_u16(ia1, v_bg_b16), vmull_u8(sb1, sa1)), 4);
                    }
            
                    vst2_u8(reinterpret_cast<u8*>(framebuffer + base),
                            uint8x8x2_t{{vorr_u8(vshl_n_u8(og0, 4), or0), vorr_u8(v_bg_a4, ob0)}});
                    vst2_u8(reinterpret_cast<u8*>(framebuffer + base + 16),
                            uint8x8x2_t{{vorr_u8(vshl_n_u8(og1, 4), or1), vorr_u8(v_bg_a4, ob1)}});
                };
            
                for (u32 y = rowStart; y < rowEnd; ++y) {
                    const u32       yPart = s_yParts[y];
                    const u8* const rs    = src_base + y * (kW * 2u);
            
                    for (u32 g = 0u; g < kGW; g += 2u)
                        do_pair(yPart + s_xGroupParts[g], rs, g);
                }
            }

            // --- Draw wallpaper ---
            // =============================================================================
            // draw_wallpaper_direct
            //
            // Full-frame specialised replacement for renderer->drawWallpaper().
            //
            // Fixed contract:
            //   • always 448×720 full-screen (correctFrameSize guaranteed)
            //   • always opacity 1.0 in normal operation
            //   • always preserveAlpha == true
            //   • always called immediately after fillScreen (no scissoring active)
            //
            // Improvements over the generic processBMPChunk path:
            //
            //  1. Static precomputed offset tables (yParts[720] + xGroupParts[56]).
            //     Generic code recomputes / re-allocates setup data every call.
            //     Here those values are computed once (≈ 3 KB static) and reused forever.
            //
            //  2. NEON 8-pixel contiguous stores.
            //     The block-linear swizzle maps each run of 8 consecutive pixels to 8
            //     consecutive framebuffer slots.  We exploit that directly with vld2_u8 /
            //     vst2_u8 instead of generic scalar scatter logic.
            //
            //  3. Background color read once, not per pixel.
            //     fillScreen writes one constant Color to every slot, so dst.a (preserved
            //     by preserveAlpha==true) is the same everywhere.  Read framebuffer[0]
            //     once and broadcast into NEON lanes.
            //
            //  4. No scissoring checks anywhere in the hot loop.
            //
            // Threading: identical to drawBitmapRGBA4444 — ult::numThreads threads,
            // each processing a row chunk and then joining.
            // =============================================================================
            inline void drawWallpaper() {
                // ── Same entry guards as Renderer::drawWallpaper() ──────────────────────
                if (!ult::expandedMemory || ult::refreshWallpaper.load(std::memory_order_acquire)) return;
            
                ult::inPlot.store(true, std::memory_order_release);
            
                if (!ult::wallpaperData.empty() &&
                    !ult::refreshWallpaper.load(std::memory_order_acquire) &&
                    ult::correctFrameSize)
                {
                    // ── Static precomputed offset tables ─────────────────────────────────
                    // yParts[y]       : y-contribution to the block-linear framebuffer offset.
                    // xGroupParts[g]  : x-contribution for the start of 8-pixel group g
                    //                   (g = 0..55; 56 groups × 8 pixels = 448 px/row).
                    //
                    // Pure functions of the fixed 448×720 / offsetWidthVar=112 geometry.
                    // Initialised on the first call; never reallocated.
                    // Total static storage: 720×4 + 56×4 = 3,104 bytes.
                    static constexpr u32 kW  = 448u;
                    static constexpr u32 kH  = 720u;
                    static constexpr u32 kGW = kW / 8u;  // 56 groups of 8 pixels per row
            
                    static u32  s_yParts[kH];
                    static u32  s_xGroupParts[kGW];
                    static bool s_tables_ready = false;
            
                    if (__builtin_expect(!s_tables_ready, 0)) {
                        const u32 owv = offsetWidthVar;
                        for (u32 yi = 0u; yi < kH; ++yi)
                            s_yParts[yi] = blockLinearYPart(yi, owv);
                        for (u32 g = 0u; g < kGW; ++g) {
                            const u32 x = g * 8u;
                            s_xGroupParts[g] = ((x >> 5u) << 12u) + ((x & 16u) << 3u) + ((x & 8u) << 1u);
                        }
                        s_tables_ready = true;
                    }
            
                    tsl::Color* const framebuffer =
                        static_cast<tsl::Color*>(this->m_currentFramebuffer);

                    // ── Background color — always black RGB, alpha from theme ────────────
                    // The wallpaper is always composited over a black background.
                    // bg RGB is hardcoded 0 so the compiler always selects the kDark=true
                    // template instantiation, which eliminates the three background-channel
                    // multiply-add pairs (vmulq_u16 × 3 + vaddq_u16 × 3) from the inner loop.
                    //
                    // bg_a mirrors all three branches of a(defaultBackgroundColor).a exactly:
                    //   disableTransparency + useOpaqueScreenshots → 0xF (fully opaque)
                    //   disableTransparency only                   → max(c.a, 0xE)
                    //   normal fade                                → min(c.a, opacity_limit)
                    //
                    // globalAlphaLimit caps the wallpaper source pixel alphas and is always
                    // 0xF * s_opacity — unaffected by disableTransparency (same as
                    // drawBitmapRGBA4444).
                    //
                    // For fully-transparent wallpaper pixels (src_a == 0) the kDark path
                    // emits {r=0, g=0, b=0, a=bg_a} — identical to what fillScreen wrote —
                    // so no pixel is left uninitialised and callers can skip fillScreen entirely.
                    const u8 globalAlphaLimit =
                        static_cast<u8>(0xF * Renderer::s_opacity);
                    const u8 bg_a = ult::disableTransparency
                        ? (ult::useOpaqueScreenshots
                               ? 0xFu
                               : (tsl::defaultBackgroundColor.a > 0xEu
                                      ? tsl::defaultBackgroundColor.a
                                      : 0xEu))
                        : (tsl::defaultBackgroundColor.a < globalAlphaLimit
                               ? tsl::defaultBackgroundColor.a
                               : globalAlphaLimit);
            
                    const u8* const src_base = ult::wallpaperData.data();
            
                    // bg is always black — always use the kDark=true fast path.
                    dispatchRowChunks(kH, [=](u32 rowStart, u32 rowEnd) {
                        drawWallpaperRows<true>(
                            rowStart, rowEnd,
                            framebuffer,
                            src_base,
                            s_yParts,
                            s_xGroupParts,
                            globalAlphaLimit,
                            0u, 0u, 0u, bg_a
                        );
                    });
                }
            
                ult::inPlot.store(false, std::memory_order_release);
            }

            /**
             * @brief Draws a RGBA8888 bitmap from memory
             *
             * @param x X start position
             * @param y Y start position
             * @param w Bitmap width
             * @param h Bitmap height
             * @param bmp Pointer to bitmap data
             */
            inline void drawBitmap(s32 x, s32 y, s32 w, s32 h, const u8 *bmp) {
                if (w <= 0 || h <= 0) [[unlikely]] return;
                
                const u8* __restrict__ src = bmp;
                
                // Pre-compute alpha limit once using global opacity
                const u8 alphaLimit = static_cast<u8>(0xF * Renderer::s_opacity);
                
                // Small-bitmap fast path (icons ≤ 8×8).
                // Simple loop compiles to a single copy of the pixel body at -Os,
                // identical in speed for counts ≤ 8 and far smaller in the binary
                // than the previous Duff's-device unroll (which generated 8 copies).
                if (w <= 8 && h <= 8) [[likely]] {
                    for (s32 py = 0; py < h; ++py) {
                        const s32 rowY = y + py;
                        for (s32 xi = 0; xi < w; ++xi) {
                            u8 alpha = src[3] >> 4;
                            if (alpha > 0) {
                                alpha = (alpha < alphaLimit) ? alpha : alphaLimit;
                                const Color c = {static_cast<u8>(src[0] >> 4), static_cast<u8>(src[1] >> 4),
                                               static_cast<u8>(src[2] >> 4), alpha};
                                setPixelBlendSrc(x + xi, rowY, a(c));
                            }
                            src += 4;
                        }
                    }
                    return;
                }
                
                // Optimized scalar path for larger bitmaps
                for (s32 py = 0; py < h; ++py) {
                    const s32 rowY = y + py;
                    s32 px = x;
                    const u8* rowEnd = src + (w * 4);
                    
                    // Prefetch first cache line
                    __builtin_prefetch(src, 0, 3);
                    
                    // Process all pixels in the row
                    while (src < rowEnd) {
                        // Prefetch next cache line when src crosses a 64-byte boundary.
                        // Fires every 16 pixels — regular and predictable, so no [[unlikely]].
                        if (((uintptr_t)src & 63) == 0) {
                            __builtin_prefetch(src + 64, 0, 3);
                        }
                        
                        u8 alpha = src[3] >> 4;
                        if (alpha > 0) {
                            alpha = (alpha < alphaLimit) ? alpha : alphaLimit;
                            const Color c = {static_cast<u8>(src[0] >> 4), static_cast<u8>(src[1] >> 4), 
                                           static_cast<u8>(src[2] >> 4), alpha};
                            setPixelBlendSrc(px, rowY, c);
                        }
                        px++;
                        src += 4;
                    }
                }
            }
            
            /**
             * @brief Fills the entire layer with a given color
             *
             * @param color Color
             */
            inline void fillScreen(const Color& color) {
                // std::fill_n at -Os compiles to a scalar loop (auto-vectorisation is
                // disabled at -Os).  An explicit NEON loop processes 8 u16 pixels per
                // iteration — same loop-body instruction count, 8× fewer iterations.
                // For the normal 448×720 framebuffer (322,560 pixels ÷ 8 = 40,320 groups)
                // the epilogue is dead code; kept for safety on other frame sizes.
                u16* const fb  = reinterpret_cast<u16*>(this->m_currentFramebuffer);
                const size_t n = this->m_framebuffer.fb_size >> 1u; // byte count → u16 count
                const uint16x8_t vc = vdupq_n_u16(color.rgba);
                size_t i = 0;
                for (; i + 8u <= n; i += 8u) vst1q_u16(fb + i, vc);
                for (; i < n; ++i)            fb[i] = color.rgba;
            }
            
            /**
             * @brief Clears the layer (With transparency)
             *
             */
            inline void clearScreen() {
                this->fillScreen(Color(0x0, 0x0, 0x0, 0x0)); // Fully transparent
            }
            
            const stbtt_fontinfo& getStandardFont() const {
                return m_stdFont;
            }
                    
            
            // Optimized unified drawString method with thread safety
            inline std::pair<s32, s32> drawString(const std::string& originalString, bool monospace, 
                                                  const s32 x, const s32 y, const u32 fontSize, 
                                                  const Color& defaultColor, const ssize_t maxWidth = 0, 
                                                  bool draw = true,
                                                  const Color* highlightColor = nullptr,
                                                  const std::vector<std::string>* specialSymbols = nullptr,
                                                  const u32 highlightStartChar = 0,
                                                  const u32 highlightEndChar = 0,
                                                  const bool useNotificationCache = false) {
                
                // Thread-safe translation cache access
                const std::string* text = &originalString;
                std::string translatedText;
                
                {
                    std::shared_lock<std::shared_mutex> readLock(s_translationCacheMutex);
                    auto translatedIt = ult::translationCache.find(originalString);
                    if (translatedIt != ult::translationCache.end()) {
                        translatedText = translatedIt->second;
                        text = &translatedText;
                    }
                }
                
                if (text->empty() || fontSize == 0) return {0, 0};
                
                const float maxWidthLimit = maxWidth > 0 ? x + maxWidth : std::numeric_limits<float>::max();
                
                // Check if highlighting is enabled
                const bool highlightingEnabled = highlightColor && highlightStartChar != 0 && highlightEndChar != 0;
                
                // Get font metrics once
                const auto fontMetrics = FontManager::getFontMetricsForCharacter('A', fontSize);
                const s32 lineHeight = static_cast<s32>(fontMetrics.lineHeight);
                
                // Fast ASCII check with early exit
                bool isAsciiOnly = true;
                const char* textPtr = text->data();
                const char* textEnd = textPtr + text->size();
                
                for (const char* p = textPtr; p < textEnd; ++p) {
                    if (static_cast<unsigned char>(*p) > 127) {
                        isAsciiOnly = false;
                        break;
                    }
                }
                
                s32 maxX = x, currX = x, currY = y;
                s32 maxY = y + lineHeight;
                bool inHighlight = false;
                const Color* currentColor = &defaultColor;
                
                // Main processing loop
                if (isAsciiOnly && !specialSymbols) {
                    // Fast ASCII-only path
                    for (const char* p = textPtr; p < textEnd && currX < maxWidthLimit; ++p) {
                        u32 currCharacter = static_cast<u32>(*p);
                        
                        // Handle highlighting
                        if (highlightingEnabled) {
                            if (currCharacter == highlightStartChar) {
                                inHighlight = true;
                                currentColor = &defaultColor;
                            } else if (currCharacter == highlightEndChar) {
                                inHighlight = false;
                                currentColor = &defaultColor;
                            } else {
                                currentColor = inHighlight ? highlightColor : &defaultColor;
                            }
                        }
                        
                        // Handle newline
                        if (currCharacter == '\n') {
                            maxX = std::max(currX, maxX);
                            currX = x;
                            currY += lineHeight;
                            maxY = std::max(maxY, currY + lineHeight);
                            continue;
                        }
                        
                        // Get glyph — hold shared_ptr to keep the glyph alive,
                        // but pass raw ptr to skip atomic ref-count overhead.
                        std::shared_ptr<FontManager::Glyph> glyph = useNotificationCache ?
                            FontManager::getOrCreateNotificationGlyph(currCharacter, monospace, fontSize) :
                            FontManager::getOrCreateGlyph(currCharacter, monospace, fontSize);
                        
                        if (!glyph) continue;
                        
                        maxY = std::max(maxY, currY + lineHeight);
                        
                        // Render if needed
                        if (draw && glyph->glyphBmp && currCharacter > 32) {
                            renderGlyph(glyph.get(), currX, currY, *currentColor, useNotificationCache);
                        }
                        
                        currX += static_cast<s32>(glyph->xAdvance * glyph->currFontSize);
                    }
                } else {
                    // UTF-8 path with special symbols support
                    auto itStr = text->cbegin();
                    const auto itStrEnd = text->cend();
                    
                    while (itStr != itStrEnd && currX < maxWidthLimit) {
                        // Check for special symbols first
                        bool symbolProcessed = false;
                        
                        if (specialSymbols) {
                            const size_t remainingLength = itStrEnd - itStr;
                            
                            for (const auto& symbol : *specialSymbols) {
                                if (remainingLength >= symbol.length() &&
                                    std::equal(symbol.begin(), symbol.end(), itStr)) {
                                    
                                    // Process special symbol
                                    for (size_t i = 0; i < symbol.length(); ) {
                                        u32 symChar;
                                        const ssize_t symWidth = decode_utf8(&symChar, 
                                            reinterpret_cast<const u8*>(&symbol[i]));
                                        if (symWidth <= 0) break;
                                        
                                        if (symChar == '\n') {
                                            maxX = std::max(currX, maxX);
                                            currX = x;
                                            currY += lineHeight;
                                            maxY = std::max(maxY, currY + lineHeight);
                                        } else {
                                            auto glyph = FontManager::getOrCreateGlyph(symChar, monospace, fontSize);
                                            if (glyph) {
                                                maxY = std::max(maxY, currY + lineHeight);
                                                
                                                if (draw && glyph->glyphBmp && symChar > 32) {
                                                    renderGlyph(glyph.get(), currX, currY, *highlightColor, useNotificationCache);
                                                }
                                                currX += static_cast<s32>(glyph->xAdvance * glyph->currFontSize);
                                            }
                                        }
                                        i += symWidth;
                                    }
                                    itStr += symbol.length();
                                    symbolProcessed = true;
                                    break;
                                }
                            }
                        }
                        
                        if (symbolProcessed) continue;
                        
                        // Decode character
                        u32 currCharacter;
                        ssize_t codepointWidth;
                        
                        if (isAsciiOnly) {
                            currCharacter = static_cast<u32>(*itStr);
                            codepointWidth = 1;
                        } else {
                            codepointWidth = decode_utf8(&currCharacter, reinterpret_cast<const u8*>(&(*itStr)));
                            if (codepointWidth <= 0) break;
                        }
                        
                        itStr += codepointWidth;
                        
                        // Handle highlighting
                        if (highlightingEnabled) {
                            if (currCharacter == highlightStartChar) {
                                inHighlight = true;
                                currentColor = &defaultColor;
                            } else if (currCharacter == highlightEndChar) {
                                inHighlight = false;
                                currentColor = &defaultColor;
                            } else {
                                currentColor = inHighlight ? highlightColor : &defaultColor;
                            }
                        }
                        
                        // Handle newline
                        if (currCharacter == '\n') {
                            maxX = std::max(currX, maxX);
                            currX = x;
                            currY += lineHeight;
                            maxY = std::max(maxY, currY + lineHeight);
                            continue;
                        }
                        
                        // Get glyph
                        auto glyph = FontManager::getOrCreateGlyph(currCharacter, monospace, fontSize);
                        if (!glyph) continue;
                        
                        maxY = std::max(maxY, currY + lineHeight);
                        
                        // Render if needed
                        if (draw && glyph->glyphBmp && currCharacter > 32) {
                            renderGlyph(glyph.get(), currX, currY, *currentColor, useNotificationCache);
                        }
                        
                        currX += static_cast<s32>(glyph->xAdvance * glyph->currFontSize);
                    }
                }
                
                maxX = std::max(currX, maxX);
                return {maxX - x, maxY - y};
            }

            inline std::pair<s32, s32> drawNotificationString(const std::string& text, bool monospace, 
                                                              const s32 x, const s32 y, const u32 fontSize, 
                                                              const Color& defaultColor, const ssize_t maxWidth = 0, 
                                                              bool draw = true,
                                                              const Color* highlightColor = nullptr,
                                                              const std::vector<std::string>* specialSymbols = nullptr,
                                                              const u32 highlightStartChar = 0,
                                                              const u32 highlightEndChar = 0) {
                return drawString(text, monospace, x, y, fontSize, defaultColor, maxWidth, draw, 
                                 highlightColor, specialSymbols, highlightStartChar, highlightEndChar, true);
            }
            
            // Convenience wrappers for backward compatibility
            inline std::pair<s32, s32> drawStringWithHighlight(const std::string& text, bool monospace, 
                                                              s32 x, s32 y, const u32 fontSize, 
                                                              const Color& defaultColor, 
                                                              const Color& specialColor, 
                                                              const ssize_t maxWidth = 0,
                                                              const u32 startChar = '(',
                                                              const u32 endChar = ')') {
                return drawString(text, monospace, x, y, fontSize, defaultColor, maxWidth, true, &specialColor, nullptr, startChar, endChar);
            }
            
            inline std::pair<s32, s32> drawStringWithColoredSections(const std::string& text, bool monospace,
                                                    const std::vector<std::string>& specialSymbols, 
                                                    s32 x, const s32 y, const u32 fontSize, 
                                                    const Color& defaultColor, 
                                                    const Color& specialColor) {
                return drawString(text, monospace, x, y, fontSize, defaultColor, 0, true, &specialColor, &specialSymbols);
            }
            
            // Calculate string dimensions without drawing
            inline std::pair<s32, s32> getTextDimensions(const std::string& text, bool monospace, 
                                                         const u32 fontSize, const ssize_t maxWidth = 0) {
                return drawString(text, monospace, 0, 0, fontSize, Color{0,0,0,0}, maxWidth, false);
            }

            inline std::pair<s32, s32> getNotificationTextDimensions(const std::string& text, bool monospace, 
                                                                     const u32 fontSize, const ssize_t maxWidth = 0) {
                return drawString(text, monospace, 0, 0, fontSize, Color{0,0,0,0}, maxWidth, false, 
                                 nullptr, nullptr, 0, 0, true);
            }
            
            // Thread-safe limitStringLength using the unified cache
            inline std::string limitStringLength(const std::string& originalString, const bool monospace, 
                                               const u32 fontSize, const s32 maxLength) {  // Changed fontSize to u32
                
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
                
                if (text.size() < 2) return text;
                
                // Get ellipsis width using shared cache (now thread-safe)
                static constexpr u32 ellipsisChar = 0x2026;
                std::shared_ptr<FontManager::Glyph> ellipsisGlyph = FontManager::getOrCreateGlyph(ellipsisChar, monospace, fontSize);
                if (!ellipsisGlyph) return text;
                
                // Fixed: Use consistent s32 calculation like other functions
                const s32 ellipsisWidth = static_cast<s32>(ellipsisGlyph->xAdvance * ellipsisGlyph->currFontSize);
                const s32 maxWidthWithoutEllipsis = maxLength - ellipsisWidth;
                
                if (maxWidthWithoutEllipsis <= 0) {
                    return "…"; // If there's no room for text, just return ellipsis
                }
                
                // Calculate width incrementally
                s32 currX = 0;
                auto itStr = text.cbegin();
                const auto itStrEnd = text.cend();
                auto lastValidPos = itStr;
                
                // Fast ASCII check
                bool isAsciiOnly = true;
                for (unsigned char c : text) {
                    if (c > 127) {
                        isAsciiOnly = false;
                        break;
                    }
                }
                
                // Move variable declarations outside the loop
                u32 currCharacter;
                ssize_t codepointWidth;
                s32 charWidth;
                size_t bytePos;

                while (itStr != itStrEnd) {
                    // Decode UTF-8 codepoint
                    if (isAsciiOnly) {
                        currCharacter = static_cast<u32>(*itStr);
                        codepointWidth = 1;
                    } else {
                        codepointWidth = decode_utf8(&currCharacter, reinterpret_cast<const u8*>(&(*itStr)));
                        if (codepointWidth <= 0) break;
                    }
                    
                    // FontManager::getOrCreateGlyph is now thread-safe
                    std::shared_ptr<FontManager::Glyph> glyph = FontManager::getOrCreateGlyph(currCharacter, monospace, fontSize);
                    if (!glyph) {
                        itStr += codepointWidth;
                        continue;
                    }
                    
                    // Fixed: Use consistent s32 calculation
                    charWidth = static_cast<s32>(glyph->xAdvance * glyph->currFontSize);
                    
                    if (currX + charWidth > maxWidthWithoutEllipsis) {
                        // Calculate the byte position for substring
                        bytePos = std::distance(text.cbegin(), lastValidPos);
                        return text.substr(0, bytePos) + "…";
                    }
                    
                    currX += charWidth;
                    itStr += codepointWidth;
                    lastValidPos = itStr;
                }
                
                return text;
            }

            inline void setLayerPos(u32 x, u32 y) {
                // Guard against placing the layer off-screen.
                // Use cfg::LayerWidth/Height (the actual VI layer dimensions) rather than
                // a hardcoded 1.5× factor so this works correctly in both 720p-scaled and
                // 1080p pixel-perfect windowed modes.
                if (x > cfg::ScreenWidth  - cfg::LayerWidth ||
                    y > cfg::ScreenHeight - cfg::LayerHeight) {
                    return;
                }
                setLayerPosImpl(x, y);
            }

            void updateLayerSize() {
                const auto [horizontalUnderscanPixels, verticalUnderscanPixels] = getUnderscanPixels();
                
                // Recalculate layer dimensions with new underscan values
                //cfg::LayerWidth  = cfg::ScreenWidth * (float(cfg::FramebufferWidth) / float(cfg::LayerMaxWidth));
                //cfg::LayerHeight = cfg::ScreenHeight * (float(cfg::FramebufferHeight) / float(cfg::LayerMaxHeight));

                {
                    const float divW = ult::windowedLayerPixelPerfect ? float(cfg::ScreenWidth)  : float(cfg::LayerMaxWidth);
                    const float divH = ult::windowedLayerPixelPerfect ? float(cfg::ScreenHeight) : float(cfg::LayerMaxHeight);
                    cfg::LayerWidth  = cfg::ScreenWidth  * (float(cfg::FramebufferWidth)  / divW);
                    cfg::LayerHeight = cfg::ScreenHeight * (float(cfg::FramebufferHeight) / divH);
                }
                
                // Apply underscan adjustments.
                // Skipped in 1080p pixel-perfect mode to keep layer == framebuffer exactly.
                if (!ult::windowedLayerPixelPerfect) {
                    if (ult::DefaultFramebufferWidth == 1280 && ult::DefaultFramebufferHeight == 28) {
                        cfg::LayerHeight += cfg::ScreenHeight/720. * verticalUnderscanPixels;
                    } else if (ult::correctFrameSize) {
                        cfg::LayerWidth += horizontalUnderscanPixels;
                    } else if (horizontalUnderscanPixels > 0) {
                        // General case: any non-standard FB size (e.g. windowed GB).
                        // Scale the correction proportionally to the fraction of the
                        // full 1280-logical-space width this layer occupies.
                        cfg::LayerWidth += static_cast<int>(
                            horizontalUnderscanPixels *
                            (float(cfg::FramebufferWidth) / float(cfg::LayerMaxWidth)) + 0.5f);
                    }
                }
                
                // Update position if using right alignment
                if (ult::useRightAlignment && ult::correctFrameSize) {
                    ult::layerEdge = (1280 - 448);
                }
                
                // Update the existing layer with new dimensions
                viSetLayerSize(&this->m_layer, cfg::LayerWidth, cfg::LayerHeight);
                
                // Update position if using right alignment
                if (ult::useRightAlignment && ult::correctFrameSize) {
                    viSetLayerPosition(&this->m_layer, 1280-32 - horizontalUnderscanPixels, 0);
                    viSetLayerSize(&this->m_layer, cfg::LayerWidth, cfg::LayerHeight);
                    viSetLayerPosition(&this->m_layer, 1280-32 - horizontalUnderscanPixels, 0);
                }
                // ADD THIS: Update position for micro mode bottom positioning
                else if (ult::DefaultFramebufferWidth == 1280 && ult::DefaultFramebufferHeight == 28 && cfg::LayerPosY > 500) {
                    // Only adjust if already positioned at bottom (LayerPosY > 500 indicates bottom positioning)
                    const u32 targetY = !verticalUnderscanPixels ? 1038 : 1038- (cfg::ScreenHeight/720. * verticalUnderscanPixels) +0.5;
                    viSetLayerPosition(&this->m_layer, 0, targetY);
                    viSetLayerSize(&this->m_layer, cfg::LayerWidth, cfg::LayerHeight);
                    viSetLayerPosition(&this->m_layer, 0, targetY);
                }
            }

            inline void setLayerPosImpl(u32 x, u32 y) {

                // Simply set the position to what was requested - no automatic right alignment
                cfg::LayerPosX = x;
                cfg::LayerPosY = y;
                
                ASSERT_FATAL(viSetLayerPosition(&this->m_layer, cfg::LayerPosX, cfg::LayerPosY));
            }


        #if USING_WIDGET_DIRECTIVE
            // Method to draw clock, temperatures, and battery percentage
            inline bool drawWidget() {
                static time_t lastTimeUpdate = 0;
                static char timeStr[20];
                static char PCB_temperatureStr[10];
                static char SOC_temperatureStr[10];
                static char chargeString[6];
                static time_t lastSensorUpdate = 0;
                
                const bool showAnyWidget = !(ult::hideBattery && ult::hidePCBTemp && ult::hideSOCTemp && ult::hideClock);
                
                // Draw separator and backdrop if showing any widget
                if (showAnyWidget) {
                    drawRect(239, 15 + 2 - 2, 1, 64 + 2, aWithOpacity(topSeparatorColor));
                    if (!ult::hideWidgetBackdrop) {
                        drawUniformRoundedRect(
                            247, 15 + 2 - 2,
                            (ult::extendedWidgetBackdrop
                                ? tsl::cfg::FramebufferWidth - 255
                                : tsl::cfg::FramebufferWidth - 215),
                            64 + 2, a(widgetBackdropColor)
                        );
                    }
                }
                
                // Calculate base Y offset
                size_t y_offset = ((ult::hideBattery && ult::hidePCBTemp && ult::hideSOCTemp) || ult::hideClock)
                                  ? (55 + 2 - 1)
                                  : (44 + 2 - 1);
                
                // Constants for centering calculations
                const int backdropCenterX = 247 + ((tsl::cfg::FramebufferWidth - 255) >> 1);
                
                time_t currentTime = time(nullptr);
                
                // Draw clock
                if (!ult::hideClock) {
                    if (currentTime != lastTimeUpdate || ult::languageWasChanged.load(std::memory_order_acquire)) {
                        strftime(timeStr, sizeof(timeStr), ult::datetimeFormat.c_str(), localtime(&currentTime));
                        ult::localizeTimeStr(timeStr);
                        lastTimeUpdate = currentTime;
                    }
                    
                    const int timeWidth = getTextDimensions(timeStr, false, 20).first;
                    
                    if (ult::centerWidgetAlignment) {
                        // Centered alignment
                        drawString(timeStr, false, backdropCenterX - (timeWidth >> 1), y_offset, 20, clockColor);
                    } else {
                        // Right alignment
                        drawString(timeStr, false, tsl::cfg::FramebufferWidth - timeWidth - 25, y_offset, 20, clockColor);
                    }
                    
                    y_offset += 22;
                }
                
                // Update sensor data every second
                if ((currentTime - lastSensorUpdate) >= 1) {
                    if (!ult::hideSOCTemp) {
                        float socTemp = 0.0f;
                        ult::ReadSocTemperature(&socTemp);
                        ult::SOC_temperature.store(socTemp, std::memory_order_release);
                        snprintf(
                            SOC_temperatureStr, sizeof(SOC_temperatureStr),
                            "%d°C",
                            static_cast<int>(round(ult::SOC_temperature.load(std::memory_order_acquire)))
                        );
                    }
                    
                    if (!ult::hidePCBTemp) {
                        float pcbTemp = 0.0f;
                        ult::ReadPcbTemperature(&pcbTemp);
                        ult::PCB_temperature.store(pcbTemp, std::memory_order_release);
                        snprintf(
                            PCB_temperatureStr, sizeof(PCB_temperatureStr),
                            "%d°C",
                            static_cast<int>(round(ult::PCB_temperature.load(std::memory_order_acquire)))
                        );
                    }
                    
                    if (!ult::hideBattery) {
                        uint32_t bc = 0;
                        bool charging = false;
                        ult::powerGetDetails(&bc, &charging);
                        bc = std::min(bc, 100U);
                        ult::batteryCharge.store(bc, std::memory_order_release);
                        ult::isCharging.store(charging, std::memory_order_release);
                        snprintf(chargeString, sizeof(chargeString), "%u%%", bc);
                    }
                    
                    lastSensorUpdate = currentTime;
                }
                
                if (ult::centerWidgetAlignment) {
                    // CENTERED ALIGNMENT
                    int totalWidth = 0;
                    int socWidth = 0, pcbWidth = 0, chargeWidth = 0;
                    bool hasMultiple = false;
                    
                    const float socTemp = ult::SOC_temperature.load(std::memory_order_acquire);
                    const float pcbTemp = ult::PCB_temperature.load(std::memory_order_acquire);
                    const uint32_t batteryCharge = ult::batteryCharge.load(std::memory_order_acquire);
                    const bool charging = ult::isCharging.load(std::memory_order_acquire);
                    
                    if (!ult::hideSOCTemp && socTemp > 0.0f) {
                        socWidth = getTextDimensions(SOC_temperatureStr, false, 20).first;
                        totalWidth += socWidth;
                        hasMultiple = true;
                    }
                    if (!ult::hidePCBTemp && pcbTemp > 0.0f) {
                        pcbWidth = getTextDimensions(PCB_temperatureStr, false, 20).first;
                        if (hasMultiple) totalWidth += 5;
                        totalWidth += pcbWidth;
                        hasMultiple = true;
                    }
                    if (!ult::hideBattery && batteryCharge > 0) {
                        chargeWidth = getTextDimensions(chargeString, false, 20).first;
                        if (hasMultiple) totalWidth += 5;
                        totalWidth += chargeWidth;
                    }
                    
                    int currentX = backdropCenterX - (totalWidth >> 1);
                    if (socWidth > 0) {
                        drawString(
                            SOC_temperatureStr, false, currentX, y_offset, 20,
                            ult::dynamicWidgetColors
                                ? tsl::GradientColor(socTemp)
                                : temperatureColor
                        );
                        currentX += socWidth + 5;
                    }
                    if (pcbWidth > 0) {
                        drawString(
                            PCB_temperatureStr, false, currentX, y_offset, 20,
                            ult::dynamicWidgetColors
                                ? tsl::GradientColor(pcbTemp)
                                : temperatureColor
                        );
                        currentX += pcbWidth + 5;
                    }
                    if (chargeWidth > 0) {
                        const Color batteryColorToUse = charging
                            ? batteryChargingColor
                            : (batteryCharge < 20 ? batteryLowColor : batteryColor);
                        drawString(chargeString, false, currentX, y_offset, 20, batteryColorToUse);
                    }
                    
                } else {
                    // RIGHT ALIGNMENT
                    int chargeWidth = 0, pcbWidth = 0, socWidth = 0;
                    const float pcbTemp = ult::PCB_temperature.load(std::memory_order_acquire);
                    const float socTemp = ult::SOC_temperature.load(std::memory_order_acquire);
                    const uint32_t batteryCharge = ult::batteryCharge.load(std::memory_order_acquire);
                    const bool charging = ult::isCharging.load(std::memory_order_acquire);
                    
                    if (!ult::hideBattery && batteryCharge > 0) {
                        const Color batteryColorToUse = charging
                            ? batteryChargingColor
                            : (batteryCharge < 20 ? batteryLowColor : batteryColor);
                        chargeWidth = getTextDimensions(chargeString, false, 20).first;
                        drawString(
                            chargeString, false,
                            tsl::cfg::FramebufferWidth - chargeWidth - 25,
                            y_offset, 20, batteryColorToUse
                        );
                    }
                    
                    int offset = 0;
                    if (!ult::hidePCBTemp && pcbTemp > 0.0f) {
                        if (!ult::hideBattery) offset -= 5;
                        pcbWidth = getTextDimensions(PCB_temperatureStr, false, 20).first;
                        drawString(
                            PCB_temperatureStr, false,
                            tsl::cfg::FramebufferWidth + offset - pcbWidth - chargeWidth - 25,
                            y_offset, 20,
                            ult::dynamicWidgetColors
                                ? tsl::GradientColor(pcbTemp)
                                : defaultTextColor
                        );
                    }
                    if (!ult::hideSOCTemp && socTemp > 0.0f) {
                        if (!ult::hidePCBTemp || !ult::hideBattery) offset -= 5;
                        socWidth = getTextDimensions(SOC_temperatureStr, false, 20).first;
                        drawString(
                            SOC_temperatureStr, false,
                            tsl::cfg::FramebufferWidth + offset - socWidth - pcbWidth - chargeWidth - 25,
                            y_offset, 20,
                            ult::dynamicWidgetColors
                                ? tsl::GradientColor(socTemp)
                                : defaultTextColor
                        );
                    }
                }
                return showAnyWidget;
            }
        #endif
            
            // Optimized glyph rendering
            // -----------------------------------------------------------------------
            // NEON-vectorised glyph renderer. Scalar prologue/epilogue + 8-wide NEON
            // blend loop with fast paths for all-transparent and all-opaque runs.
            inline void renderGlyph(const FontManager::Glyph* glyph,
                                    float x, float y,
                                    const Color& color,
                                    bool skipAlphaLimit = false) {

                if (!glyph->glyphBmp || color.a == 0) [[unlikely]] return;

                const s32 xPos = static_cast<s32>(x) + glyph->bounds[0];
                const s32 yPos = static_cast<s32>(y) + glyph->bounds[1];
                const s32 glyphWidth  = glyph->width;
                const s32 glyphHeight = glyph->height;

                if (xPos >= static_cast<s32>(cfg::FramebufferWidth)  ||
                    yPos >= static_cast<s32>(cfg::FramebufferHeight)  ||
                    xPos + glyphWidth  <= 0 ||
                    yPos + glyphHeight <= 0) [[unlikely]] return;

                s32 startX = std::max(0, -xPos);
                s32 startY = std::max(0, -yPos);
                s32 endX   = std::min(glyphWidth,  static_cast<s32>(cfg::FramebufferWidth)  - xPos);
                s32 endY   = std::min(glyphHeight, static_cast<s32>(cfg::FramebufferHeight) - yPos);

                if (this->m_scissorDepth != 0) [[unlikely]] {
                    const auto& sc = this->m_scissorStack[this->m_scissorDepth - 1];
                    startX = std::max(startX, static_cast<s32>(sc.x)     - xPos);
                    startY = std::max(startY, static_cast<s32>(sc.y)     - yPos);
                    endX   = std::min(endX,   static_cast<s32>(sc.x_max) - xPos);
                    endY   = std::min(endY,   static_cast<s32>(sc.y_max) - yPos);
                    if (endX <= startX || endY <= startY) return;
                }

                const s32 spanW      = endX - startX;
                const u8  alphaLimit = skipAlphaLimit ? color.a
                                                      : static_cast<u8>(0xF * Renderer::s_opacity);

                u16* const fb16 = reinterpret_cast<u16*>(static_cast<Color*>(this->m_currentFramebuffer));

                constexpr s32 kMaxSpan = 128;
                u32 colOff[kMaxSpan];
                const s32  safeSpan   = std::min(spanW, kMaxSpan);
                const u32  basePixelX = static_cast<u32>(xPos + startX);
                for (s32 i = 0; i < safeSpan; ++i) {
                    const u32 px = basePixelX + static_cast<u32>(i);
                    colOff[i] = ((px >> 5u) << 12u) | ((px & 16u) << 3u)
                              | ((px & 8u)  <<  1u) |  (px & 7u);
                }

                const uint8x8_t  vLimitU8  = vdup_n_u8(alphaLimit);
                const uint8x8_t  vZeroU8   = vdup_n_u8(0u);
                const uint8x8_t  vFullU8   = vdup_n_u8(0xFu);
                const uint16x8_t vColorFull = vdupq_n_u16(color.rgba);   // 8 × full colour
                const uint16x8_t vMask4    = vdupq_n_u16(0x000Fu);       // low-nibble mask
                const uint16x8_t v15       = vdupq_n_u16(15u);
                const uint16x8_t vDstR     = vdupq_n_u16(color.r);       // constant glyph R
                const uint16x8_t vDstG     = vdupq_n_u16(color.g);       // constant glyph G
                const uint16x8_t vDstB     = vdupq_n_u16(color.b);       // constant glyph B

                const s32 prologueLen = static_cast<s32>((8u - (basePixelX & 7u)) & 7u);

                const uint8_t* bmpRow = glyph->glyphBmp + startY * glyphWidth + startX;

                for (s32 bmpY = startY; bmpY < endY; ++bmpY, bmpRow += glyphWidth) {

                    // rowBase(y) — computed once per scanline, never per pixel.
                    const u32 py = static_cast<u32>(yPos + bmpY);
                    const u32 rowBase =
                        ((((py & 127u) >> 4u) + ((py >> 7u) * offsetWidthVar)) << 9u)
                        | ((py & 8u) << 5u) | ((py & 6u) << 4u) | ((py & 1u) << 3u);

                    s32 i = 0;

                    // ── Scalar pixel blender (prologue + epilogue share this) ───────
                    auto blendScalar = [&](s32 lim) __attribute__((always_inline)) {
                        for (; i < lim; ++i) {
                            u8 alpha = bmpRow[i] >> 4u;
                            if (alpha == 0u) [[unlikely]] continue;
                            if (alpha > alphaLimit) alpha = alphaLimit;
                            const u32 off = rowBase + colOff[i];
                            if (alpha == 0xFu) [[likely]] {
                                fb16[off] = color.rgba;
                            } else {
                                const Color src = reinterpret_cast<const Color*>(fb16)[off];
                                reinterpret_cast<Color*>(fb16)[off] = Color(
                                    blendColor(src.r, color.r, alpha),
                                    blendColor(src.g, color.g, alpha),
                                    blendColor(src.b, color.b, alpha),
                                    alpha + static_cast<u8>((src.a * (0xFu - alpha)) >> 4u));
                            }
                        }
                    };
                    blendScalar(std::min(prologueLen, safeSpan));  // prologue

                    // ── NEON main loop — 8 pixels per iteration ────────────────────
                    while (i + 8 <= safeSpan) {

                        // Load 8 bitmap bytes and extract 4-bit alphas (0..15).
                        uint8x8_t bmp8   = vld1_u8(bmpRow + i);
                        uint8x8_t alpha8 = vshr_n_u8(bmp8, 4);
                        alpha8            = vmin_u8(alpha8, vLimitU8);  // clamp to alphaLimit

                        // ── Fast path A: all 8 pixels transparent ──────────────────
                        // Reinterpret the 8-byte alpha register as a u64 and compare
                        // to zero — no horizontal-min instruction needed.
                        if (vget_lane_u64(vreinterpret_u64_u8(alpha8), 0) == 0u) {
                            i += 8; continue;
                        }

                        // ── Fast path B: all 8 pixels fully opaque ─────────────────
                        // vceq_u8 produces 0xFF per lane where alpha==0xF.  If all 8
                        // lanes are 0xFF the u64 view is all-ones.  No fb read needed.
                        const uint8x8_t eqFull = vceq_u8(alpha8, vFullU8);
                        if (vget_lane_u64(vreinterpret_u64_u8(eqFull), 0)
                                == 0xFFFFFFFFFFFFFFFFull) {
                            vst1q_u16(fb16 + rowBase + colOff[i], vColorFull);
                            i += 8; continue;
                        }

                        // ── Mixed path: partial alpha, full NEON blend ─────────────
                        //
                        // Load 8 existing framebuffer colors (contiguous u16 block).
                        uint16x8_t src16 = vld1q_u16(fb16 + rowBase + colOff[i]);

                        // Expand alpha / inv-alpha to u16 for multiply.
                        const uint16x8_t alpha16 = vmovl_u8(alpha8);
                        const uint16x8_t inva16  = vsubq_u16(v15, alpha16); // 15 - alpha

                        // Unpack src channels from packed RGBA4444.
                        const uint16x8_t src_r = vandq_u16(src16, vMask4);
                        const uint16x8_t src_g = vandq_u16(vshrq_n_u16(src16,  4), vMask4);
                        const uint16x8_t src_b = vandq_u16(vshrq_n_u16(src16,  8), vMask4);
                        const uint16x8_t src_a =           vshrq_n_u16(src16, 12);

                        // out_ch = (src_ch*(15-α) + dst_ch*α) >> 4
                        // vmlaq_u16: multiply-accumulate, one fewer vaddq per channel.
                        const uint16x8_t r_out = vshrq_n_u16(
                            vmlaq_u16(vmulq_u16(src_r, inva16), vDstR, alpha16), 4);
                        const uint16x8_t g_out = vshrq_n_u16(
                            vmlaq_u16(vmulq_u16(src_g, inva16), vDstG, alpha16), 4);
                        const uint16x8_t b_out = vshrq_n_u16(
                            vmlaq_u16(vmulq_u16(src_b, inva16), vDstB, alpha16), 4);
                        // out_a = α + (src_a*(15-α)) >> 4
                        const uint16x8_t a_out = vaddq_u16(alpha16,
                            vshrq_n_u16(vmulq_u16(src_a, inva16), 4));

                        // Repack to RGBA4444.
                        uint16x8_t blended = vorrq_u16(r_out,
                            vorrq_u16(vshlq_n_u16(g_out,  4),
                            vorrq_u16(vshlq_n_u16(b_out,  8),
                                      vshlq_n_u16(a_out, 12))));

                        // Correction passes — sign-extend u8 mask (0xFF→0xFFFF, 0x00→0x0000)
                        // so vbslq_u16 selects ALL 16 bits correctly, not just the low byte.
                        //   α==0xF → must equal vColorFull exactly (not blend rounding)
                        //   α==0   → must preserve src (not ~0.9375*src from blend)
                        const uint16x8_t fullMask16 =
                            vreinterpretq_u16_s16(vmovl_s8(vreinterpret_s8_u8(eqFull)));
                        blended = vbslq_u16(fullMask16, vColorFull, blended);

                        const uint16x8_t zeroMask16 =
                            vreinterpretq_u16_s16(vmovl_s8(vreinterpret_s8_u8(vceq_u8(alpha8, vZeroU8))));
                        blended = vbslq_u16(zeroMask16, src16, blended);

                        vst1q_u16(fb16 + rowBase + colOff[i], blended);
                        i += 8;
                    }

                    blendScalar(safeSpan);  // epilogue
                }
            }

            // Legacy shared_ptr overload — call-sites that pass a shared_ptr directly
            // still work; passing .get() to the raw-pointer overload is preferred.
            inline void renderGlyph(const std::shared_ptr<FontManager::Glyph>& glyph,
                                    float x, float y, const Color& color,
                                    bool skipAlphaLimit = false) {
                renderGlyph(glyph.get(), x, y, color, skipAlphaLimit);
            }
            

            /**
             * @brief Adds the layer from screenshot and recording stacks
             */
            inline void addScreenshotStacks(bool forceDisable = true) {
                tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_Screenshot);
                tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_Recording);
                tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_LastFrame);
                screenshotsAreDisabled.store(false, std::memory_order_release);
                if (forceDisable)
                    screenshotsAreForceDisabled.store(false, std::memory_order_release);
            }

            /**
             * @brief Removes the layer from screenshot and recording stacks
             */
            inline void removeScreenshotStacks(bool forceDisable = true) {
                tsl::hlp::viRemoveFromLayerStack(&this->m_layer, ViLayerStack_Screenshot);
                tsl::hlp::viRemoveFromLayerStack(&this->m_layer, ViLayerStack_Recording);
                tsl::hlp::viRemoveFromLayerStack(&this->m_layer, ViLayerStack_LastFrame);
                screenshotsAreDisabled.store(true, std::memory_order_release);
                if (forceDisable)
                    screenshotsAreForceDisabled.store(true, std::memory_order_release);
            }

            /**
             * @brief Get the current framebuffer address
             *
             * @return Framebuffer address
             */
            inline void* getCurrentFramebuffer() {
                return this->m_currentFramebuffer;
            }
            

        private:
            Renderer() {}
            
            /**
             * @brief Sets the opacity of the layer
             *
             * @param opacity Opacity
             */
            inline static void setOpacity(float opacity) {
                opacity = std::clamp(opacity, 0.0F, 1.0F);
                
                Renderer::s_opacity = opacity;
            }
            
            bool m_initialized = false;
            ViDisplay m_display;
            ViLayer m_layer;
            Event m_vsyncEvent;
            
            NWindow m_window;
            Framebuffer m_framebuffer;
            void *m_currentFramebuffer = nullptr;
            
            // Inline scissor stack — replaces std::stack<std::deque> to eliminate heap
            // allocation and pointer indirection on every drawing operation.
            // Depth never exceeds a handful of levels in practice; 8 is a safe ceiling.
            ScissoringConfig m_scissorStack[8];
            s32              m_scissorDepth = 0;
            
            
            
            /**
             * @brief Get the next framebuffer address
             *
             * @return Next framebuffer address
             */
            inline void* getNextFramebuffer() {
                return static_cast<u8*>(this->m_framebuffer.buf) + this->getNextFramebufferSlot() * this->m_framebuffer.fb_size;
            }
            
            /**
             * @brief Get the framebuffer size
             *
             * @return Framebuffer size
             */
            inline size_t getFramebufferSize() {
                return this->m_framebuffer.fb_size;
            }
            
            /**
             * @brief Get the number of framebuffers in use
             *
             * @return Number of framebuffers
             */
            inline size_t getFramebufferCount() {
                return this->m_framebuffer.num_fbs;
            }
            
            /**
             * @brief Get the currently used framebuffer's slot
             *
             * @return Slot
             */
            inline u8 getCurrentFramebufferSlot() {
                return this->m_window.cur_slot;
            }
            
            /**
             * @brief Get the next framebuffer's slot
             *
             * @return Next slot
             */
            inline u8 getNextFramebufferSlot() {
                return (this->m_window.cur_slot + 1) % this->m_framebuffer.num_fbs;
            }
            
            /**
             * @brief Waits for the vsync event
             *
             */
            inline void waitForVSync() {
                eventWait(&this->m_vsyncEvent, UINT64_MAX);
            }
            
            /**
             * @brief Decodes a x and y coordinate into a offset into the swizzled framebuffer
             *
             * @param x X pos
             * @param y Y Pos
             * @return Offset
             */

            inline u32 __attribute__((always_inline)) getPixelOffset(const u32 x, const u32 y) {
                // Check for scissoring boundaries
                if (this->m_scissorDepth != 0) [[unlikely]] {
                    const auto& currScissorConfig = this->m_scissorStack[this->m_scissorDepth - 1];
                    if (x < currScissorConfig.x || y < currScissorConfig.y || 
                        x >= currScissorConfig.x_max || 
                        y >= currScissorConfig.y_max) {
                        return UINT32_MAX;
                    }
                }
                
                return ((((y & 127) >> 4) + ((x >> 5) << 3) + ((y >> 7) * offsetWidthVar)) << 9) +
                       ((y & 8) << 5) + ((x & 16) << 3) + ((y & 6) << 4) + 
                       ((x & 8) << 1) + ((y & 1) << 3) + (x & 7);
            }

            
            /**
             * @brief Initializes the renderer and layers
             *
             */
            void init() {
                // Get the underscan pixel values for both horizontal and vertical borders
                const auto [horizontalUnderscanPixels, verticalUnderscanPixels] = getUnderscanPixels();
                
                ult::useRightAlignment = (ult::parseValueFromIniSection(ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME, "right_alignment") == ult::TRUE_STR);

                cfg::LayerPosX = 0;
                cfg::LayerPosY = 0;
                cfg::FramebufferWidth  = ult::DefaultFramebufferWidth;
                cfg::FramebufferHeight = ult::DefaultFramebufferHeight;

                offsetWidthVar = (((cfg::FramebufferWidth / 2) >> 4) << 3);

                ult::correctFrameSize = (cfg::FramebufferWidth == 448 && cfg::FramebufferHeight == 720); // for detecting the correct Overlay display size
                if (ult::useRightAlignment && ult::correctFrameSize) {
                    cfg::LayerPosX = 1280-32 - horizontalUnderscanPixels;
                    ult::layerEdge = (1280-448);
                }

                //cfg::LayerWidth  = cfg::ScreenWidth * (float(cfg::FramebufferWidth) / float(cfg::LayerMaxWidth));
                //cfg::LayerHeight = cfg::ScreenHeight * (float(cfg::FramebufferHeight) / float(cfg::LayerMaxHeight));

                {
                    const float divW = ult::windowedLayerPixelPerfect ? float(cfg::ScreenWidth)  : float(cfg::LayerMaxWidth);
                    const float divH = ult::windowedLayerPixelPerfect ? float(cfg::ScreenHeight) : float(cfg::LayerMaxHeight);
                    cfg::LayerWidth  = cfg::ScreenWidth  * (float(cfg::FramebufferWidth)  / divW);
                    cfg::LayerHeight = cfg::ScreenHeight * (float(cfg::FramebufferHeight) / divH);
                }

                // Apply underscanning offset.
                // Skipped entirely in 1080p pixel-perfect mode: the layer equals the
                // framebuffer exactly (1:1), so adding underscan pixels would break that
                // mapping and make cfg::LayerWidth/Height incorrect for bounds calculation.
                if (!ult::windowedLayerPixelPerfect) {
                    if (ult::DefaultFramebufferWidth == 1280 && ult::DefaultFramebufferHeight == 28) // for status monitor micro mode
                        cfg::LayerHeight += cfg::ScreenHeight/720. *verticalUnderscanPixels;
                    else if (ult::correctFrameSize)
                        cfg::LayerWidth += horizontalUnderscanPixels;
                    else if (horizontalUnderscanPixels > 0)
                        cfg::LayerWidth += static_cast<int>(
                            horizontalUnderscanPixels *
                            (float(cfg::FramebufferWidth) / float(cfg::LayerMaxWidth)) + 0.5f);
                }
                
                if (this->m_initialized)
                    return;
                
                tsl::hlp::doWithSmSession([this, horizontalUnderscanPixels]{

                    ASSERT_FATAL(viInitialize(ViServiceType_Manager));
                    ASSERT_FATAL(viOpenDefaultDisplay(&this->m_display));
                    ASSERT_FATAL(viGetDisplayVsyncEvent(&this->m_display, &this->m_vsyncEvent));
                    ASSERT_FATAL(viCreateManagedLayer(&this->m_display, static_cast<ViLayerFlags>(0), 0, &__nx_vi_layer_id));
                    ASSERT_FATAL(viCreateLayer(&this->m_display, &this->m_layer));
                    ASSERT_FATAL(viSetLayerScalingMode(&this->m_layer, ViScalingMode_FitToLayer));

                    if (horizontalUnderscanPixels == 0) {
                        s32 layerZ = 0;
                        if (R_SUCCEEDED(viGetZOrderCountMax(&this->m_display, &layerZ)) && layerZ > 0) {
                            ASSERT_FATAL(viSetLayerZ(&this->m_layer, layerZ));
                        }
                        else {
                            ASSERT_FATAL(viSetLayerZ(&this->m_layer, 255)); // max value 255 as fallback
                        }
                    } else {
                        ASSERT_FATAL(viSetLayerZ(&this->m_layer, 34)); // 34 is the edge for underscanning
                    }

                    ASSERT_FATAL(tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_Default));
                    ASSERT_FATAL(tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_Screenshot));
                    ASSERT_FATAL(tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_Recording));
                    ASSERT_FATAL(tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_Arbitrary));
                    ASSERT_FATAL(tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_LastFrame));
                    ASSERT_FATAL(tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_Null));
                    ASSERT_FATAL(tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_ApplicationForDebug));
                    ASSERT_FATAL(tsl::hlp::viAddToLayerStack(&this->m_layer, ViLayerStack_Lcd));
                    
                    ASSERT_FATAL(viSetLayerSize(&this->m_layer, cfg::LayerWidth, cfg::LayerHeight));
                    ASSERT_FATAL(viSetLayerPosition(&this->m_layer, cfg::LayerPosX, cfg::LayerPosY));
                    ASSERT_FATAL(nwindowCreateFromLayer(&this->m_window, &this->m_layer));
                    ASSERT_FATAL(framebufferCreate(&this->m_framebuffer, &this->m_window, cfg::FramebufferWidth, cfg::FramebufferHeight, PIXEL_FORMAT_RGBA_4444, 2));
                    ASSERT_FATAL(setInitialize());
                    ASSERT_FATAL(this->initFonts());
                    setExit();
                });
                
                this->m_initialized = true;
            }
            
            /**
             * @brief Exits the renderer and layer
             *
             */
            void exit() {
                if (!this->m_initialized)
                    return;
                
                // Cleanup shared font manager
                FontManager::cleanup();

                framebufferClose(&this->m_framebuffer);
                nwindowClose(&this->m_window);
                viDestroyManagedLayer(&this->m_layer);
                viCloseDisplay(&this->m_display);
                eventClose(&this->m_vsyncEvent);
                viExit();
            }
            
            /**
             * @brief Initializes Nintendo's shared fonts. Default and Extended
             *
             * @return Result
             */
            Result initFonts() {
                PlFontData stdFontData, localFontData, extFontData;
                
                // Nintendo's default font
                TSL_R_TRY(plGetSharedFontByType(&stdFontData, PlSharedFontType_Standard));
                
                u8 *fontBuffer = reinterpret_cast<u8*>(stdFontData.address);
                stbtt_InitFont(&this->m_stdFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
                
                u64 languageCode;
                if (R_SUCCEEDED(setGetSystemLanguage(&languageCode))) {
                    // Check if need localization font
                    SetLanguage setLanguage;
                    TSL_R_TRY(setMakeLanguage(languageCode, &setLanguage));
                    this->m_hasLocalFont = true;
                    switch (setLanguage) {
                    case SetLanguage_ZHCN:
                    case SetLanguage_ZHHANS:
                        TSL_R_TRY(plGetSharedFontByType(&localFontData, PlSharedFontType_ChineseSimplified));
                        break;
                    case SetLanguage_ZHTW:
                    case SetLanguage_ZHHANT:
                        TSL_R_TRY(plGetSharedFontByType(&localFontData, PlSharedFontType_ChineseTraditional));
                        break;
                    case SetLanguage_KO:
                        TSL_R_TRY(plGetSharedFontByType(&localFontData, PlSharedFontType_KO));
                        break;
                    default:
                        this->m_hasLocalFont = false;
                        break;
                    }
                    
                    if (this->m_hasLocalFont) {
                        fontBuffer = reinterpret_cast<u8*>(localFontData.address);
                        stbtt_InitFont(&this->m_localFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
                    }
                }
                
                // Nintendo's extended font containing a bunch of icons
                TSL_R_TRY(plGetSharedFontByType(&extFontData, PlSharedFontType_NintendoExt));
                
                fontBuffer = reinterpret_cast<u8*>(extFontData.address);
                stbtt_InitFont(&this->m_extFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
                
                // Load all three local fonts unconditionally for fallback support
                PlFontData cnFontData, twFontData, koFontData;
                
                TSL_R_TRY(plGetSharedFontByType(&cnFontData, PlSharedFontType_ChineseSimplified));
                fontBuffer = reinterpret_cast<u8*>(cnFontData.address);
                stbtt_InitFont(&this->m_localFontCN, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
                
                TSL_R_TRY(plGetSharedFontByType(&twFontData, PlSharedFontType_ChineseTraditional));
                fontBuffer = reinterpret_cast<u8*>(twFontData.address);
                stbtt_InitFont(&this->m_localFontTW, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
                
                TSL_R_TRY(plGetSharedFontByType(&koFontData, PlSharedFontType_KO));
                fontBuffer = reinterpret_cast<u8*>(koFontData.address);
                stbtt_InitFont(&this->m_localFontKO, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
                
                // Initialize the shared font manager
                FontManager::initializeFonts(&this->m_stdFont, 
                                           &this->m_localFont,
                                           &this->m_localFontCN,
                                           &this->m_localFontTW, 
                                           &this->m_localFontKO,
                                           &this->m_extFont,
                                           this->m_hasLocalFont);
                
                return 0;
            }
            

            /**
             * @brief Start a new frame
             * @warning Don't call this more than once before calling \ref endFrame
             */
            inline void startFrame() {
                this->m_currentFramebuffer = framebufferBegin(&this->m_framebuffer, nullptr);
            }

            inline void endFrame() {
            #if IS_STATUS_MONITOR_DIRECTIVE
                if (isRendering) {
                    static u32 lastFPS = 0;
                    static u64 cachedIntervalNs = 1000000000ULL / 60;
                    
                    u32 fps = TeslaFPS;
                    if (__builtin_expect(fps != lastFPS, 0)) {
                        cachedIntervalNs = (fps > 0) ? (1000000000ULL / fps) : cachedIntervalNs;
                        lastFPS = fps;
                    }
                    
                    // Just wait - touch thread will signal if needed
                    leventWait(&renderingStopEvent, cachedIntervalNs);
                }
            #endif
            
                this->waitForVSync();
                framebufferEnd(&this->m_framebuffer);
                this->m_currentFramebuffer = nullptr;
            
                if (tsl::clearGlyphCacheNow.exchange(false, std::memory_order_acq_rel)) {
                    tsl::gfx::FontManager::clearCache();
                }
            }
            

        };

        static std::pair<int, int> getUnderscanPixels() {
            if (!ult::consoleIsDocked()) {
                return {0, 0};
            }
            
            // Retrieve the TV settings
            SetSysTvSettings tvSettings;
            Result res = setsysGetTvSettings(&tvSettings);
            if (R_FAILED(res)) {
                // Handle error: return default underscan or log error
                return {0, 0};
            }
            
            // The underscan value might not be a percentage, we need to interpret it correctly
            const u32 underscanValue = tvSettings.underscan;
            
            // Convert the underscan value to a fraction. Assuming 0 means no underscan and larger values represent
            // greater underscan. Adjust this formula based on actual observed behavior or documentation.
            const float underscanPercentage = 1.0f - (underscanValue / 100.0f);
            
            // Original dimensions of the full 720p image (1280x720)
            const float originalWidth = 1280;
            const float originalHeight = 720;
            
            // Adjust the width and height based on the underscan percentage
            const float adjustedWidth = (originalWidth * underscanPercentage);
            const float adjustedHeight = (originalHeight * underscanPercentage);
            
            // Calculate the underscan in pixels (left/right and top/bottom)
            const int horizontalUnderscanPixels = (originalWidth - adjustedWidth);
            const int verticalUnderscanPixels = (originalHeight - adjustedHeight);
            
            return {horizontalUnderscanPixels, verticalUnderscanPixels};
        }

        TESLA_OPT_SPEED_POP
    }


    
    // Elements
    
    // ── Shared frame-drawing helpers ──────────────────────────────────────────
    // Extracted from virtual draw() overrides in OverlayFrame and
    // HeaderOverlayFrame. Because these are virtual methods on distinct classes,
    // LTO/ICF cannot merge their bodies even when the logic is identical — the
    // compiler must emit one copy per class. Free functions with [[gnu::noinline]]
    // ensure a single copy in the binary regardless of how many classes call them.

    // Conditional atomic-float store: only writes if the value changed.
    // Replaces the updateAtomic lambda (defined fresh per draw() call in
    // IS_LAUNCHER frame) and the manual if-check pattern in the other frames.
    [[gnu::noinline]] inline void updateAtomicFloat(std::atomic<float>& atom, float val) {
        if (val != atom.load(std::memory_order_acquire))
            atom.store(val, std::memory_order_release);
    }

    // Draws the 1-px vertical edge separator on whichever side useRightAlignment
    // selects. Appeared verbatim in 3 different frame draw() overrides.
    [[gnu::noinline]] inline void drawEdgeSeparator(gfx::Renderer* renderer) {
        if (!ult::useRightAlignment)
            renderer->drawRect(447, 0, 448, 720, renderer->a(edgeSeparatorColor));
        else
            renderer->drawRect(0, 0, 1, 720, renderer->a(edgeSeparatorColor));
    }

    // Measures the footer button widths, updates the three shared atomics, and
    // draws the Back/Select touch-highlight rectangles. Used by OverlayFrame and
    // both HeaderOverlayFrame variants; the IS_LAUNCHER variant passes its own
    // label strings so interpreter-mode text is handled at the call site.
    //
    // After this call the caller can read ult::backWidth / ult::halfGap for
    // subsequent text-positioning without holding local copies.
    //
    // noClickableItems suppresses the Select rect (OverlayFrame behaviour);
    // pass false when the class has no such guard (HeaderOverlayFrame).
    [[gnu::noinline]] inline void updateFooterButtonWidths(
            gfx::Renderer*     renderer,
            const std::string& backLabel,
            const std::string& selectLabel,
            bool               noClickableItems) {
        static constexpr float kButtonStartX = 30.0f;
        const float gapWidth  = renderer->getTextDimensions(ult::GAP_1, false, 23).first;
        const float halfGap   = gapWidth * 0.5f;
        const float backW     = renderer->getTextDimensions(backLabel,   false, 23).first + gapWidth;
        const float selW      = renderer->getTextDimensions(selectLabel, false, 23).first + gapWidth;
        updateAtomicFloat(ult::halfGap,    halfGap);
        updateAtomicFloat(ult::backWidth,  backW);
        updateAtomicFloat(ult::selectWidth, selW);
        const float buttonY = static_cast<float>(cfg::FramebufferHeight - 73 + 1);
        if (ult::touchingBack.load(std::memory_order_acquire))
            renderer->drawRoundedRect(kButtonStartX + 2 - halfGap, buttonY, backW - 1, 73.0f, 12.0f, renderer->a(clickColor));
        if (ult::touchingSelect.load(std::memory_order_acquire) && !noClickableItems)
            renderer->drawRoundedRect(kButtonStartX + 2 - halfGap + backW + 1, buttonY, selW - 2, 73.0f, 12.0f, renderer->a(clickColor));
    }
    // ──────────────────────────────────────────────────────────────────────────

    namespace elm {
        
        enum class TouchEvent {
            Touch,
            Hold,
            Scroll,
            Release,
            None
        };
        
        /**
         * @brief The top level Element of the libtesla UI library
         * @note When creating your own elements, extend from this or one of it's sub classes
         */
        class Element {
        public:
            
            Element() {}
            virtual ~Element() {
                m_clickListener = {};   // frees captures immediately
            }
            
            bool m_isTable = false;  // Default to false for non-table elements
            bool m_isItem = true;
            

            u64 t_ns;  // Changed from chrono::duration to nanoseconds
            u8 saturation;
            float progress;
            
            s32 x, y;
            s32 amplitude;
            u64 m_animationStartTime; // Changed from chrono::time_point to nanoseconds
            
            virtual bool isTable() const {
                return m_isTable;
            }

            virtual bool isItem() const {
                return m_isItem;
            }

            /**
             * @brief Handles focus requesting
             * @note This function should return the element to focus.
             *       When this element should be focused, return `this`.
             *       When one of it's child should be focused, return `this->child->requestFocus(oldFocus, direction)`
             *       When this element is not focusable, return `nullptr`
             *
             * @param oldFocus Previously focused element
             * @param direction Direction in which focus moved. \ref FocusDirection::None is passed for the initial load
             * @return Element to focus
             */
            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) {
                return nullptr;
            }
            
            /**
             * @brief Function called when a joycon button got pressed
             *
             * @param keys Keys pressed in the last frame
             * @return true when button press has been consumed
             * @return false when button press should be passed on to the parent
             */
            virtual bool onClick(u64 keys) {
                return m_clickListener(keys);
            }
            
            /**
             * @brief Called once per frame with the latest HID inputs
             *
             * @param keysDown Buttons pressed in the last frame
             * @param keysHeld Buttons held down longer than one frame
             * @param touchInput Last touch position
             * @param leftJoyStick Left joystick position
             * @param rightJoyStick Right joystick position
             * @return Weather or not the input has been consumed
             */
            virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
                return false;
            }
            
            /**
             * @brief Function called when the element got touched
             * @todo Not yet implemented
             *
             * @param x X pos
             * @param y Y pos
             * @return true when touch input has been consumed
             * @return false when touch input should be passed on to the parent
             */
            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) {
                return false;
            }
            
            /**
             * @brief Called once per frame to draw the element
             * @warning Do not call this yourself. Use \ref Element::frame(gfx::Renderer *renderer)
             *
             * @param renderer Renderer
             */
            virtual void draw(gfx::Renderer *renderer) = 0;
            
            /**
             * @brief Called when the underlying Gui gets created and after calling \ref Gui::invalidate() to calculate positions and boundaries of the element
             * @warning Do not call this yourself. Use \ref Element::invalidate()
             *
             * @param parentX Parent X pos
             * @param parentY Parent Y pos
             * @param parentWidth Parent Width
             * @param parentHeight Parent Height
             */
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) = 0;
            
            /**
             * @brief Draws highlighting and the element itself
             * @note When drawing children of a element in \ref Element::draw(gfx::Renderer *renderer), use `this->child->frame(renderer)` instead of calling draw directly
             *
             * @param renderer
             */
            void inline frame(gfx::Renderer *renderer) {
                
                if (this->m_focused) {
                    renderer->enableScissoring(0, ult::activeHeaderHeight, tsl::cfg::FramebufferWidth, tsl::cfg::FramebufferHeight-73-ult::activeHeaderHeight);
                    this->drawFocusBackground(renderer);
                    this->drawHighlight(renderer);
                    renderer->disableScissoring();
                }
                
                this->draw(renderer);
            }
            
            /**
             * @brief Forces a layout recreation of a element
             *
             */
            void inline invalidate() {
                const auto& parent = this->getParent();
                
                if (parent == nullptr)
                    this->layout(0, 0, cfg::FramebufferWidth, cfg::FramebufferHeight);
                else
                    this->layout(ELEMENT_BOUNDS(parent));
            }
            
            /**
             * @brief Shake the highlight in the given direction to signal that the focus cannot move there
             *
             * @param direction Direction to shake highlight in
             */
            void inline shakeHighlight(FocusDirection direction) {
                this->m_highlightShaking = true;
                this->m_highlightShakingDirection = direction;
                this->m_highlightShakingStartTime = ult::nowNs(); // Changed
                if (direction != FocusDirection::None && m_isItem) {
                    triggerWallFeedback();
                }
            }
            
            /**
             * @brief Triggers the blue click animation to signal a element has been clicked on
             *
             */
            void inline triggerClickAnimation() {
                this->m_clickAnimationProgress = tsl::style::ListItemHighlightLength;
                this->m_animationStartTime = ult::nowNs(); // Changed
            }


            
            /**
             * @brief Resets the click animation progress, canceling the animation
             */
            void inline resetClickAnimation() {
                this->m_clickAnimationProgress = 0;
            }
            
            /**
             * @brief Draws the blue highlight animation when clicking on a button
             * @note Override this if you have a element that e.g requires a non-rectangular animation or a different color
             *
             * @param renderer Renderer
             */
            virtual void drawClickAnimation(gfx::Renderer *renderer) {
                if (!m_isItem)
                    return;
                if (ult::useSelectionBG) {
                    renderer->drawRectAdaptive(this->getX() + x + 4, this->getY() + y, this->getWidth() - 8, this->getHeight(), aWithOpacity(selectionBGColor));
                }
            
                saturation = tsl::style::ListItemHighlightSaturation * (float(this->m_clickAnimationProgress) / float(tsl::style::ListItemHighlightLength));
            
                Color animColor = {0xF,0xF,0xF,0xF};
                if (invertBGClickColor) {
                    const u8 inverted = 15-saturation;
                    animColor = {inverted, inverted, inverted, selectionBGColor.a};
                } else {
                    animColor = {saturation, saturation, saturation, selectionBGColor.a};
                }
                renderer->drawRectAdaptive(ELEMENT_BOUNDS(this), aWithOpacity(animColor));
            
                // Cache time calculation - only compute once
                static u64 lastTimeUpdate = 0;
                static double cachedProgress = 0.0;
                const u64 currentTime_ns = ult::nowNs();
                
                // Only recalculate progress if enough time has passed (reduce computation frequency)
                if (currentTime_ns - lastTimeUpdate > 16666666) { // ~60 FPS update rate
                    cachedProgress = (ult::cos(2.0 * ult::_M_PI * std::fmod(currentTime_ns / 1000000000.0 - 0.25, 1.0)) + 1.0) / 2.0;
                    lastTimeUpdate = currentTime_ns;
                }
                progress = cachedProgress;
                
                Color clickColor1 = highlightColor1;
                Color clickColor2 = clickColor;
                
                if (progress >= 0.5) {
                    clickColor1 = clickColor;
                    clickColor2 = highlightColor2;
                }
                
                // Combine color interpolation into single calculation
                s_highlightColor = lerpColor(clickColor1, clickColor2, progress);
                
                x = 0;
                y = 0;
                if (this->m_highlightShaking) {
                    t_ns = currentTime_ns - this->m_highlightShakingStartTime;
                    const double t_ms = t_ns / 1000000.0;
                    
                    static constexpr double SHAKE_DURATION_MS = 200.0;
                    
                    if (t_ms >= SHAKE_DURATION_MS)
                        this->m_highlightShaking = false;
                    else {
                        // Generate random amplitude only once per shake using the start time as seed
                        const double amplitude = 6.0 + ((this->m_highlightShakingStartTime / 1000000) % 5);
                        const double progress = t_ms / SHAKE_DURATION_MS; // 0 to 1
                        
                        // Lighter damping so both bounces are visible
                        const double damping = 1.0 / (1.0 + 2.5 * progress * (1.0 + 1.3 * progress));
                        
                        // 2 full oscillations = 2 clear bounces
                        const double oscillation = ult::cos(ult::_M_PI * 4.0 * progress);
                        const double displacement = amplitude * oscillation * damping;
                        const int offset = static_cast<int>(displacement);
                        
                        switch (this->m_highlightShakingDirection) {
                            case FocusDirection::Up:    y = -offset; break;
                            case FocusDirection::Down:  y = offset; break;
                            case FocusDirection::Left:  x = -offset; break;
                            case FocusDirection::Right: x = offset; break;
                            default: break;
                        }
                    }
                }
                
                renderer->drawBorderedRoundedRect(this->getX() + x, this->getY() + y, this->getWidth() +4, this->getHeight(), 5, 5, a(s_highlightColor));
            }
            
            /**
             * @brief Draws the back background when a element is highlighted
             * @note Override this if you have a element that e.g requires a non-rectangular focus
             *
             * @param renderer Renderer
             */
            virtual void drawFocusBackground(gfx::Renderer *renderer) {
                if (this->m_clickAnimationProgress > 0) {
                    this->drawClickAnimation(renderer);
            
                    // Direct calculation without intermediate multiplication
                    this->m_clickAnimationProgress = tsl::style::ListItemHighlightLength * (1.0f - ((ult::nowNs() - this->m_animationStartTime) * 0.000001) * 0.002f); // 0.002f = 1/500
            
                    // Clamp to 0 in one operation
                    if (this->m_clickAnimationProgress < 0) {
                        this->m_clickAnimationProgress = 0;
                    }
                }
            }
            
            /**
             * @brief Draws the blue boarder when a element is highlighted
             * @note Override this if you have a element that e.g requires a non-rectangular focus
             *
             * @param renderer Renderer
             */
            virtual void drawHighlight(gfx::Renderer *renderer) {
                if (!m_isItem)
                    return;
                
                // Use cached time calculation from drawClickAnimation if possible
                static u64 lastHighlightUpdate = 0;
                static double cachedHighlightProgress = 0.0;
                const u64 currentTime_ns = ult::nowNs();
                
                // Update progress at 60 FPS rate with high-precision calculation
                if (currentTime_ns - lastHighlightUpdate > 16666666) {
                    
                    // Match original calculation exactly but with higher precision
                    cachedHighlightProgress = (ult::cos(2.0 * ult::_M_PI * std::fmod(currentTime_ns * 0.000000001 - 0.25, 1.0)) + 1.0) * 0.5;
                    
                    lastHighlightUpdate = currentTime_ns;
                }
                progress = cachedHighlightProgress;
            
                // Cache the interpreter state check result to avoid atomic load overhead
                static bool lastInterpreterState = false;
                static u64 lastInterpreterCheck = 0;
                if (currentTime_ns - lastInterpreterCheck > 50000000) { // Check every 50ms
                    lastInterpreterState = ult::runningInterpreter.load(std::memory_order_acquire);
                    lastInterpreterCheck = currentTime_ns;
                }
            
                if (lastInterpreterState) {
                    // High precision floating point color interpolation for interpreter colors
                    s_highlightColor = {
                        static_cast<u8>(highlightColor4.r + (highlightColor3.r - highlightColor4.r) * progress + 0.5),
                        static_cast<u8>(highlightColor4.g + (highlightColor3.g - highlightColor4.g) * progress + 0.5),
                        static_cast<u8>(highlightColor4.b + (highlightColor3.b - highlightColor4.b) * progress + 0.5),
                        0xF
                    };
                } else {
                    // High precision floating point color interpolation for normal colors
                    s_highlightColor = {
                        static_cast<u8>(highlightColor2.r + (highlightColor1.r - highlightColor2.r) * progress + 0.5),
                        static_cast<u8>(highlightColor2.g + (highlightColor1.g - highlightColor2.g) * progress + 0.5),
                        static_cast<u8>(highlightColor2.b + (highlightColor1.b - highlightColor2.b) * progress + 0.5),
                        0xF
                    };
                }
                
                x = 0;
                y = 0;
                
                if (this->m_highlightShaking) {
                    t_ns = currentTime_ns - this->m_highlightShakingStartTime;
                    const double t_ms = t_ns / 1000000.0;
                    
                    static constexpr double SHAKE_DURATION_MS = 200.0;
                    
                    if (t_ms >= SHAKE_DURATION_MS)
                        this->m_highlightShaking = false;
                    else {
                        // Generate random amplitude only once per shake using the start time as seed
                        const double amplitude = 6.0 + ((this->m_highlightShakingStartTime / 1000000) % 5);
                        const double progress = t_ms / SHAKE_DURATION_MS; // 0 to 1
                        
                        // Lighter damping so both bounces are visible
                        const double damping = 1.0 / (1.0 + 2.5 * progress * (1.0 + 1.3 * progress));
                        
                        // 2 full oscillations = 2 clear bounces
                        const double oscillation = ult::cos(ult::_M_PI * 4.0 * progress);
                        const double displacement = amplitude * oscillation * damping;
                        const int offset = static_cast<int>(displacement);
                        
                        switch (this->m_highlightShakingDirection) {
                            case FocusDirection::Up:    y = -offset; break;
                            case FocusDirection::Down:  y = offset; break;
                            case FocusDirection::Left:  x = -offset; break;
                            case FocusDirection::Right: x = offset; break;
                            default: break;
                        }
                    }
                }
                
                if (this->m_clickAnimationProgress == 0) {
                    if (ult::useSelectionBG) {
                        renderer->drawRectAdaptive(this->getX() + x + 4, this->getY() + y, this->getWidth() - 12 +4, this->getHeight(), aWithOpacity(selectionBGColor));
                    }
            
                    #if IS_LAUNCHER_DIRECTIVE
                    // Determine the active percentage to use
                    const float activePercentage = ult::displayPercentage.load(std::memory_order_acquire);
                    if (activePercentage > 0){
                        renderer->drawRectAdaptive(this->getX() + x + 4, this->getY() + y, (this->getWidth()- 12 +4)*(activePercentage * 0.01f), this->getHeight(), aWithOpacity(progressColor));
                    }
                    #endif
            
                    renderer->drawBorderedRoundedRect(this->getX() + x, this->getY() + y, this->getWidth() +4, this->getHeight(), 5, 5, a(s_highlightColor));
                }
                
                ult::onTrackBar.store(false, std::memory_order_release);
            }
            
            
            
            /**
             * @brief Sets the boundaries of this view
             *
             * @param x Start X pos
             * @param y Start Y pos
             * @param width Width
             * @param height Height
             */
            inline void setBoundaries(s32 x, s32 y, s32 width, s32 height) {
                this->m_x = x;
                this->m_y = y;
                this->m_width = width;
                this->m_height = height;
            }
            
            /**
             * @brief Adds a click listener to the element
             *
             * @param clickListener Click listener called with keys that were pressed last frame. Callback should return true if keys got consumed
             */
            virtual void setClickListener(std::function<bool(u64 keys)> clickListener) {
                this->m_clickListener = clickListener;
            }
            
            /**
             * @brief Gets the element's X position
             *
             * @return X position
             */
            inline s32 getX() { return this->m_x; }
            /**
             * @brief Gets the element's Y position
             *
             * @return Y position
             */
            inline s32 getY() { return this->m_y; }
            /**
             * @brief Gets the element's Width
             *
             * @return Width
             */
            inline s32 getWidth() { return this->m_width;  }
            /**
             * @brief Gets the element's Height
             *
             * @return Height
             */
            inline s32 getHeight() { return this->m_height; }
            
            inline s32 getTopBound() { return this->getY(); }
            inline s32 getLeftBound() { return this->getX(); }
            inline s32 getRightBound() { return this->getX() + this->getWidth(); }
            inline s32 getBottomBound() { return this->getY() + this->getHeight(); }
            
            /**
             * @brief Check if the coordinates are in the elements bounds
             *
             * @return true if coordinates are in bounds, false otherwise
             */
            bool inBounds(s32 touchX, s32 touchY) {
                return touchX >= this->getLeftBound() + int(ult::layerEdge) && touchX <= this->getRightBound() + int(ult::layerEdge) && touchY >= this->getTopBound() && touchY <= this->getBottomBound();
            }
            
            /**
             * @brief Sets the element's parent
             * @note This is required to handle focus and button downpassing properly
             *
             * @param parent Parent
             */
            inline void setParent(Element *parent) { this->m_parent = parent; }
            
            /**
             * @brief Get the element's parent
             *
             * @return Parent
             */
            inline Element* getParent() { return this->m_parent; }
            

            virtual std::vector<Element*> getChildren() const {
                return {}; // Return empty vector for simplicity
            }

            /**
             * @brief Marks this element as focused or unfocused to draw the highlight
             *
             * @param focused Focused
             */
            virtual void setFocused(bool focused) {
                this->m_focused = focused;
                this->m_clickAnimationProgress = 0;
            }

            inline bool hasFocus() {
                return this->m_focused;
            }

            virtual bool matchesJumpCriteria(const std::string& jumpText, const std::string& jumpValue, bool contains) const {
                return false; // Default implementation for non-ListItem elements
            }
            
            
            static InputMode getInputMode() { return Element::s_inputMode; }
            
            static void setInputMode(InputMode mode) { Element::s_inputMode = mode; }
            
        protected:
            constexpr static inline auto a = &gfx::Renderer::a;
            constexpr static inline auto aWithOpacity = &gfx::Renderer::aWithOpacity;
            bool m_focused = false;
            u8 m_clickAnimationProgress = 0;
            
            // Highlight shake animation
            bool m_highlightShaking = false;
            u64 m_highlightShakingStartTime; // Changed from chrono::time_point to nanoseconds
            FocusDirection m_highlightShakingDirection;
            
            static inline InputMode s_inputMode;
            
        private:
            friend class Gui;
            
            s32 m_x = 0, m_y = 0, m_width = 0, m_height = 0;
            Element *m_parent = nullptr;
            std::vector<Element*> m_children;
            std::function<bool(u64 keys)> m_clickListener = [](u64) { return false; };
        };

        /**
         * @brief A Element that exposes the renderer directly to draw custom views easily
         */
        class CustomDrawer : public Element {
        public:
            /**
             * @brief Constructor
             * @note This element should only be used to draw static things the user cannot interact with e.g info text, images, etc.
             *
             * @param renderFunc Callback that will be called once every frame to draw this view
             */
            CustomDrawer(std::function<void(gfx::Renderer* r, s32 x, s32 y, s32 w, s32 h)> renderFunc) : Element(), m_renderFunc(renderFunc) {
                m_isItem = false;
                m_isTable = true;
            }

            virtual ~CustomDrawer() {}
            
            virtual void draw(gfx::Renderer* renderer) override {
                this->m_renderFunc(renderer, ELEMENT_BOUNDS(this));
            }
            
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                
            }
            
        private:
            std::function<void(gfx::Renderer*, s32 x, s32 y, s32 w, s32 h)> m_renderFunc;
        };

        /**
         * @brief A Element that exposes the renderer directly to draw custom views easily
         */
        class TableDrawer : public Element {
        public:
            TableDrawer(std::function<void(gfx::Renderer* r, s32 x, s32 y, s32 w, s32 h)> renderFunc, bool _hideTableBackground, size_t _endGap, bool _isScrollable = false)
                : Element(), m_renderFunc(renderFunc), hideTableBackground(_hideTableBackground), endGap(_endGap), isScrollable(_isScrollable) {
                    m_isTable = isScrollable;  // Mark this element as a table
                    m_isItem = false;
                }
            
            virtual ~TableDrawer() {}

            virtual void draw(gfx::Renderer* renderer) override {

                renderer->enableScissoring(0, 88, tsl::cfg::FramebufferWidth, tsl::cfg::FramebufferHeight - 73 - 97 +2+5);
                
                if (!hideTableBackground)
                    renderer->drawRoundedRect(this->getX() + 4+2, this->getY()-4-1, this->getWidth() +2 + 1, this->getHeight() + 20 - endGap+2, 12.0, aWithOpacity(tableBGColor));
                
                m_renderFunc(renderer, this->getX() + 4, this->getY(), this->getWidth() + 4, this->getHeight());
                
                renderer->disableScissoring();
            }
            
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {}


            virtual bool onClick(u64 keys) {
                return false;
            }
            
            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) override {
                return nullptr;
            }
        
        private:
            std::function<void(gfx::Renderer*, s32 x, s32 y, s32 w, s32 h)> m_renderFunc;
            bool hideTableBackground = false;
            size_t endGap = 3;
            bool isScrollable = false;
        };


        //#if IS_LAUNCHER_DIRECTIVE
        // Simple utility function to draw the dynamic "Ultra" part of the logo
        static s32 drawDynamicUltraText(gfx::Renderer* renderer, s32 startX, s32 y, u32 fontSize, 
                                       const tsl::Color& staticColor, bool useNotificationMethod = false) {
            static constexpr double cycleDuration = 1.6;
            s32 currentX = startX;
            
            if (ult::useDynamicLogo) {
                const u64 currentTime_ns = ult::nowNs();
                const double currentTimeCount = static_cast<double>(currentTime_ns) / 1000000000.0;
                const double timeBase = std::fmod(currentTimeCount, cycleDuration);
                const double waveScale = 2.0 * ult::_M_PI / cycleDuration;
                static constexpr double phaseShift = ult::_M_PI / 2.0;
                
                float countOffset = 0;
                for (const char letter : ult::SPLIT_PROJECT_NAME_1) {
                    const double wavePhase = waveScale * (timeBase + static_cast<double>(countOffset));
                    const double rawProgress = ult::cos(wavePhase - phaseShift);
                    
                    const double normalizedProgress = (rawProgress + 1.0) * 0.5;
                    const double smoothedProgress = normalizedProgress * normalizedProgress * (3.0 - 2.0 * normalizedProgress);
                    const double ultraSmoothProgress = smoothedProgress * smoothedProgress * (3.0 - 2.0 * smoothedProgress);
                    
                    const double blend = std::max(0.0, std::min(1.0, ultraSmoothProgress));

                    const tsl::Color _highlightColor = lerpColor(dynamicLogoRGB2, dynamicLogoRGB1, blend);
                    
                    const std::string letterStr(1, letter);
                    if (useNotificationMethod) {
                        currentX += renderer->drawNotificationString(letterStr, false, currentX, y, fontSize, _highlightColor).first;
                    } else {
                        currentX += renderer->drawString(letterStr, false, currentX, y, fontSize, _highlightColor).first;
                    }
                    countOffset -= static_cast<float>(cycleDuration / 8.0);
                }
            } else {
                // Static rendering
                for (const char letter : ult::SPLIT_PROJECT_NAME_1) {
                    const std::string letterStr(1, letter);
                    if (useNotificationMethod) {
                        currentX += renderer->drawNotificationString(letterStr, false, currentX, y, fontSize, staticColor).first;
                    } else {
                        currentX += renderer->drawString(letterStr, false, currentX, y, fontSize, staticColor).first;
                    }
                }
            }
            
            return currentX;
        }
        
        // Utility function to calculate width of the Ultra text (for notification centering)
        static s32 calculateUltraTextWidth(gfx::Renderer* renderer, u32 fontSize, bool useNotificationMethod = false) {
            s32 totalWidth = 0;
            
            if (ult::useDynamicLogo) {
                // Calculate width by measuring each character for dynamic rendering
                for (const char letter : ult::SPLIT_PROJECT_NAME_1) {
                    const std::string letterStr(1, letter);
                    if (useNotificationMethod) {
                        totalWidth += renderer->getNotificationTextDimensions(letterStr, false, fontSize).first;
                    } else {
                        totalWidth += renderer->getTextDimensions(letterStr, false, fontSize).first;
                    }
                }
            } else {
                // Static rendering - measure the whole string at once
                if (useNotificationMethod) {
                    totalWidth = renderer->getNotificationTextDimensions(ult::SPLIT_PROJECT_NAME_1, false, fontSize).first;
                } else {
                    totalWidth = renderer->getTextDimensions(ult::SPLIT_PROJECT_NAME_1, false, fontSize).first;
                }
            }
            
            return totalWidth;
        }

        //#endif
        
        /**
         * @brief The base frame which can contain another view
         *
         */
        class OverlayFrame : public Element {
        public:
            /**
             * @brief Constructor
             *
             * @param title Name of the Overlay drawn bolt at the top
             * @param subtitle Subtitle drawn bellow the title e.g version number
             */
            std::string m_title;
            std::string m_subtitle;
        
            bool m_noClickableItems;
        
        #if IS_LAUNCHER_DIRECTIVE
            std::string m_menuMode;
            std::string m_colorSelection;
            
            tsl::Color titleColor = {0xF,0xF,0xF,0xF};
            float letterWidth;
        #endif

            std::string m_pageLeftName;
            std::string m_pageRightName;
        
        #if USING_WIDGET_DIRECTIVE
            bool m_showWidget = false;
        #endif
        
            float x, y;
            int offset, y_offset;
            int fontSize;
        
        #if IS_LAUNCHER_DIRECTIVE
            OverlayFrame(const std::string& title, const std::string& subtitle, const bool& _noClickableItems=false, const std::string& menuMode = "", const std::string& colorSelection = "", const std::string& pageLeftName = "", const std::string& pageRightName = "")
                : Element(), m_title(title), m_subtitle(subtitle), m_noClickableItems(_noClickableItems), m_menuMode(menuMode), m_colorSelection(colorSelection), m_pageLeftName(pageLeftName), m_pageRightName(pageRightName) {
        #else
            OverlayFrame(const std::string& title, const std::string& subtitle, const bool& _noClickableItems=false, const std::string& pageLeftName = "", const std::string& pageRightName = "")
                : Element(), m_title(title), m_subtitle(subtitle), m_noClickableItems(_noClickableItems), m_pageLeftName(pageLeftName), m_pageRightName(pageRightName) {
        #endif
                    ult::activeHeaderHeight = 97;
                    ult::loadWallpaperFileWhenSafe();
                    m_isItem = false;
                    disableSound.store(false, std::memory_order_release);
                }
        
            ~OverlayFrame() {
                delete m_contentElement;
            }
        
        #if USING_FPS_INDICATOR_DIRECTIVE
            // Function to calculate FPS
            inline float updateFPS(double currentTimeCount) {
                static double lastUpdateTime = currentTimeCount;
                static int frameCount = 0;
                static float fps = 0.0f;
            
                ++frameCount;
                const double elapsedTime = currentTimeCount - lastUpdateTime;
            
                if (elapsedTime >= 1.0) { // Update FPS every second
                    fps = frameCount / static_cast<float>(elapsedTime);
                    lastUpdateTime = currentTimeCount;
                    frameCount = 0;
                }
                return fps;
            }
        #endif
            
            void draw(gfx::Renderer *renderer) override {
            
                if (ult::expandedMemory && !ult::refreshWallpaper.load(std::memory_order_acquire) &&
                    !ult::wallpaperData.empty() && ult::correctFrameSize)
                    renderer->drawWallpaper();
                else
                    renderer->fillScreen(a(defaultBackgroundColor));
                
                y = 50;
                offset = 0;
                
            #if IS_LAUNCHER_DIRECTIVE
                const bool interpreterIsRunningNow = ult::runningInterpreter.load(std::memory_order_acquire) &&
                    (ult::downloadPercentage.load(std::memory_order_acquire)  != -1 ||
                     ult::unzipPercentage.load(std::memory_order_acquire)     != -1 ||
                     ult::copyPercentage.load(std::memory_order_acquire)      != -1);
                
                if (m_noClickableItems != ult::noClickableItems.load(std::memory_order_acquire))
                    ult::noClickableItems.store(m_noClickableItems, std::memory_order_release);
            
                const bool renderIsUltrahandMenu = (m_title == ult::CAPITAL_ULTRAHAND_PROJECT_NAME && 
                                                     m_subtitle.find("Ultrahand Package") == std::string::npos && 
                                                     m_subtitle.find("Ultrahand Script")  == std::string::npos);
                
                bool widgetDrawn = false;
                if (renderIsUltrahandMenu) {
                #if USING_WIDGET_DIRECTIVE
                    widgetDrawn = renderer->drawWidget();
                #endif
                    if (ult::touchingMenu.load(std::memory_order_acquire) &&
                        (ult::inMainMenu.load(std::memory_order_acquire) ||
                         (ult::inHiddenMode.load(std::memory_order_acquire) &&
                          !ult::inSettingsMenu.load(std::memory_order_acquire) &&
                          !ult::inSubSettingsMenu.load(std::memory_order_acquire)))) {
                        renderer->drawRoundedRect(7.0f, 12.0f, 232.0f, 73.0f, 12.0f, a(clickColor));
                    }
                    x = 20; fontSize = 42; offset = 6;
                    if (ult::useDynamicLogo) {
                        x = drawDynamicUltraText(renderer, x, y + offset, fontSize, logoColor1, false);
                    } else {
                        for (const char letter : ult::SPLIT_PROJECT_NAME_1) {
                            const std::string letterStr(1, letter);
                            x += renderer->drawString(letterStr, false, x, y + offset, fontSize, logoColor1).first;
                        }
                    }
                    renderer->drawString(ult::SPLIT_PROJECT_NAME_2, false, x, y + offset, fontSize, logoColor2);
                } else {
                #if USING_WIDGET_DIRECTIVE
                    widgetDrawn = m_showWidget && renderer->drawWidget();
                #endif
                    x = 20; y = 50; fontSize = 32;
                    calcScrollWidth(renderer, titleScroll, m_title, 32, widgetDrawn);
                    const bool isScript = m_subtitle.find("Ultrahand Script") != std::string::npos;
                    drawScrollableText(renderer, titleScroll, isScript ? defaultScriptColor : getPackageColor(), x, y, 32, 27, 35);
                }
                
                {
                    std::string subtitle = m_subtitle;
                    const size_t pos = subtitle.find("?Ultrahand Script");
                    if (pos != std::string::npos) subtitle.erase(pos, 17);
                    calcScrollWidth(renderer, subScroll, subtitle, 15, widgetDrawn);
                    const int subtitleX = 20, subtitleY = y + 25;
                    if (m_title == ult::CAPITAL_ULTRAHAND_PROJECT_NAME) {
                        renderer->drawStringWithColoredSections(ult::versionLabel, false, tsl::s_dividerSpecialChars,
                                                               subtitleX, subtitleY, 15, bannerVersionTextColor, textSeparatorColor);
                    } else if (subScroll.trunc) {
                        if (!subScroll.active) { subScroll.active = true; subScroll.timeIn = ult::nowNs(); }
                        renderer->enableScissoring(subtitleX, subtitleY - 16, subScroll.maxW, 24);
                        renderer->drawStringWithColoredSections(subScroll.scrollText, false, tsl::s_dividerSpecialChars,
                            subtitleX - static_cast<s32>(subScroll.offset), subtitleY, 15, bannerVersionTextColor, textSeparatorColor);
                        renderer->disableScissoring();
                        updateScroll(subScroll);
                    } else {
                        renderer->drawStringWithColoredSections(subtitle, false, tsl::s_dividerSpecialChars,
                            subtitleX, subtitleY, 15, bannerVersionTextColor, textSeparatorColor);
                    }
                }
    
            #else
                if (m_noClickableItems != ult::noClickableItems.load(std::memory_order_acquire))
                    ult::noClickableItems.store(m_noClickableItems, std::memory_order_release);
                
                bool widgetDrawn = false;
                #if USING_WIDGET_DIRECTIVE
                widgetDrawn = m_showWidget && renderer->drawWidget();
                #endif
                calcScrollWidth(renderer, titleScroll, m_title, 32, widgetDrawn);
                drawScrollableText(renderer, titleScroll, defaultOverlayColor, 20, 50, 32, 27, 35);
                calcScrollWidth(renderer, subScroll, m_subtitle, 15, widgetDrawn);
                {
                    const int subtitleX = 20, subtitleY = y + 25;
                    if (subScroll.trunc) {
                        if (!subScroll.active) { subScroll.active = true; subScroll.timeIn = ult::nowNs(); }
                        renderer->enableScissoring(subtitleX, subtitleY - 16, subScroll.maxW, 24);
                        renderer->drawString(subScroll.scrollText, false,
                            subtitleX - static_cast<s32>(subScroll.offset), subtitleY, 15, bannerVersionTextColor);
                        renderer->disableScissoring();
                        updateScroll(subScroll);
                    } else {
                        renderer->drawString(m_subtitle, false, subtitleX, subtitleY, 15, bannerVersionTextColor);
                    }
                }
            #endif
            
                renderer->drawRect(15, tsl::cfg::FramebufferHeight - 73, tsl::cfg::FramebufferWidth - 30, 1, a(bottomSeparatorColor));
            
            #if IS_LAUNCHER_DIRECTIVE
                updateFooterButtonWidths(renderer,
                    "\uE0E1" + ult::GAP_2 + (!interpreterIsRunningNow ? ult::BACK   : ult::HIDE),
                    "\uE0E0" + ult::GAP_2 + (!interpreterIsRunningNow ? ult::OK     : ult::CANCEL),
                    m_noClickableItems);
            #else
                updateFooterButtonWidths(renderer,
                    "\uE0E1" + ult::GAP_2 + ult::BACK,
                    "\uE0E0" + ult::GAP_2 + ult::OK,
                    m_noClickableItems);
            #endif
                const float _halfGap   = ult::halfGap.load(std::memory_order_acquire);
                const float _backWidth = ult::backWidth.load(std::memory_order_acquire);
                const float _selectWidth = ult::selectWidth.load(std::memory_order_acquire);
                const float gapWidth = _halfGap * 2.0f;
                static constexpr float buttonStartX = 30;
                const float buttonY = static_cast<float>(cfg::FramebufferHeight - 73 + 1);
                
            #if IS_LAUNCHER_DIRECTIVE
                const bool hasNextPage = !interpreterIsRunningNow &&
                    ((ult::inMainMenu.load(std::memory_order_acquire) &&
                      ((m_menuMode == ult::OVERLAYS_STR) || (m_menuMode == ult::PACKAGES_STR))) ||
                     !m_pageLeftName.empty() || !m_pageRightName.empty());
                if (hasNextPage != ult::hasNextPageButton.load(std::memory_order_acquire))
                    ult::hasNextPageButton.store(hasNextPage, std::memory_order_release);
                if (hasNextPage) {
                    const float _nextPageWidth = renderer->getTextDimensions(
                        !m_pageLeftName.empty()  ? ("\uE0ED" + ult::GAP_2 + m_pageLeftName) :
                        !m_pageRightName.empty() ? ("\uE0EE" + ult::GAP_2 + m_pageRightName) :
                        (ult::inMainMenu.load(std::memory_order_acquire) ?
                            (((m_menuMode == "packages") ? (ult::usePageSwap ? "\uE0EE" : "\uE0ED") :
                                                           (ult::usePageSwap ? "\uE0ED" : "\uE0EE")) +
                             ult::GAP_2 + (ult::inOverlaysPage.load(std::memory_order_acquire) ? ult::PACKAGES : ult::OVERLAYS_ABBR)) : ""),
                        false, 23).first + gapWidth;
                    updateAtomicFloat(ult::nextPageWidth, _nextPageWidth);
                    if (ult::touchingNextPage.load(std::memory_order_acquire)) {
                        float nextX = buttonStartX+2 - _halfGap + _backWidth + 1;
                        if (!m_noClickableItems) nextX += _selectWidth;
                        renderer->drawRoundedRect(nextX, buttonY, _nextPageWidth-2, 73.0f, 12.0f, a(clickColor));
                    }
                }
            #else
                const bool hasNextPage = !m_pageLeftName.empty() || !m_pageRightName.empty();
                if (hasNextPage != ult::hasNextPageButton.load(std::memory_order_acquire))
                    ult::hasNextPageButton.store(hasNextPage, std::memory_order_release);
                if (hasNextPage) {
                    const float _nextPageWidth = renderer->getTextDimensions(
                        !m_pageLeftName.empty() ? ("\uE0ED" + ult::GAP_2 + m_pageLeftName)
                                                : ("\uE0EE" + ult::GAP_2 + m_pageRightName),
                        false, 23).first + gapWidth;
                    updateAtomicFloat(ult::nextPageWidth, _nextPageWidth);
                    if (ult::touchingNextPage.load(std::memory_order_acquire)) {
                        float nextX = buttonStartX+2 - _halfGap + _backWidth + 1;
                        if (!m_noClickableItems) nextX += _selectWidth;
                        renderer->drawRoundedRect(nextX, buttonY, _nextPageWidth-2, 73.0f, 12.0f, a(clickColor));
                    }
                } else {
                    ult::nextPageWidth.store(0.0f, std::memory_order_release);
                }
            #endif
    
            #if IS_LAUNCHER_DIRECTIVE
                const std::string currentBottomLine =
                    "\uE0E1" + ult::GAP_2 +
                    (interpreterIsRunningNow ? ult::HIDE : ult::BACK) + ult::GAP_1 +
                    (!m_noClickableItems && !interpreterIsRunningNow ? "\uE0E0" + ult::GAP_2 + ult::OK + ult::GAP_1 : "") +
                    (interpreterIsRunningNow ? "\uE0E5" + ult::GAP_2 + ult::CANCEL + ult::GAP_1 : "") +
                    (!interpreterIsRunningNow
                        ? (!ult::usePageSwap
                            ? ((m_menuMode == "packages") ? "\uE0ED" + ult::GAP_2 + ult::OVERLAYS_ABBR
                               : (m_menuMode == "overlays") ? "\uE0EE" + ult::GAP_2 + ult::PACKAGES : "")
                            : ((m_menuMode == "packages") ? "\uE0EE" + ult::GAP_2 + ult::OVERLAYS_ABBR
                               : (m_menuMode == "overlays") ? "\uE0ED" + ult::GAP_2 + ult::PACKAGES : ""))
                        : "") +
                    (!interpreterIsRunningNow && !m_pageLeftName.empty()  ? "\uE0ED" + ult::GAP_2 + m_pageLeftName  :
                     !interpreterIsRunningNow && !m_pageRightName.empty() ? "\uE0EE" + ult::GAP_2 + m_pageRightName : "");
                const bool _hasOkBtn = !m_noClickableItems && !interpreterIsRunningNow;
            #else
                const std::string currentBottomLine =
                    "\uE0E1" + ult::GAP_2 + ult::BACK + ult::GAP_1 +
                    (!m_noClickableItems ? "\uE0E0" + ult::GAP_2 + ult::OK + ult::GAP_1 : "") +
                    (!m_pageLeftName.empty()  ? "\uE0ED" + ult::GAP_2 + m_pageLeftName  :
                     !m_pageRightName.empty() ? "\uE0EE" + ult::GAP_2 + m_pageRightName : "");
                const bool _hasOkBtn = !m_noClickableItems;
            #endif
    
                renderer->drawStringWithColoredSections(currentBottomLine, false, tsl::s_footerSpecialChars,
                                                        buttonStartX, 693, 23, bottomTextColor, buttonColor);
                if (_hasOkBtn && !usingUnfocusedColor) {
                    static const std::string okOverdraw = "\uE0E0" + ult::GAP_2 + ult::OK + ult::GAP_1;
                    renderer->drawStringWithColoredSections(okOverdraw, false, tsl::s_footerSpecialChars,
                                                            buttonStartX + _backWidth, 693, 23, unfocusedColor, unfocusedColor);
                }
    
            #if USING_FPS_INDICATOR_DIRECTIVE
                {
                    const u64 currentTime_ns = ult::nowNs();
                    const float currentFps = updateFPS(currentTime_ns / 1e9);
                    static char fpsBuffer[32];
                    static float lastFps = -1.0f;
                    if (std::abs(currentFps - lastFps) > 0.1f) {
                        snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS: %.2f", currentFps);
                        lastFps = currentFps;
                    }
                    static constexpr tsl::Color whiteColor = {0xF,0xF,0xF,0xF};
                    renderer->drawString(fpsBuffer, false, 20, tsl::cfg::FramebufferHeight - 60, 20, whiteColor);
                }
            #endif
            
                if (m_contentElement != nullptr)
                    m_contentElement->frame(renderer);
            
                drawEdgeSeparator(renderer);
            }
        
            inline void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                setBoundaries(parentX, parentY, parentWidth, parentHeight);
                
                if (m_contentElement != nullptr) {
                    m_contentElement->setBoundaries(parentX + 35, parentY + 97, parentWidth - 85, parentHeight - 73 - 105);
                    m_contentElement->invalidate();
                }
            }
            
            inline Element* requestFocus(Element *oldFocus, FocusDirection direction) override {
                return m_contentElement ? m_contentElement->requestFocus(oldFocus, direction) : nullptr;
            }
            
            inline bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) {
                // Discard touches outside bounds
                if (!m_contentElement || !m_contentElement->inBounds(currX, currY))
                    return false;
                
                return m_contentElement->onTouch(event, currX, currY, prevX, prevY, initialX, initialY);
            }
            
            /**
             * @brief Sets the content of the frame
             *
             * @param content Element
             */
            inline void setContent(Element *content) {
                delete m_contentElement;
                m_contentElement = content;
                
                if (content != nullptr) {
                    m_contentElement->setParent(this);
                    invalidate();
                }
            }
            
            /**
             * @brief Changes the title of the menu
             *
             * @param title Title to change to
             */
            inline void setTitle(const std::string& t)    { resetScroll(titleScroll, m_title,    t); }
            
            /**
             * @brief Changes the subtitle of the menu
             *
             * @param title Subtitle to change to
             */
            inline void setSubtitle(const std::string& s) { resetScroll(subScroll,   m_subtitle, s); }
            
        protected:
            Element *m_contentElement = nullptr;
            
        private:
            // Unified scroll state structure
            struct ScrollState {
                u64 timeIn, lastUpd;
                float offset;
                u32 maxW, textW;
                bool active, trunc;
                std::string scrollText;
            };
            
            ScrollState subScroll = {0, 0, 0.0f, 0, 0, false, false, ""};
            ScrollState titleScroll = {0, 0, 0.0f, 0, 0, false, false, ""};
            
            // Unified width calculation
            void calcScrollWidth(gfx::Renderer* renderer, ScrollState& s, const std::string& text, u32 fontSize, bool widgetDrawn) {
                if (s.maxW) return;
                
                s.maxW = widgetDrawn ? 217 : (tsl::cfg::FramebufferWidth - 40);
                
                const u32 w = renderer->getTextDimensions(text, false, fontSize).first;
                s.trunc = w > s.maxW;
                
                if (s.trunc) {
                    s.scrollText = text + "        ";
                    s.textW = renderer->getTextDimensions(s.scrollText, false, fontSize).first;
                    s.scrollText += text;
                } else {
                    s.textW = w;
                }
            }

            static void resetScroll(ScrollState& s, std::string& dest, const std::string& src) {
                if (dest == src) return;
                dest = src;
                s.maxW = 0;
                s.active = s.trunc = false;
            }
            
        #if IS_LAUNCHER_DIRECTIVE
            // Get package color based on m_colorSelection
            tsl::Color getPackageColor() const {
                if (m_colorSelection.empty()) return defaultPackageColor;
                
                const char c = m_colorSelection[0];
                const size_t len = m_colorSelection.length();
                
                switch (c) {
                    case 'g': return (len == 5) ? tsl::Color{0x0,0xF,0x0,0xF} : defaultPackageColor;
                    case 'r': return (len == 3) ? tsl::Color{0xF,0x2,0x4,0xF} : defaultPackageColor;
                    case 'b': return (len == 4) ? tsl::Color{0x7,0x7,0xF,0xF} : defaultPackageColor;
                    case 'y': return (len == 6) ? tsl::Color{0xF,0xF,0x0,0xF} : defaultPackageColor;
                    case 'o': return (len == 6) ? tsl::Color{0xF,0xA,0x0,0xF} : defaultPackageColor;
                    case 'p': 
                        if (len == 4) return tsl::Color{0xF,0x6,0xB,0xF};
                        if (len == 6) return tsl::Color{0x8,0x0,0x8,0xF};
                        return defaultPackageColor;
                    case 'w': return (len == 5) ? tsl::Color{0xF,0xF,0xF,0xF} : defaultPackageColor;
                    case '#': 
                        return (len == 7 && isValidHexColor(m_colorSelection.substr(1))) 
                            ? RGB888(m_colorSelection.substr(1)) : defaultPackageColor;
                    default: return defaultPackageColor;
                }
            }
        #endif
            
            // Draw scrollable text with common parameters
            void drawScrollableText(gfx::Renderer* renderer, ScrollState& s, const tsl::Color& clr, 
                                   int xPos, int yPos, u32 fontSize, int scissorYOffset, int scissorHeight) {
                if (s.trunc) {
                    if (!s.active) {
                        s.active = true;
                        s.timeIn = ult::nowNs();
                    }
                    
                    renderer->enableScissoring(xPos, yPos - scissorYOffset, s.maxW, scissorHeight);
                    renderer->drawString(s.scrollText, false, xPos - static_cast<s32>(s.offset), yPos, fontSize, clr);
                    renderer->disableScissoring();
                    
                    updateScroll(s);
                } else {
                    renderer->drawString(m_title, false, xPos, yPos, fontSize, clr);
                }
            }
            
            // Unified scroll update
            void updateScroll(ScrollState& s) {
                const u64 now = ult::nowNs();
                
                // Only update at ~120Hz
                if (now - s.lastUpd < 8333333ULL) return;
                
                static constexpr double delay = 3.0, pause = 2.0, vel = 100.0, accel = 0.5, decel = 0.5;
                static constexpr double invBil = 1e-9, invAccel = 2.0, invDecel = 2.0;
                
                const double minDist = s.textW;
                const double accelDist = 0.5 * vel * accel;
                const double decelDist = 0.5 * vel * decel;
                const double constDist = std::max(0.0, minDist - accelDist - decelDist);
                const double constTime = constDist / vel;
                const double totalDur = delay + accel + constTime + decel + pause;
                
                const double t = (now - s.timeIn) * invBil;
                const double cycle = std::fmod(t, totalDur);
                
                if (cycle < delay) {
                    s.offset = 0.0f;
                } else if (cycle < delay + accel + constTime + decel) {
                    const double st = cycle - delay;
                    double d;
                    
                    if (st <= accel) {
                        const double r = st * invAccel;
                        d = r * r * accelDist;
                    } else if (st <= accel + constTime) {
                        d = accelDist + (st - accel) * vel;
                    } else {
                        const double r = (st - accel - constTime) * invDecel;
                        const double omr = 1.0 - r;
                        d = accelDist + constDist + (1.0 - omr * omr) * (minDist - accelDist - constDist);
                    }
                    
                    s.offset = static_cast<float>(std::min(d, minDist));
                } else {
                    s.offset = static_cast<float>(s.textW);
                }
                
                s.lastUpd = now;
                
                if (t >= totalDur) s.timeIn = now;
            }
        };
        
    #if IS_STATUS_MONITOR_DIRECTIVE

        /**
         * @brief The base frame which can contain another view
         *
         */
        class HeaderOverlayFrame : public Element {
        public:
            /**
             * @brief Constructor
             *
             * @param title Name of the Overlay drawn bolt at the top
             * @param subtitle Subtitle drawn bellow the title e.g version number
             */
            std::string m_title;
            std::string m_subtitle;
            bool m_noClickableItems;

            float x, y;
            int offset, y_offset;
            int fontSize;
            
        HeaderOverlayFrame(const std::string& title, const std::string& subtitle, const bool& _noClickableItems=false)
            : Element(), m_title(title), m_subtitle(subtitle), m_noClickableItems(_noClickableItems) {
                ult::activeHeaderHeight = 97;

                if (FullMode)
                    ult::loadWallpaperFileWhenSafe();
                else
                    svcSleepThread(250'000); // sleep thread for initial values to auto-load

                m_isItem = false;
            }

            virtual ~HeaderOverlayFrame() {
                if (this->m_contentElement != nullptr)
                    delete this->m_contentElement;
            }

            
            virtual void draw(gfx::Renderer *renderer) override {

                if (m_noClickableItems != ult::noClickableItems.load(std::memory_order_acquire)) {
                    ult::noClickableItems.store(m_noClickableItems, std::memory_order_release);
                }
                
                
                if (FullMode == true) {
                    if ((lastMode.empty() || (lastMode.compare("returning") == 0)) &&
                        ult::expandedMemory && !ult::refreshWallpaper.load(std::memory_order_acquire) &&
                        !ult::wallpaperData.empty() && ult::correctFrameSize)
                        renderer->drawWallpaper();   // bakes bg color — no fillScreen needed
                    else {
                        renderer->fillScreen(a(defaultBackgroundColor));
                        if (lastMode.empty() || (lastMode.compare("returning") == 0))
                            renderer->drawWallpaper();
                    }
                } else {
                    renderer->fillScreen({ 0x0, 0x0, 0x0, 0x0});
                }
                
                y = 50;
                offset = 0;
                
                // Use cached or current data for rendering
                const std::string& renderTitle = m_title;
                const std::string& renderSubtitle = m_subtitle;
                
                renderer->drawString(renderTitle, false, 20, 50, 32, defaultOverlayColor);
                renderer->drawString(renderSubtitle, false, 20, y+2+23, 15, bannerVersionTextColor);
                
                if (FullMode == true)
                    renderer->drawRect(15, tsl::cfg::FramebufferHeight - 73, tsl::cfg::FramebufferWidth - 30, 1, a(bottomSeparatorColor));
                
                // Set initial button position
                static constexpr float buttonStartX = 30;
                
                if (FullMode && !deactivateOriginalFooter)
                    updateFooterButtonWidths(renderer,
                        "\uE0E1" + ult::GAP_2 + ult::BACK,
                        "\uE0E0" + ult::GAP_2 + ult::OK,
                        m_noClickableItems);
                
                // Build current bottom line
                const std::string currentBottomLine = 
                    "\uE0E1" + ult::GAP_2 + ult::BACK + ult::GAP_1 +
                    (!m_noClickableItems 
                        ? "\uE0E0" + ult::GAP_2 + ult::OK + ult::GAP_1
                        : "");
                
                const std::string& menuBottomLine = currentBottomLine;
                
                // Render the text with special character handling
                if (!deactivateOriginalFooter) {
                    renderer->drawStringWithColoredSections(menuBottomLine, false, tsl::s_footerSpecialChars,
                                                            buttonStartX, 693, 23, bottomTextColor, buttonColor);
                    if (!m_noClickableItems && !usingUnfocusedColor) {
                        renderer->drawStringWithColoredSections("\uE0E0" + ult::GAP_2 + ult::OK + ult::GAP_1, false,
                                                                tsl::s_footerSpecialChars,
                                                                buttonStartX + ult::backWidth.load(std::memory_order_acquire),
                                                                693, 23, unfocusedColor, unfocusedColor);
                    }
                }
                
                if (this->m_contentElement != nullptr)
                    this->m_contentElement->frame(renderer);

                if (FullMode)
                    drawEdgeSeparator(renderer);
            }
            

            
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                this->setBoundaries(parentX, parentY, parentWidth, parentHeight);
        
                if (this->m_contentElement != nullptr) {
                    this->m_contentElement->setBoundaries(parentX + 35, parentY + ult::activeHeaderHeight, parentWidth - 85, parentHeight - 73 - 105);
                    this->m_contentElement->invalidate();
                }
            }
            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) override {
                if (this->m_contentElement != nullptr)
                    return this->m_contentElement->requestFocus(oldFocus, direction);
                else
                    return nullptr;
            }
            
            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) {
                // Discard touches outside bounds
                if (!this->m_contentElement->inBounds(currX, currY))
                    return false;
                
                if (this->m_contentElement != nullptr)
                    return this->m_contentElement->onTouch(event, currX, currY, prevX, prevY, initialX, initialY);
                else return false;
            }
            
            /**
             * @brief Sets the content of the frame
             *
             * @param content Element
             */
            inline void setContent(Element *content) {
                if (this->m_contentElement != nullptr)
                    delete this->m_contentElement;
                
                this->m_contentElement = content;
                
                if (content != nullptr) {
                    this->m_contentElement->setParent(this);
                    this->invalidate();
                }
            }
            
            /**
             * @brief Changes the title of the menu
             *
             * @param title Title to change to
             */
            inline void setTitle(const std::string &title) {
                this->m_title = title;
            }
            
            /**
             * @brief Changes the subtitle of the menu
             *
             * @param title Subtitle to change to
             */
            inline void setSubtitle(const std::string &subtitle) {
                this->m_subtitle = subtitle;
            }
            
        protected:
            Element *m_contentElement = nullptr;
            
            //std::string m_title, m_subtitle;
        };
    #else
        /**
         * @brief The base frame which can contain another view with a customizable header
         *
         */
        class HeaderOverlayFrame : public Element {
        public:
        #if USING_WIDGET_DIRECTIVE
            bool m_showWidget = false;
        #endif

            HeaderOverlayFrame(u16 headerHeight = 175) : Element(), m_headerHeight(headerHeight) {
                ult::activeHeaderHeight = headerHeight;
                // Load the bitmap file into memory
                ult::loadWallpaperFileWhenSafe();
                m_isItem = false;

            }
            virtual ~HeaderOverlayFrame() {
                if (this->m_contentElement != nullptr)
                    delete this->m_contentElement;
                
                if (this->m_header != nullptr)
                    delete this->m_header;
            }
            
            virtual void draw(gfx::Renderer *renderer) override {
                
                if (ult::expandedMemory && !ult::refreshWallpaper.load(std::memory_order_acquire) &&
                    !ult::wallpaperData.empty() && ult::correctFrameSize)
                    renderer->drawWallpaper();
                else
                    renderer->fillScreen(a(defaultBackgroundColor));
                renderer->drawRect(15, tsl::cfg::FramebufferHeight - 73, tsl::cfg::FramebufferWidth - 30, 1, a(bottomSeparatorColor));
                
                #if USING_WIDGET_DIRECTIVE
                if (m_showWidget)
                    renderer->drawWidget();
                #endif

                updateFooterButtonWidths(renderer,
                    "\uE0E1" + ult::GAP_2 + ult::BACK,
                    "\uE0E0" + ult::GAP_2 + ult::OK,
                    false);
                static constexpr float buttonStartX = 30;
            
                // Draw bottom text
                const std::string menuBottomLine = "\uE0E1" + ult::GAP_2 + ult::BACK + ult::GAP_1 +
                                                   "\uE0E0" + ult::GAP_2 + ult::OK + ult::GAP_1;
                renderer->drawStringWithColoredSections(menuBottomLine, false,
                                                        {"\uE0E1", "\uE0E0", "\uE0ED", "\uE0EE"},
                                                        buttonStartX, 693, 23,
                                                        bottomTextColor, buttonColor);
                if (!usingUnfocusedColor) {
                    renderer->drawStringWithColoredSections("\uE0E0" + ult::GAP_2 + ult::OK + ult::GAP_1, false,
                                                            {"\uE0E1", "\uE0E0", "\uE0ED", "\uE0EE"},
                                                            buttonStartX + ult::backWidth.load(std::memory_order_acquire), 693, 23,
                                                            unfocusedColor, unfocusedColor);
                }
            
                if (this->m_header != nullptr)
                    this->m_header->frame(renderer);
            
                if (this->m_contentElement != nullptr)
                    this->m_contentElement->frame(renderer);

                drawEdgeSeparator(renderer);
            }
            
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                this->setBoundaries(parentX, parentY, parentWidth, parentHeight);
                
                if (this->m_contentElement != nullptr) {
                    this->m_contentElement->setBoundaries(parentX + 35, parentY + this->m_headerHeight, parentWidth - 85, parentHeight - 73 - this->m_headerHeight -8);
                    this->m_contentElement->invalidate();
                }
                
                if (this->m_header != nullptr) {
                    this->m_header->setBoundaries(parentX, parentY, parentWidth, this->m_headerHeight);
                    this->m_header->invalidate();
                }
            }
            
            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) {
                // Discard touches outside bounds
                if (!this->m_contentElement->inBounds(currX, currY))
                    return false;
                
                if (this->m_contentElement != nullptr)
                    return this->m_contentElement->onTouch(event, currX, currY, prevX, prevY, initialX, initialY);
                else return false;
            }
            
            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) override {
                if (this->m_contentElement != nullptr)
                    return this->m_contentElement->requestFocus(oldFocus, direction);
                else
                    return nullptr;
            }
            
            /**
             * @brief Sets the content of the frame
             *
             * @param content Element
             */
            inline void setContent(Element *content) {
                if (this->m_contentElement != nullptr)
                    delete this->m_contentElement;
                
                this->m_contentElement = content;
                
                if (content != nullptr) {
                    this->m_contentElement->setParent(this);
                    this->invalidate();
                }
            }
            
            /**
             * @brief Sets the header of the frame
             *
             * @param header Header custom drawer
             */
            inline void setHeader(CustomDrawer *header) {
                if (this->m_header != nullptr)
                    delete this->m_header;
                
                this->m_header = header;
                
                if (header != nullptr) {
                    this->m_header->setParent(this);
                    this->invalidate();
                }
            }
            
        protected:
            Element *m_contentElement = nullptr;
            CustomDrawer *m_header = nullptr;
            
            u16 m_headerHeight;
        };
    #endif
        


        /**
         * @brief Single color rectangle element mainly used for debugging to visualize boundaries
         *
         */
        class DebugRectangle : public Element {
        public:
            /**
             * @brief Constructor
             *
             * @param color Color of the rectangle
             */
            DebugRectangle(Color color) : Element(), m_color(color) {
                m_isItem = false;
            }
            virtual ~DebugRectangle() {}
            
            virtual void draw(gfx::Renderer *renderer) override {
                renderer->drawRect(ELEMENT_BOUNDS(this), a(this->m_color));
            }
            
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {}
            
        private:
            Color m_color;
        };

        class ListItem; // forward declaration

        static std::atomic<float> s_currentScrollVelocity{0};
        static std::atomic<bool> s_directionalKeyReleased{false};
        static std::atomic<bool> lastInternalTouchRelease{true};

        static std::mutex s_safeToSwapMutex;
        static std::atomic<bool> s_safeToSwap{false};
        static std::atomic<bool> skipOnce{false};

        static std::atomic<bool> isTableScrolling{false};

        class List : public Element {
        
        public:
            List() : Element() {
            
                s_safeToSwap.store(false, std::memory_order_release);
                
                // Clear table scrolling flag when list is cleared
                isTableScrolling.store(false, std::memory_order_release);

                // Initialize instance state
                m_pendingJump = false;
                m_clearList = false;
                m_focusedIndex = 0;
                m_offset = 0;
                m_nextOffset = 0;
                m_listHeight = 0;
                actualItemCount = 0;
                m_isItem = false;
                m_hasSetInitialFocusHack = false;
                m_hasRenderedInitialFocus = false;

                // Initialize new scrollbar color transition members
                m_scrollbarAtWall = false;
                m_scrollbarColorTransition = 0.0f;
                m_lastWallReleaseTime = 0;
            }
            
            virtual ~List() {
                s_safeToSwap.store(false, std::memory_order_release);
                purgePendingItems();
                clearItems();
            }
            
                                                            
            virtual void draw(gfx::Renderer* renderer) override {
            
                s_safeToSwap.store(false, std::memory_order_release);
                std::lock_guard<std::mutex> lock(s_safeToSwapMutex);
                
                if (m_clearList) {
                    clearItems();
                    return;
                }
                
                bool justResolved = false;
                
                // Process pending operations
                if (!m_itemsToAdd.empty()) {
                    // Add items to m_items but DON'T invalidate yet
                    addPendingItems(true);  // Skip invalidate
                    
                    // Calculate m_listHeight FIRST
                    m_listHeight = BOTTOM_PADDING;
                    for (Element* entry : m_items) {
                        m_listHeight += entry->getHeight();
                    }
                    
                    // NOW invalidate with m_offset still at 0 to get initial positions
                    invalidate();
                    
                    // THEN resolve jump AFTER layout has positioned items
                    if (m_pendingJump && !m_items.empty()) {
                        resolveJumpImmediately();
                        justResolved = true;
                    } else if (!m_hasSetInitialFocusHack && !m_items.empty()) {
                        // NO JUMP: Set up focus on first item
                        for (size_t i = 0; i < m_items.size(); ++i) {
                            if (m_items[i]->m_isItem) {
                                m_focusedIndex = i;
                                m_hasSetInitialFocusHack = true;
                                
                                // Calculate position using the same logic as updateScrollOffset
                                float itemPos = 0.0f;
                                for (size_t j = 0; j < i && j < m_items.size(); ++j) {
                                    itemPos += m_items[j]->getHeight();
                                }
                                
                                const float itemHeight = m_items[i]->getHeight();
                                const float viewHeight = static_cast<float>(getHeight());
                                const float maxOffset = (m_listHeight > viewHeight) ? 
                                                       static_cast<float>(m_listHeight - viewHeight) : 0.0f;
                                
                                const float itemCenterPos = itemPos + (itemHeight / 2.0f);
                                const float viewportCenter = viewHeight / 2.0f + VIEW_CENTER_OFFSET + 0.5f;
                                const float idealOffset = std::max(0.0f, std::min(itemCenterPos - viewportCenter, maxOffset));
                                
                                m_offset = m_nextOffset = idealOffset;
                                
                                // Now invalidate AGAIN with correct offset
                                invalidate();
                                justResolved = true;
                                break;
                            }
                        }
                    }
                }
                if (!m_itemsToRemove.empty()) {
                    removePendingItems();
                }
                
                const s32 topBound = getTopBound();
                const s32 bottomBound = getBottomBound();
                const s32 height = getHeight();
                
                renderer->enableScissoring(getLeftBound(), topBound-8, getWidth() + 8, height + 14);
            
                // Manually set focus flag on the target item for the first frame
                if (m_hasSetInitialFocusHack && !m_hasRenderedInitialFocus && !m_items.empty() && m_focusedIndex < m_items.size()) {
                    bool anyItemFocused = false;
                    for (Element* item : m_items) {
                        if (item && item->hasFocus()) {
                            anyItemFocused = true;
                            break;
                        }
                    }
                    
                    if (!anyItemFocused) {
                        m_items[m_focusedIndex]->setFocused(true);
                    }
                    m_hasRenderedInitialFocus = true;
                }
            
                // Scissor constants that mirror exactly what TableDrawer::draw() sets:
                //   enableScissoring(0, 88, FramebufferWidth, FramebufferHeight - 73 - 97 + 2 + 5)
                // The List's logical bounds (topBound ≈ 97, bottomBound ≈ 647) don't match this,
                // which causes two culling bugs for table items:
                //
                //  TOP BUG  – rect vanishes too early while scrolling up:
                //    Outer cull stops when logical_bottom ≤ topBound (97), but the rounded-rect
                //    background extends (20 − endGap + 2) px below logical_bottom (≤ 19 px for
                //    the smallest endGap used).  Background is still inside scissor [88, 645] for
                //    those extra pixels, so dropping the draw call makes the rect jump-disappear.
                //    Fix: keep drawing until  logical_bottom + kTableBGOverhang > kTableScissorTop,
                //    i.e. until the background has fully scrolled above the scissor edge.
                //
                //  BOTTOM BUG – lines appear only when they can fit (entering from below):
                //    Outer cull starts when logical_top < bottomBound (647), but the first row
                //    isn't drawn until logical_top + startGap < kTableScissorBottom (645), i.e.
                //    logical_top < 625.  The 22 px gap (647→625) shows background with no rows.
                //    Fix: don't start the draw call until the first row is about to enter the
                //    scissor, i.e. logical_top < kTableScissorBottom − kTableFirstRowGap.
                static constexpr s32 kTableScissorTop   = 88;
                const         s32 kTableScissorBottom   = kTableScissorTop
                    + static_cast<s32>(cfg::FramebufferHeight) - 73 - 97 + 2 + 5;
                // Background overhang below logical bottom = 20 − endGap + 2.
                // Worst case (smallest endGap = 3) → 19 px; use 20 to be safe.
                static constexpr s32 kTableBGOverhang   = 20;
                // Default startGap in buildTableDrawerLines (first row y-offset from logical top).
                static constexpr s32 kTableFirstRowGap  = 20;

                for (Element* entry : m_items) {
                    if (entry->isTable()) {
                        if (entry->getBottomBound() + kTableBGOverhang  > kTableScissorTop &&
                            entry->getTopBound()                         < kTableScissorBottom - kTableFirstRowGap) {
                            entry->frame(renderer);
                        }
                    } else {
                        if (entry->getBottomBound() > topBound && entry->getTopBound() < bottomBound) {
                            entry->frame(renderer);
                        }
                    }
                }
            
                renderer->disableScissoring();
                
                if (m_listHeight > height) {
                    drawScrollbar(renderer, height);
                    if (!justResolved) {
                        updateScrollAnimation();
                    }
                }
                
                s_safeToSwap.store(true, std::memory_order_release);
            }
            
            void resolveJumpImmediately() {
                float h = 0.0f;
                bool foundMatch = false;
                
                for (size_t i = 0; i < m_items.size(); ++i) {
                    if (m_items[i]->matchesJumpCriteria(m_jumpToText, m_jumpToValue, m_jumpToExactMatch)) {
                        m_focusedIndex = i;
                        foundMatch = true;
                        
                        // Calculate position using the same logic as updateScrollOffset
                        const float itemHeight = m_items[i]->getHeight();
                        const float viewHeight = static_cast<float>(getHeight());
                        const float maxOffset = (m_listHeight > viewHeight) ? 
                                               static_cast<float>(m_listHeight - viewHeight) : 0.0f;
                        
                        const float itemCenterPos = h + (itemHeight / 2.0f);
                        const float viewportCenter = viewHeight / 2.0f + VIEW_CENTER_OFFSET + 0.5f;
                        const float idealOffset = std::max(0.0f, std::min(itemCenterPos - viewportCenter, maxOffset));
                        
                        m_offset = m_nextOffset = idealOffset;
                        
                        // Now invalidate AGAIN with correct offset so layout repositions items
                        invalidate();
                        
                        // Manually set the focus flag for first frame drawing
                        m_items[m_focusedIndex]->setFocused(true);
                        
                        m_hasSetInitialFocusHack = true;
                        m_hasRenderedInitialFocus = true;
                        m_pendingJump = false;
                        
                        break;
                    }
                    
                    h += m_items[i]->getHeight();
                }
                
                // FALLBACK: If no match found, focus first item instead
                if (!foundMatch) {
                    for (size_t i = 0; i < m_items.size(); ++i) {
                        if (m_items[i]->m_isItem) {
                            m_focusedIndex = i;
                            m_hasSetInitialFocusHack = true;
                            
                            // Calculate position using the same logic as updateScrollOffset
                            float itemPos = 0.0f;
                            for (size_t j = 0; j < i && j < m_items.size(); ++j) {
                                itemPos += m_items[j]->getHeight();
                            }
                            
                            const float itemHeight = m_items[i]->getHeight();
                            const float viewHeight = static_cast<float>(getHeight());
                            const float maxOffset = (m_listHeight > viewHeight) ? 
                                                   static_cast<float>(m_listHeight - viewHeight) : 0.0f;
                            
                            const float itemCenterPos = itemPos + (itemHeight / 2.0f);
                            const float viewportCenter = viewHeight / 2.0f + VIEW_CENTER_OFFSET + 0.5f;
                            const float idealOffset = std::max(0.0f, std::min(itemCenterPos - viewportCenter, maxOffset));
                            
                            m_offset = m_nextOffset = idealOffset;
                            
                            // Now invalidate AGAIN with correct offset
                            invalidate();
                            
                            // Manually set the focus flag for first frame drawing
                            m_items[i]->setFocused(true);

                            m_hasRenderedInitialFocus = true;
                            
                            break;
                        }
                    }
                }
                
                m_pendingJump = false;
            }
                                    
            
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                s32 y = getY() - m_offset;
                
                // Position all items first (don't calculate m_listHeight here)
                for (Element* entry : m_items) {
                    entry->setBoundaries(getX(), y, getWidth(), entry->getHeight());
                    entry->invalidate();
                    y += entry->getHeight();
                }

                
                // Calculate total height AFTER all invalidations are done
                m_listHeight = BOTTOM_PADDING;
                for (Element* entry : m_items) {
                    m_listHeight += entry->getHeight();
                }
            }
                                                
            // Fixed onTouch method - prevents controller state corruption
            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) override {
                // Quick bounds check
                if (!inBounds(currX, currY)) return false;
                
                // Forward to children first
                for (Element* item : m_items) {
                    if (item->onTouch(event, currX, currY, prevX, prevY, initialX, initialY)) {
                        return true;
                    }
                }
                
                // Handle scrolling
                if (event != TouchEvent::Release && Element::getInputMode() == InputMode::TouchScroll) {
                    if (prevX && prevY) {
                        m_nextOffset += (prevY - currY);
                        m_nextOffset = std::clamp(m_nextOffset, 0.0f, static_cast<float>(m_listHeight - getHeight()));
                        
                        // Track that we're touch scrolling
                        m_touchScrollActive = true;
                    }
                    return true;
                }
                
                return false;
            }
            

            inline void addItem(Element* element, u16 height = 0, ssize_t index = -1) {
                if (!element) return;
                
                // First item optimization
                if (actualItemCount == 0 && element->m_isItem) {
                    auto* customDrawer = new tsl::elm::CustomDrawer([](gfx::Renderer*, s32, s32, s32, s32) {});
                    customDrawer->setBoundaries(getX(), getY(), getWidth(), 29+4);
                    customDrawer->setParent(this);
                    customDrawer->invalidate();
                    m_itemsToAdd.emplace_back(-1, customDrawer);
                }
        
                if (height) {
                    element->setBoundaries(getX(), getY(), getWidth(), height);
                }
        
                element->setParent(this);
                element->invalidate();
                m_itemsToAdd.emplace_back(index, element);
                ++actualItemCount;
            }
        
            virtual void removeItem(Element *element) {
                if (element) m_itemsToRemove.push_back(element);
            }
            
            virtual void removeIndex(size_t index) {
                if (index < m_items.size()) removeItem(m_items[index]);
            }
            
            inline void clear() {
                m_clearList = true;
            }
                    
            virtual Element* requestFocus(Element* oldFocus, FocusDirection direction) override {
                if (m_clearList || !m_itemsToAdd.empty()) return nullptr;
                
                // If jump was just resolved, return the target item with proper focus
                if (m_hasSetInitialFocusHack && direction == FocusDirection::None && m_focusedIndex < m_items.size()) {
                    // Request focus properly through the focus system
                    Element* newFocus = m_items[m_focusedIndex]->requestFocus(oldFocus, FocusDirection::None);
                    if (newFocus && newFocus != oldFocus) {
                        return newFocus;
                    }
                }
                
                if (jumpToBottom.exchange(false, std::memory_order_acq_rel))
                    return handleJumpToBottom(oldFocus);
                
                if (jumpToTop.exchange(false, std::memory_order_acq_rel))
                    return handleJumpToTop(oldFocus);
                
                if (skipDown.exchange(false, std::memory_order_acq_rel))
                    return handleSkipDown(oldFocus);
                
                if (skipUp.exchange(false, std::memory_order_acq_rel))
                    return handleSkipUp(oldFocus);
            
                if (direction == FocusDirection::None) {
                    return handleInitialFocus(oldFocus);
                }
                else if (direction == FocusDirection::Down) {
                    return handleDownFocus(oldFocus);
                }
                else if (direction == FocusDirection::Up) {
                    return handleUpFocus(oldFocus);
                }
            
                return oldFocus;
            }

            inline void jumpToItem(const std::string& text = "", const std::string& value = "", bool exactMatch=true) {

                if (!text.empty() || !value.empty()) {
                    m_pendingJump = true;
                    m_jumpToText = text;
                    m_jumpToValue = value;
                    m_jumpToExactMatch = exactMatch;
                }

            }
                        
            virtual Element* getItemAtIndex(u32 index) {
                return (m_items.size() <= index) ? nullptr : m_items[index];
            }
            
            virtual s32 getIndexInList(Element *element) {
                auto it = std::find(m_items.begin(), m_items.end(), element);
                return (it == m_items.end()) ? -1 : static_cast<s32>(it - m_items.begin());
            }
        
            virtual s32 getLastIndex() {
                return static_cast<s32>(m_items.size()) - 1;
            }
            
            virtual void setFocusedIndex(u32 index) {
                if (m_items.size() > index) {
                    m_focusedIndex = index;
                    updateScrollOffset();
                }
            }
            
            inline void onDirectionalKeyReleased() {
                m_hasWrappedInCurrentSequence = false;
                m_lastNavigationResult = NavigationResult::None;
                m_isHolding = false;
                m_stoppedAtBoundary = false;
                m_justArrivedAtBoundary = false; 
                m_lastNavigationTime = 0;
                m_lastScrollTime = 0;
            }

        protected:

            std::vector<Element*> m_items;
            u16 m_focusedIndex = 0;
            
            float m_offset = 0, m_nextOffset = 0;
            s32 m_listHeight = 0;
            
            bool m_clearList = false;
            std::vector<Element*> m_itemsToRemove;
            std::vector<std::pair<ssize_t, Element*>> m_itemsToAdd;
            std::vector<float> prefixSums;
            

            // Enhanced navigation state tracking
            bool m_justWrapped = false;
            bool m_isHolding = false;
            bool m_stoppedAtBoundary = false;
            u64 m_lastNavigationTime = 0;
            static constexpr u64 HOLD_THRESHOLD_NS = 100000000ULL;  // 100ms
        
            size_t actualItemCount = 0;

            // Jump to navigation variables
            std::string m_jumpToText;
            std::string m_jumpToValue;
            bool m_jumpToExactMatch = false;
            bool m_pendingJump = false;

            bool m_justArrivedAtBoundary = false;
            bool m_hasSetInitialFocusHack = false;
            bool m_hasRenderedInitialFocus = false;

            // Stack variables for hot path - reused to avoid allocations
            u32 scrollbarHeight;
            u32 scrollbarOffset;
            u32 prevOffset;
            static constexpr float SCROLLBAR_X_OFFSET = 21.0f;
            static constexpr float SCROLLBAR_Y_OFFSET = 3.0f;
            static constexpr float SCROLLBAR_HEIGHT_TRIM = 6.0f;
            
            bool m_scrollbarAtWall = false;
            float m_scrollbarColorTransition = 0.0f;  // 0.0 = scrollBarColor, 1.0 = scrollBarWallColor
            u64 m_lastWallReleaseTime = 0;
            static constexpr u64 COLOR_TRANSITION_DURATION_NS = 300000000ULL;  // 0.3 seconds

            static constexpr float TABLE_SCROLL_SPEED_PPS = 120.0f*4;      // Pixels per second when holding
            static constexpr float TABLE_SCROLL_SPEED_CLICK_PPS = 120.0f*4; // Pixels per second for single click

            static constexpr float BOTTOM_PADDING = 7.0f;
            static constexpr float VIEW_CENTER_OFFSET = 7.0f;

            u64 m_lastScrollTime = 0;
            float m_scrollVelocity = 0.0f;
            bool m_touchScrollActive = false;

            enum class NavigationResult {
                None,
                Success,
                HitBoundary,
                Wrapped
            };
            
            bool m_hasWrappedInCurrentSequence = false;
            NavigationResult m_lastNavigationResult = NavigationResult::None;
        
        private:

            void clearItems() {

                for (Element* item : m_items) delete item;
                m_items = {};
                m_offset = 0;
                m_focusedIndex = 0;
                invalidate();
                m_clearList = false;
                actualItemCount = 0;
                m_hasSetInitialFocusHack = false;

                // Clear table scrolling flag when list is cleared
                isTableScrolling.store(false, std::memory_order_release);
            }
            
            void addPendingItems(bool skipInvalidate = false) {
                for (auto [index, element] : m_itemsToAdd) {
                    element->invalidate();
                    if (index >= 0 && static_cast<size_t>(index) < m_items.size()) {
                        m_items.insert(m_items.begin() + index, element);
                    } else {
                        m_items.push_back(element);
                    }
                }
                m_itemsToAdd.clear();
                
                if (!skipInvalidate) {
                    invalidate();
                    updateScrollOffset();
                }
            }
            
            void removePendingItems() {
                //size_t index;
                for (Element* element : m_itemsToRemove) {
                    auto it = std::find(m_items.begin(), m_items.end(), element);
                    if (it != m_items.end()) {
                        const size_t index = static_cast<size_t>(it - m_items.begin());
                        m_items.erase(it);
                        if (m_focusedIndex >= index && m_focusedIndex > 0) {
                            --m_focusedIndex;
                        }
                        delete element;
                    }
                }
                m_itemsToRemove = {};
                invalidate();
                updateScrollOffset();
            }

            void purgePendingItems() {
                for (auto& [_, element] : m_itemsToAdd) {
                    if (element) { element->invalidate(); delete element; }
                }
                m_itemsToAdd = {};
                
                //size_t index;
                for (Element* element : m_itemsToRemove) {
                    auto it = std::find(m_items.begin(), m_items.end(), element);
                    if (it != m_items.end()) {
                        const u16 index16 = static_cast<u16>(static_cast<std::size_t>(it - m_items.begin()));
                        element->invalidate();
                        delete element;
                        m_items.erase(it);
            
                        constexpr u16 noFocus = static_cast<u16>(0xFFFF);
                        if (m_focusedIndex == index16)
                            m_focusedIndex = noFocus;
                        else if (m_focusedIndex != noFocus && m_focusedIndex > index16)
                            --m_focusedIndex;
                    }
                }
                m_itemsToRemove = {};
            
                invalidate();
                updateScrollOffset();
            }

            
            void drawScrollbar(gfx::Renderer* renderer, s32 height) {
                const float viewHeight = static_cast<float>(height);
                const float totalHeight = static_cast<float>(m_listHeight);
                const u32 maxScrollableHeight = std::max(static_cast<u32>(totalHeight - viewHeight), 1u);
                
                scrollbarHeight = std::min(static_cast<u32>((viewHeight * viewHeight) / totalHeight), 
                                         static_cast<u32>(viewHeight));
                
                scrollbarOffset = std::min(static_cast<u32>((m_offset / maxScrollableHeight) * (viewHeight - scrollbarHeight)), 
                                         static_cast<u32>(viewHeight - scrollbarHeight));
            
                const u32 scrollbarX = getRightBound() + SCROLLBAR_X_OFFSET;
                const u32 scrollbarY = getY() + scrollbarOffset + SCROLLBAR_Y_OFFSET;
            
                scrollbarHeight -= SCROLLBAR_HEIGHT_TRIM;
            
                // Check if we're at a wall (boundary)
                const bool currentlyAtWall = (m_lastNavigationResult == NavigationResult::HitBoundary) && 
                                              (m_stoppedAtBoundary || m_justArrivedAtBoundary);
                
                static bool triggerOnce = true;

                // Detect transition from "not at wall" to "at wall" - trigger flash ONCE
                if (currentlyAtWall && !m_scrollbarAtWall && !s_directionalKeyReleased.load(std::memory_order_acquire)) {
                    m_scrollbarColorTransition = 1.0f;  // Instant jump to wall color

                    if (triggerOnce) {
                        // NEW: Trigger wall effect here based on scroll position
                        const float maxOffset = static_cast<float>(m_listHeight - getHeight());
                        if (m_offset <= 0.0f) {
                            triggerWallEffect(FocusDirection::Up);
                        } else if (m_offset >= maxOffset - 1.0f) {
                            triggerWallEffect(FocusDirection::Down);
                        }
                    }
                    triggerOnce = false;
                } else {
                    triggerOnce = true;
                }

                // Detect transition from "not at wall" to "at wall" - trigger flash ONCE
                if (currentlyAtWall && !m_scrollbarAtWall && s_directionalKeyReleased.load(std::memory_order_acquire)) {
                    m_scrollbarAtWall = true;
                    m_scrollbarColorTransition = 1.0f;  // Instant jump to wall color
                    m_lastWallReleaseTime = ult::nowNs();  // Start transition immediately
                }
                
                // Reset flag when we leave the wall (so we can trigger again next time)
                if (!currentlyAtWall && m_scrollbarAtWall) {
                    m_scrollbarAtWall = false;
                    m_scrollbarColorTransition = 0.0f;  // Reset to normal immediately
                }
                
                // Smooth transition back to scrollBarColor over 0.5s
                if (m_scrollbarAtWall && m_scrollbarColorTransition > 0.0f) {
                    const u64 currentTime = ult::nowNs();
                    const u64 elapsed = currentTime - m_lastWallReleaseTime;
                    
                    if (elapsed >= COLOR_TRANSITION_DURATION_NS) {
                        m_scrollbarColorTransition = 0.0f;  // Transition complete
                    } else {
                        // Linear interpolation from 1.0 to 0.0
                        const float progress = static_cast<float>(elapsed) / static_cast<float>(COLOR_TRANSITION_DURATION_NS);
                        m_scrollbarColorTransition = 1.0f - progress;
                    }
                }
                
                // Interpolate between scrollBarColor and scrollBarWallColor
                tsl::Color currentColor = scrollBarColor;
                if (m_scrollbarColorTransition >= 1.0f) {
                    currentColor = scrollBarWallColor;
                } else if (m_scrollbarColorTransition > 0.0f) {
                    const float t = m_scrollbarColorTransition;
                    const float oneMinusT = 1.0f - t;
                    
                    const u8 r = static_cast<u8>(scrollBarColor.r * oneMinusT + scrollBarWallColor.r * t);
                    const u8 g = static_cast<u8>(scrollBarColor.g * oneMinusT + scrollBarWallColor.g * t);
                    const u8 b = static_cast<u8>(scrollBarColor.b * oneMinusT + scrollBarWallColor.b * t);
                    const u8 a = static_cast<u8>(scrollBarColor.a * oneMinusT + scrollBarWallColor.a * t);
                    
                    currentColor = tsl::Color(r, g, b, a);
                }
            
                // Draw scrollbar with interpolated color
                renderer->drawRect(scrollbarX, scrollbarY, 5, scrollbarHeight, a(currentColor));
                renderer->drawCircle(scrollbarX + 2, scrollbarY, 2, true, a(currentColor));
                renderer->drawCircle(scrollbarX + 2, scrollbarY + scrollbarHeight, 2, true, a(currentColor));
            }

            
            inline void updateScrollAnimation() {
                if (Element::getInputMode() == InputMode::Controller) {
                    m_touchScrollActive = false;
                    
                    const float diff = m_nextOffset - m_offset;
                    const float distance = std::abs(diff);
                    
                    // Boundary snapping
                    if (distance < 1.0f) {
                        m_offset = m_nextOffset;
                        m_scrollVelocity = 0.0f;
                        s_currentScrollVelocity.store(m_scrollVelocity, std::memory_order_release);
                        
                        if (prevOffset != m_offset) {
                            invalidate();
                            prevOffset = m_offset;
                        }
                        return;
                    }
                    
                    const float maxOffset = static_cast<float>(m_listHeight - getHeight());
                    if (m_nextOffset == 0.0f || m_nextOffset == maxOffset) {
                        if (distance < 3.0f) {
                            m_offset = m_nextOffset;
                            m_scrollVelocity = 0.0f;
                            s_currentScrollVelocity.store(m_scrollVelocity, std::memory_order_release);
                            
                            if (prevOffset != m_offset) {
                                invalidate();
                                prevOffset = m_offset;
                            }
                            return;
                        }
                    }
                    
                    // Emergency correction: only activate when the scroll target (m_nextOffset)
                    // does NOT already bring the focused item into view.  Without this guard,
                    // navigateUp/Down sets m_nextOffset = idealOffset via updateScrollOffset and
                    // then the EC immediately slams m_offset there (up to 0.9× of the full
                    // distance in a single frame), causing the jarring jump the user sees.
                    // When the animation is already heading to the right place, let the smooth
                    // path handle it.
                    if (m_focusedIndex < m_items.size()) {
                        float itemTop = 0.0f;
                        for (size_t i = 0; i < m_focusedIndex; ++i) {
                            itemTop += m_items[i]->getHeight();
                        }
                        const float itemBottom = itemTop + m_items[m_focusedIndex]->getHeight();
                        const float viewBottom = m_offset + getHeight();
                        
                        if (itemTop < m_offset || itemBottom > viewBottom) {
                            const float futureViewBottom = m_nextOffset + static_cast<float>(getHeight());
                            const bool itemWillBeVisible =
                                (itemTop  >= m_nextOffset - 1.0f) &&
                                (itemBottom <= futureViewBottom + 1.0f);
                            
                            if (!itemWillBeVisible) {
                                const float emergencySpeed = (itemBottom < m_offset || itemTop > viewBottom) ? 0.9f : 0.6f;
                                m_offset += diff * emergencySpeed;
                                m_scrollVelocity = diff * 0.3f;
                                s_currentScrollVelocity.store(m_scrollVelocity, std::memory_order_release);
                                
                                if (prevOffset != m_offset) {
                                    invalidate();
                                    prevOffset = m_offset;
                                }
                                return;
                            }
                        }
                    }
                    
                    const bool isTableScrolling = tsl::elm::isTableScrolling.load(std::memory_order_acquire);
                    
                    if (isTableScrolling) {
                        // Direct assignment - instant updates, smoothness comes from small frequent steps
                        m_offset = m_nextOffset;
                        m_scrollVelocity = 0.0f;
                    } else {
                        // Original smooth scrolling for regular navigation
                        const bool isLargeJump = distance > getHeight() * 1.5f;
                        const bool isFromRest = std::abs(m_scrollVelocity) < 2.0f;
                        
                        if (isLargeJump && isFromRest) {
                            static constexpr float gentleAcceleration = 0.08f;
                            static constexpr float gentleDamping = 0.85f;
                            
                            const float targetVelocity = diff * gentleAcceleration;
                            m_scrollVelocity += (targetVelocity - m_scrollVelocity) * gentleDamping;
                        } else {
                            const float urgency = std::min(distance / getHeight(), 1.0f);
                            const float accelerationFactor = 0.18f + (0.24f * urgency);
                            const float dampingFactor = 0.48f - (0.18f * urgency);
                            
                            const float targetVelocity = diff * accelerationFactor;
                            m_scrollVelocity += (targetVelocity - m_scrollVelocity) * dampingFactor;
                        }
                        
                        m_offset += m_scrollVelocity;
                    }
                    
                    // Overshoot prevention
                    if ((m_scrollVelocity > 0 && m_offset > m_nextOffset) ||
                        (m_scrollVelocity < 0 && m_offset < m_nextOffset)) {
                        m_offset = m_nextOffset;
                        m_scrollVelocity = 0.0f;
                    }
                    
                    // Force exact boundary values
                    if (m_nextOffset == 0.0f && m_offset < 1.0f) {
                        m_offset = 0.0f;
                        m_scrollVelocity = 0.0f;
                    } else if (m_nextOffset == maxOffset && m_offset > maxOffset - 1.0f) {
                        m_offset = maxOffset;
                        m_scrollVelocity = 0.0f;
                    }
            
                    s_currentScrollVelocity.store(m_scrollVelocity, std::memory_order_release);
                
                } else if (Element::getInputMode() == InputMode::TouchScroll) {
                    m_offset = m_nextOffset;
                    m_scrollVelocity = 0.0f;
                    
                    if (m_touchScrollActive) {
                        const float viewCenter = m_offset + (getHeight() / 2.0f);
                        float accumHeight = 0.0f;
                        
                        for (size_t i = 0; i < m_items.size(); ++i) {
                            const float itemHeight = m_items[i]->getHeight();
                            const float itemCenter = accumHeight + (itemHeight / 2.0f);
                            
                            if (itemCenter >= viewCenter) {
                                m_focusedIndex = i;
                                break;
                            }
                            
                            accumHeight += itemHeight;
                        }
                    }
                }
                
                if (prevOffset != m_offset) {
                    invalidate();
                    prevOffset = m_offset;
                }
            }
            

            Element* handleInitialFocus(Element* oldFocus) {
                const size_t itemCount = m_items.size();
                if (itemCount == 0) return nullptr;
                
                size_t startIndex = 0;
                
                // Calculate starting index based on current scroll position
                if (!oldFocus && m_offset > 0) {
                    float elementHeight = 0.0f;
                    const size_t maxIndex = itemCount - 1;
                    while (elementHeight < m_offset && startIndex < maxIndex) {
                        elementHeight += m_items[startIndex]->getHeight();
                        ++startIndex;
                    }
                }
                
                // Save current offset to prevent scroll jumping
                const float savedOffset = m_offset;
                const float savedNextOffset = m_nextOffset;
                
                // When recovering from touch scroll (no oldFocus, scrolled into a table region),
                // search BOTH directions from startIndex and pick whichever focusable item is
                // closest to the current viewport center.  Without this the forward-wraparound
                // search skips an entire long table and lands on an item at the opposite end of
                // the list, causing either a jarring upward jump (DOWN) or blind scrollUp stutter (UP).
                if (!oldFocus && startIndex > 0) {
                    const float viewCenter = savedOffset + static_cast<float>(getHeight()) * 0.5f;

                    // Forward search (with wraparound)
                    size_t fwdIdx = itemCount;
                    for (size_t count = 0; count < itemCount; ++count) {
                        const size_t i = (startIndex + count) % itemCount;
                        if (!m_items[i]->isTable()) {
                            Element* f = m_items[i]->requestFocus(oldFocus, FocusDirection::None);
                            if (f && f != oldFocus) { fwdIdx = i; break; }
                        }
                    }

                    // Backward search (no wraparound — items before startIndex)
                    size_t bkwIdx = itemCount;
                    for (ssize_t j = static_cast<ssize_t>(startIndex) - 1; j >= 0; --j) {
                        const size_t i = static_cast<size_t>(j);
                        if (!m_items[i]->isTable()) {
                            Element* f = m_items[i]->requestFocus(oldFocus, FocusDirection::None);
                            if (f && f != oldFocus) { bkwIdx = i; break; }
                        }
                    }

                    // Pick whichever candidate is closer to the viewport centre
                    size_t bestIdx = itemCount;
                    if (fwdIdx < itemCount && bkwIdx < itemCount) {
                        const float fwdCenter = calculateItemPosition(fwdIdx) + m_items[fwdIdx]->getHeight() * 0.5f;
                        const float bkwCenter = calculateItemPosition(bkwIdx) + m_items[bkwIdx]->getHeight() * 0.5f;
                        bestIdx = (std::abs(bkwCenter - viewCenter) <= std::abs(fwdCenter - viewCenter))
                                  ? bkwIdx : fwdIdx;
                    } else if (bkwIdx < itemCount) {
                        bestIdx = bkwIdx;
                    } else if (fwdIdx < itemCount) {
                        bestIdx = fwdIdx;
                    }

                    if (bestIdx < itemCount) {
                        Element* newFocus = m_items[bestIdx]->requestFocus(oldFocus, FocusDirection::None);
                        if (newFocus && newFocus != oldFocus) {
                            m_focusedIndex = bestIdx;
                            m_offset = savedOffset;
                            m_nextOffset = savedNextOffset;
                            return newFocus;
                        }
                    }
                    // Fall through to original logic if nothing found above
                }
                
                // Original single forward-wraparound loop (used when oldFocus is set,
                // when startIndex==0, or as fallback)
                for (size_t count = 0; count < itemCount; ++count) {
                    const size_t i = (startIndex + count) % itemCount;
                    
                    if (!m_items[i]->isTable()) {
                        Element* const newFocus = m_items[i]->requestFocus(oldFocus, FocusDirection::None);
                        if (newFocus && newFocus != oldFocus) {
                            m_focusedIndex = i;
                            m_offset = savedOffset;
                            m_nextOffset = savedNextOffset;
                            return newFocus;
                        }
                    }
                }
                
                return nullptr;
            }
            
            inline void triggerWallEffect(FocusDirection direction) {
                
                if (m_items.empty()) {
                    triggerWallFeedback();
                    return;
                }
            
                // Directional search bounds
                ssize_t i  = static_cast<ssize_t>(m_focusedIndex);
                ssize_t end = (direction == FocusDirection::Down) ? -1 : static_cast<ssize_t>(m_items.size());
                ssize_t step = (direction == FocusDirection::Down) ? -1 : 1;
            
                // Walk until we hit a real item
                for (; i != end; i += step) {
                    auto *it = m_items[i];
                    if (it->m_isItem) {
                        it->shakeHighlight(direction);
                        return;
                    }
                }

                triggerWallFeedback();
            }
            
            inline Element* handleDownFocus(Element* oldFocus) {
                const bool atBottom = isAtBottom();
                updateHoldState();
                
                // Check if the next item is non-focusable
                if (m_focusedIndex + 1 < static_cast<int>(m_items.size()) &&
                    !m_items[m_focusedIndex + 1]->m_isItem && m_listHeight > getHeight()) {
                    isTableScrolling.store(true, std::memory_order_release);
                }
                
                // If holding and at boundary, try to scroll first
                if (m_isHolding && m_stoppedAtBoundary && !atBottom) {
                    scrollDown();
                    m_stoppedAtBoundary = false;
                    return oldFocus;
                }
                
                Element* result = navigateDown(oldFocus);
                
                if (result != oldFocus) {
                    m_lastNavigationResult = NavigationResult::Success;
                    m_stoppedAtBoundary = false;

                    // NEW: Check if we just navigated to the last focusable item
                    // If so, set boundary flag regardless of scroll position
                    bool isLastFocusableItem = true;
                    for (size_t i = m_focusedIndex + 1; i < m_items.size(); ++i) {
                        if (m_items[i]->m_isItem) {
                            isLastFocusableItem = false;
                            break;
                        }
                    }
                    
                    // Set boundary flag if we're at last focusable item OR at scroll bottom
                    m_justArrivedAtBoundary = isLastFocusableItem || isAtBottom();
                    
                    triggerNavigationFeedback();
                    return result;
                }
                
                // Check if we can still scroll down
                if (!atBottom) {
                    scrollDown();
                    return oldFocus;
                }
                
                // Force boundary hit before allowing wrap
                if (m_justArrivedAtBoundary) {
                    m_justArrivedAtBoundary = false;
                    m_stoppedAtBoundary = true;
                    m_lastNavigationResult = NavigationResult::HitBoundary;
                    if (m_listHeight <= getHeight())
                        triggerWallEffect(FocusDirection::Down);
                    return oldFocus;
                }
                
                // Check for wrapping (single tap only)
                if (!m_isHolding && !m_hasWrappedInCurrentSequence) {
                    s_directionalKeyReleased.store(false, std::memory_order_release);
                    m_hasWrappedInCurrentSequence = true;
                    m_lastNavigationResult = NavigationResult::Wrapped;
                    return handleJumpToTop(oldFocus);
                }
                
                // Set boundary flag (for holding)
                m_lastNavigationResult = NavigationResult::HitBoundary;
                if (m_isHolding) {
                    m_stoppedAtBoundary = true;
                }
                
                return oldFocus;
            }
            
            inline Element* handleUpFocus(Element* oldFocus) {
                const bool atTop = isAtTop();
                updateHoldState();
                
                // Check if the previous item is non-focusable
                if (m_focusedIndex > 0 && m_items[m_focusedIndex - 1]->isTable() && m_listHeight > getHeight()) {
                    isTableScrolling.store(true, std::memory_order_release);
                }
                
                // If holding and at boundary, try to scroll first
                if (m_isHolding && m_stoppedAtBoundary && !atTop) {
                    scrollUp();
                    m_stoppedAtBoundary = false;
                    return oldFocus;
                }
                
                // If the currently focused item is off-screen above (e.g. focus was restored
                // to the nearest item after a touch scroll, but the list is still scrolled deep
                // into a table below it), scroll up toward it instead of immediately jumping to
                // the item above via navigateUp.  Without this guard, navigateUp(ListItem B)
                // would succeed (finding ListItem A) and set m_nextOffset=0, causing the list
                // to zoom all the way to the top in one press rather than scrolling gradually.
                if (!atTop && m_focusedIndex < m_items.size()) {
                    const float itemTop = calculateItemPosition(m_focusedIndex);
                    if (itemTop < m_offset) {
                        isTableScrolling.store(true, std::memory_order_release);
                        scrollUp();
                        return oldFocus;
                    }
                }
                
                Element* result = navigateUp(oldFocus);
                
                if (result != oldFocus) {
                    m_lastNavigationResult = NavigationResult::Success;
                    m_stoppedAtBoundary = false;
                    
                    // NEW: Check if we just navigated to the first focusable item
                    // If so, set boundary flag regardless of scroll position
                    bool isFirstFocusableItem = true;
                    for (size_t i = 0; i < m_focusedIndex; ++i) {
                        if (m_items[i]->m_isItem) {
                            isFirstFocusableItem = false;
                            break;
                        }
                    }
                    
                    // Set boundary flag if we're at first focusable item OR at scroll top
                    m_justArrivedAtBoundary = isFirstFocusableItem || isAtTop();
                    
                    triggerNavigationFeedback();
                    return result;
                }
                
                // Check if we can still scroll up
                if (!atTop) {
                    scrollUp();
                    return oldFocus;
                }
                
                // Force boundary hit before allowing wrap
                if (m_justArrivedAtBoundary) {
                    m_justArrivedAtBoundary = false;
                    m_stoppedAtBoundary = true;
                    m_lastNavigationResult = NavigationResult::HitBoundary;
                    if (m_listHeight <= getHeight())
                        triggerWallEffect(FocusDirection::Up);
                    return oldFocus;
                }
                
                // Check for wrapping (single tap only)
                if (!m_isHolding && !m_hasWrappedInCurrentSequence) {
                    s_directionalKeyReleased.store(false, std::memory_order_release);
                    m_hasWrappedInCurrentSequence = true;
                    m_lastNavigationResult = NavigationResult::Wrapped;
                    return handleJumpToBottom(oldFocus);
                }
                
                // Set boundary flag (for holding)
                m_lastNavigationResult = NavigationResult::HitBoundary;
                if (m_isHolding) {
                    m_stoppedAtBoundary = true;
                }
                
                return oldFocus;
            }
            
            
            inline bool isAtTop() {
                if (m_items.empty()) return true;
                
                // Check if we're at scroll position 0
                if (m_offset != 0.0f) return false;
                
                // Even at offset 0, check if the first item is actually visible
                // This handles cases where the first item might be partially above viewport
                if (!m_items.empty()) {
                    Element* firstItem = m_items[0];
                    return firstItem->getTopBound() >= getTopBound();
                }
                
                return true;
            }
            
            inline bool isAtBottom() {
                if (m_items.empty()) return true;
                
                // First check: are we at the maximum scroll offset?
                const bool atMaxOffset = (m_offset >= static_cast<float>(m_listHeight - getHeight()));
                
                // If list is shorter than viewport, we're always at bottom
                if (m_listHeight <= getHeight()) return true;
                
                // If we're not at max offset, we're definitely not at bottom
                if (!atMaxOffset) return false;
                
                // At max offset - now check if the last item is actually fully visible
                // This prevents wrap-around when there's still content below viewport
                if (!m_items.empty()) {
                    Element* lastItem = m_items.back();
                    
                    // We're truly at bottom only if:
                    // 1. We're at max scroll offset AND
                    // 2. The last item's bottom is at or above the viewport bottom
                    return lastItem->getBottomBound() <= getBottomBound();
                }
                
                return atMaxOffset;
            }

            // Helper to check if there are any focusable items
            inline bool hasAnyFocusableItems() {
                for (size_t i = 0; i < m_items.size(); ++i) {
                    if (m_items[i]->m_isItem) return true;
                }
                return false;
            }

            
            inline void updateHoldState() {
                const u64 currentTime = ult::nowNs();
                if ((m_lastNavigationTime != 0 && (currentTime - m_lastNavigationTime) < HOLD_THRESHOLD_NS)) {
                    m_isHolding = true;
                } else {
                    m_isHolding = false;
                    m_stoppedAtBoundary = false;
                    m_hasWrappedInCurrentSequence = false;
                }
                m_lastNavigationTime = currentTime;

                // bug fix, boundary reset upon key release
                if (s_directionalKeyReleased.load(std::memory_order_acquire))
                     m_justArrivedAtBoundary = false;
            }
        
            inline void resetNavigationState() {
                m_hasWrappedInCurrentSequence = false;
                m_lastNavigationResult = NavigationResult::None;
                m_isHolding = false;
                m_stoppedAtBoundary = false;
                m_justArrivedAtBoundary = false;
                m_lastNavigationTime = 0;
            }

            inline Element* handleJumpToItem(Element* oldFocus) {
                resetNavigationState();
                invalidate();
                
                const bool needsScroll = m_listHeight > getHeight();
                const float viewHeight = static_cast<float>(getHeight());
                const float maxOffset = needsScroll ? m_listHeight - viewHeight : 0.0f;
                
                float h = 0.0f;
                

                for (size_t i = 0; i < m_items.size(); ++i) {
                    m_focusedIndex = i;
                    
                    Element* newFocus = m_items[i]->requestFocus(oldFocus, FocusDirection::Down);
                    if (newFocus && newFocus != oldFocus && m_items[i]->matchesJumpCriteria(m_jumpToText, m_jumpToValue, m_jumpToExactMatch)) {
                        // CHANGED: Calculate center of the item and center it in viewport
                        const float itemHeight = m_items[i]->getHeight();
                        // For middle items, use centering logic
                        const float itemCenterPos = h + (itemHeight / 2.0f);  // FIXED: Use center, not bottom
                        const float viewportCenter = viewHeight / 2.0f + VIEW_CENTER_OFFSET + 0.5f; // Same offset as updateScrollOffset

                        // Clamp to valid bounds (same as updateScrollOffset)
                        const float idealOffset = std::max(0.0f, std::min(itemCenterPos - viewportCenter, maxOffset));
                        
                        // Set both current and target offset
                        m_offset = m_nextOffset = idealOffset;
                        
                        return newFocus;
                    }
                    
                    h += m_items[i]->getHeight();
                }
                
                // No match found
                return handleInitialFocus(oldFocus);
            }
        
            inline void syncFocusIndex(Element* oldFocus, ssize_t& searchIndex) {
                for (size_t i = 0; i < m_items.size(); ++i) {
                    if (m_items[i] == oldFocus) {
                        m_focusedIndex = i;
                        searchIndex = static_cast<ssize_t>(i);
                        break;
                    }
                }
            }

            // Core navigation logic
            // Optimized version with variable definitions pulled outside the loop
            inline Element* navigateDown(Element* oldFocus) {
                size_t searchIndex = m_focusedIndex + 1;
                
                // If currently on a table that needs more scrolling
                if (m_focusedIndex < m_items.size() && m_items[m_focusedIndex]->isTable()) {
                    Element* currentTable = m_items[m_focusedIndex];
                    if (currentTable->getBottomBound() > getBottomBound()) {
                        isTableScrolling.store(true, std::memory_order_release);
                        scrollDown();
                        return oldFocus;
                    }
                }
            
                // Sync AFTER table check - if we're not mid-table-scroll
                if (oldFocus && !isTableScrolling.load(std::memory_order_acquire)) {
                    syncFocusIndex(oldFocus, reinterpret_cast<ssize_t&>(searchIndex));
                    searchIndex++;  // Down increments after sync
                }
                
                
                const s32 viewBottom = getBottomBound();
                const float containerHeight = getHeight();
                const float offsetPlusHeight = m_offset + containerHeight;
                
                while (searchIndex < m_items.size()) {
                    Element* item = m_items[searchIndex];
                    m_focusedIndex = searchIndex;
                    
                    if (item->isTable()) {
                        const s32 tableBottom = item->getBottomBound();
                        if (tableBottom > viewBottom) {
                            isTableScrolling.store(true, std::memory_order_release);
                            scrollDown();
                            return oldFocus;
                        }
                        searchIndex++;
                        continue;
                    }
                    
                    Element* newFocus = item->requestFocus(oldFocus, FocusDirection::Down);
                    if (newFocus && newFocus != oldFocus) {
                        isTableScrolling.store(false, std::memory_order_release);
                        updateScrollOffset();
                        return newFocus;
                    } else {
                        const float itemBottom = calculateItemPosition(searchIndex) + item->getHeight();
                        if (itemBottom > offsetPlusHeight) {
                            isTableScrolling.store(true, std::memory_order_release);
                            scrollDown();
                            return oldFocus;
                        }
                        searchIndex++;
                    }
                }
                
                // ADDED: Clear flag when navigation completes without finding anything
                if (m_focusedIndex >= m_items.size() || !m_items[m_focusedIndex]->isTable())
                    isTableScrolling.store(false, std::memory_order_release);
                return oldFocus;
            }
            
            inline Element* navigateUp(Element* oldFocus) {

                ssize_t searchIndex = static_cast<ssize_t>(m_focusedIndex) - 1;
                
                // If currently on a table that needs more scrolling
                if (m_focusedIndex < m_items.size() && m_items[m_focusedIndex]->isTable()) {
                    Element* currentTable = m_items[m_focusedIndex];
                    if (currentTable->getTopBound() < getTopBound()) {
                        isTableScrolling.store(true, std::memory_order_release);
                        scrollUp();
                        return oldFocus;
                    }
                }
            
                // Sync AFTER table check - if we're not mid-table-scroll
                if (oldFocus && !isTableScrolling.load(std::memory_order_acquire)) {
                    syncFocusIndex(oldFocus, searchIndex);
                    searchIndex--;  // Up decrements after sync
                }
                
                const s32 viewTop = getTopBound();
                const float offset = m_offset;
                
                while (searchIndex >= 0) {
                    Element* item = m_items[searchIndex];
                    m_focusedIndex = static_cast<size_t>(searchIndex);
                    
                    if (item->isTable()) {
                        const s32 tableTop = item->getTopBound();
                        if (tableTop < viewTop) {
                            isTableScrolling.store(true, std::memory_order_release);
                            scrollUp();
                            return oldFocus;
                        }
                        searchIndex--;
                        continue;
                    }
                    
                    Element* newFocus = item->requestFocus(oldFocus, FocusDirection::Up);
                    if (newFocus && newFocus != oldFocus) {
                        isTableScrolling.store(false, std::memory_order_release);
                        updateScrollOffset();
                        return newFocus;
                    } else {
                        const float itemTop = calculateItemPosition(static_cast<size_t>(searchIndex));
                        if (itemTop < offset) {
                            isTableScrolling.store(true, std::memory_order_release);
                            scrollUp();
                            return oldFocus;
                        }
                        searchIndex--;
                    }
                }
                
                // Only clear table scrolling if we're not still focused on a table
                if (m_focusedIndex >= m_items.size() || !m_items[m_focusedIndex]->isTable())
                    isTableScrolling.store(false, std::memory_order_release);
                return oldFocus;
            }
            
            // Helper method to calculate an item's position in the list
            inline float calculateItemPosition(size_t index) {
                float position = 0.0f;
                for (size_t i = 0; i < index && i < m_items.size(); ++i) {
                    position += m_items[i]->getHeight();
                }
                return position;
            }


            inline float getScrollDelta() {
                const u64 currentTime = ult::nowNs();
                float deltaTime = (m_lastScrollTime != 0)
                    ? static_cast<float>(currentTime - m_lastScrollTime) / 1000000000.0f
                    : 1.0f / 60.0f;
                m_lastScrollTime = currentTime;
                deltaTime = std::min(deltaTime, 0.1f);
                const float speedPPS = m_isHolding ? TABLE_SCROLL_SPEED_PPS : TABLE_SCROLL_SPEED_CLICK_PPS;
                return speedPPS * deltaTime;
            }

            // Enhanced scroll methods that snap to exact boundaries
            inline void scrollDown() {
                m_nextOffset = std::min(m_nextOffset + getScrollDelta(),
                                       static_cast<float>(m_listHeight - getHeight()));
            }
            
            inline void scrollUp() {
                m_nextOffset = std::max(m_nextOffset - getScrollDelta(), 0.0f);
            }

            // Unified jump-to-edge: toBottom=true → jump to bottom, false → jump to top
            Element* handleJumpToEdge(Element* oldFocus, bool toBottom) {
                if (m_items.empty()) return oldFocus;

                invalidate();
                resetNavigationState();
                if (toBottom) jumpToBottom.store(false, std::memory_order_release);
                else          jumpToTop.store(false, std::memory_order_release);

                static constexpr float tolerance = 5.0f;
                const float targetOffset = toBottom
                    ? ((m_listHeight > getHeight()) ? static_cast<float>(m_listHeight - getHeight()) : 0.0f)
                    : 0.0f;

                // Find the edge focusable item
                size_t edgeFocusableIndex = m_items.size();
                if (toBottom) {
                    for (ssize_t i = static_cast<ssize_t>(m_items.size()) - 1; i >= 0; --i) {
                        if (m_items[i]->requestFocus(nullptr, FocusDirection::None)) {
                            edgeFocusableIndex = static_cast<size_t>(i);
                            break;
                        }
                    }
                } else {
                    for (size_t i = 0; i < m_items.size(); ++i) {
                        if (m_items[i]->requestFocus(nullptr, FocusDirection::None)) {
                            edgeFocusableIndex = i;
                            break;
                        }
                    }
                }

                if (edgeFocusableIndex == m_items.size()) return oldFocus;

                const bool alreadyAtEdge = (m_focusedIndex == edgeFocusableIndex) &&
                                           (std::abs(m_nextOffset - targetOffset) <= tolerance);
                if (alreadyAtEdge) return oldFocus;

                const float oldOffset = m_nextOffset;
                m_focusedIndex = edgeFocusableIndex;
                m_nextOffset   = targetOffset;

                // Check for adjacent tables and update table scrolling state
                bool hasTables = false;
                if (toBottom) {
                    for (size_t i = edgeFocusableIndex + 1; i < m_items.size(); ++i) {
                        if (m_items[i]->isTable()) { m_focusedIndex = i; hasTables = true; }
                    }
                } else if (edgeFocusableIndex > 0) {
                    for (ssize_t i = static_cast<ssize_t>(edgeFocusableIndex) - 1; i >= 0; --i) {
                        if (m_items[i]->isTable()) { m_focusedIndex = static_cast<size_t>(i); hasTables = true; }
                    }
                }

                if (hasTables && m_listHeight > getHeight()) {
                    float itemPos = 0.0f;
                    for (size_t i = 0; i < edgeFocusableIndex; ++i) itemPos += m_items[i]->getHeight();
                    const float itemCenter    = itemPos + m_items[edgeFocusableIndex]->getHeight() * 0.5f;
                    const float viewHeight    = static_cast<float>(getHeight());
                    const float viewportCenter = m_nextOffset + (viewHeight * 0.5f + VIEW_CENTER_OFFSET + 0.5f);
                    isTableScrolling.store(
                        toBottom ? (itemCenter < viewportCenter - 1.0f)
                                 : (itemCenter > viewportCenter + 1.0f),
                        std::memory_order_release);
                } else {
                    isTableScrolling.store(false, std::memory_order_release);
                }

                Element* newFocus = m_items[edgeFocusableIndex]->requestFocus(oldFocus, FocusDirection::None);
                if ((newFocus && newFocus != oldFocus) || (std::abs(m_nextOffset - oldOffset) > tolerance))
                    triggerNavigationFeedback();

                return newFocus ? newFocus : oldFocus;
            }

            Element* handleJumpToBottom(Element* oldFocus) { return handleJumpToEdge(oldFocus, true);  }
            Element* handleJumpToTop   (Element* oldFocus) { return handleJumpToEdge(oldFocus, false); }
            

            // Unified page-skip: skipDown=true → skip down, false → skip up
            Element* handleSkip(Element* oldFocus, bool skipDown) {
                if (m_items.empty()) return oldFocus;

                invalidate();
                resetNavigationState();

                const float viewHeight  = static_cast<float>(getHeight());
                const float maxOffset   = (m_listHeight > viewHeight) ? static_cast<float>(m_listHeight - viewHeight) : 0.0f;
                static constexpr float tolerance = 0.0f;

                // Find the edge focusable item for the "already there" check
                size_t edgeFocusableIndex = m_items.size();
                if (skipDown) {
                    for (ssize_t i = static_cast<ssize_t>(m_items.size()) - 1; i >= 0; --i) {
                        if (m_items[i]->requestFocus(nullptr, FocusDirection::None)) {
                            edgeFocusableIndex = static_cast<size_t>(i); break;
                        }
                    }
                    const bool alreadyAtEdge = (edgeFocusableIndex < m_items.size()) &&
                                               (m_focusedIndex == edgeFocusableIndex) &&
                                               (std::abs(m_nextOffset - maxOffset) <= tolerance);
                    if (alreadyAtEdge) return oldFocus;
                } else {
                    for (size_t i = 0; i < m_items.size(); ++i) {
                        if (m_items[i]->requestFocus(nullptr, FocusDirection::None)) {
                            edgeFocusableIndex = i; break;
                        }
                    }
                    const bool alreadyAtEdge = (edgeFocusableIndex < m_items.size()) &&
                                               (m_focusedIndex == edgeFocusableIndex) &&
                                               (std::abs(m_nextOffset - 0.0f) <= tolerance);
                    if (alreadyAtEdge) return oldFocus;
                }

                // Calculate target viewport
                const float targetViewportTop = skipDown
                    ? std::min(m_offset + viewHeight, maxOffset)
                    : std::max(0.0f, m_offset - viewHeight);

                const float actualTravelDistance = skipDown
                    ? (targetViewportTop - m_offset)
                    : (m_offset - targetViewportTop);
                const bool traveledFullViewport = (actualTravelDistance >= viewHeight - tolerance);
                const float targetViewportCenter = targetViewportTop + (viewHeight * 0.5f + VIEW_CENTER_OFFSET);

                // Find the focusable item closest to the target viewport center
                float itemTop = 0.0f;
                size_t targetIndex = 0;
                bool foundFocusable = false;
                float bestDistance = std::numeric_limits<float>::max();

                for (size_t i = 0; i < m_items.size(); ++i) {
                    const float itemHeight = m_items[i]->getHeight();
                    const float itemCenter = itemTop + itemHeight * 0.5f;
                    const float dist = std::abs(itemCenter - targetViewportCenter);
                    Element* test = m_items[i]->requestFocus(nullptr, FocusDirection::None);
                    if (test && test->m_isItem && dist < bestDistance) {
                        targetIndex = i; bestDistance = dist; foundFocusable = true;
                    }
                    itemTop += itemHeight;
                }

                const float oldOffset = m_nextOffset;

                if (foundFocusable) {
                    bool nearEdge = true;
                    const bool movedPastFocus = skipDown ? (targetIndex > m_focusedIndex)
                                                         : (targetIndex < m_focusedIndex);
                    if (movedPastFocus && traveledFullViewport) {
                        m_focusedIndex = targetIndex;
                        nearEdge = false;
                    }
                    isTableScrolling.store(false, std::memory_order_release);
                    updateScrollOffset();

                    Element* newFocus = m_items[targetIndex]->requestFocus(oldFocus, FocusDirection::None);
                    if (newFocus && newFocus != oldFocus && !nearEdge && traveledFullViewport) {
                        triggerNavigationFeedback();
                        return newFocus;
                    } else {
                        return handleJumpToEdge(oldFocus, skipDown);
                    }
                } else {
                    // No focusable items — scroll viewport and update focus to nearest visible item
                    isTableScrolling.store(true, std::memory_order_release);
                    m_nextOffset = targetViewportTop;

                    if (std::abs(m_nextOffset - oldOffset) > 0.0f)
                        triggerNavigationFeedback();

                    float searchItemTop = 0.0f;
                    size_t bestVisible = m_focusedIndex;

                    for (size_t i = 0; i < m_items.size(); ++i) {
                        const float itemHeight  = m_items[i]->getHeight();
                        const float itemBottom  = searchItemTop + itemHeight;
                        const bool inViewport   = itemBottom > targetViewportTop &&
                                                  searchItemTop < targetViewportTop + viewHeight;

                        if (skipDown) {
                            // Wants the LAST visible focusable
                            if (searchItemTop >= targetViewportTop + viewHeight) break;
                            if (inViewport) {
                                Element* test = m_items[i]->requestFocus(nullptr, FocusDirection::None);
                                if (test && test->m_isItem) bestVisible = i;
                            }
                        } else {
                            // Wants the FIRST visible focusable
                            if (inViewport) {
                                Element* test = m_items[i]->requestFocus(nullptr, FocusDirection::None);
                                if (test && test->m_isItem) { bestVisible = i; break; }
                            }
                        }
                        searchItemTop += itemHeight;
                    }

                    if (bestVisible != m_focusedIndex) {
                        m_focusedIndex = bestVisible;
                        Element* newFocus = m_items[m_focusedIndex]->requestFocus(oldFocus, FocusDirection::None);
                        if (newFocus && newFocus != oldFocus) {
                            triggerNavigationFeedback();
                            return newFocus;
                        }
                    }
                }

                return oldFocus;
            }

            Element* handleSkipDown(Element* oldFocus) { return handleSkip(oldFocus, true);  }
            Element* handleSkipUp  (Element* oldFocus) { return handleSkip(oldFocus, false); }
            
                        
            inline void initializePrefixSums() {
                prefixSums.clear();
                prefixSums.resize(m_items.size() + 1, 0.0f);
                
                for (size_t i = 1; i < prefixSums.size(); ++i) {
                    prefixSums[i] = prefixSums[i - 1] + m_items[i - 1]->getHeight();
                }
            }
            
            
            virtual void updateScrollOffset() {
                if (Element::getInputMode() != InputMode::Controller) return;
                
                if (m_listHeight <= getHeight()) {
                    m_nextOffset = m_offset = 0;
                    return;
                }
                
                // Calculate position of focused item
                float itemPos = 0.0f;
                for (size_t i = 0; i < m_focusedIndex && i < m_items.size(); ++i) {
                    itemPos += m_items[i]->getHeight();
                }
                
                // Get the focused item's height
                const float itemHeight = (m_focusedIndex < m_items.size()) ? m_items[m_focusedIndex]->getHeight() : 0.0f;
                
                // Calculate viewport height
                const float viewHeight = static_cast<float>(getHeight());
                
                // FIXED: Special handling for items near the bottom
                const float maxOffset = static_cast<float>(m_listHeight - getHeight());

                // For middle items, use centering logic
                const float itemCenterPos = itemPos + (itemHeight / 2.0f);
                const float viewportCenter = viewHeight / 2.0f + VIEW_CENTER_OFFSET + 0.5f; // add slight offset
                
                // Clamp to valid scroll bounds
                const float idealOffset = std::max(0.0f, std::min(itemCenterPos - viewportCenter, maxOffset));
                
                // Set target for smooth animation
                m_nextOffset = idealOffset;
            }
            
        };


        

        /**
         * @brief A item that goes into a list
         *
         */
        class ListItem : public Element {
        public:
            u32 width, height;
            u64 m_touchStartTime_ns;
            bool isLocked = false;
            bool m_touched = false;

            u64 m_shortHoldKey = KEY_Y;
            u64 m_longHoldKey = KEY_X;

            ListItem(const std::string& text, const std::string& value = "", bool isMini = false)
                : Element(), m_text(text), m_value(value), m_listItemHeight(isMini ? tsl::style::MiniListItemDefaultHeight : tsl::style::ListItemDefaultHeight) {
                m_isItem = true;
                m_flags.m_useClickAnimation = true;
                m_text_clean = m_text;
                ult::removeTag(m_text_clean);
                applyInitialTranslations();
                if (!value.empty()) applyInitialTranslations(true);
            }
            
        
            virtual ~ListItem() = default;
        
            virtual void draw(gfx::Renderer *renderer) override {
                const bool useClickTextColor = m_touched && Element::getInputMode() == InputMode::Touch && ult::touchInBounds;
                
                if (useClickTextColor && !m_flags.m_isTouchHolding) [[unlikely]]
                    renderer->drawRectAdaptive(this->getX() + 4, this->getY(), this->getWidth() - 8, this->getHeight(), aWithOpacity(clickColor));
                
                #if IS_LAUNCHER_DIRECTIVE

                if (m_flags.m_isTouchHolding) [[unlikely]] {
                    // Determine the active percentage to use
                    const float activePercentage = ult::displayPercentage.load(std::memory_order_acquire);
                    if (activePercentage > 0){
                        renderer->drawRectAdaptive(this->getX() + 4, this->getY(), (this->getWidth()- 12 +4)*(activePercentage * 0.01f), this->getHeight(), aWithOpacity(progressColor)); // Direct percentage conversion
                    }
                }
                #endif

                const s16 yOffset = ((tsl::style::ListItemDefaultHeight - m_listItemHeight) >> 1) + 1;
        
                if (!m_maxWidth) [[unlikely]] {
                    calculateWidths(renderer);
                }
        
                // Optimized separator drawing
                const float topBound = this->getTopBound();
                const float bottomBound = this->getBottomBound();
                static float lastBottomBound = 0.0f;
                
                if (lastBottomBound != topBound) [[unlikely]] {
                    renderer->drawRect(this->getX() + 4, topBound, this->getWidth() + 10, 1, a(separatorColor));
                }
                renderer->drawRect(this->getX() + 4, bottomBound, this->getWidth() + 10, 1, a(separatorColor));
                lastBottomBound = bottomBound;
            
            #if IS_LAUNCHER_DIRECTIVE
                static const std::vector<std::string> specialChars = {ult::STAR_SYMBOL};
            #else
                static const std::vector<std::string> specialChars = s_dividerSpecialChars;
            #endif
                // Fast path for non-truncated text
                if (!m_flags.m_truncated) [[likely]] {
                    const Color textColor = m_focused
                        ? (!ult::useSelectionText
                            ? (m_flags.m_hasCustomTextColor ? m_customTextColor : defaultTextColor)
                            : (useClickTextColor
                                ? clickTextColor
                                : selectedTextColor))
                        : (m_flags.m_hasCustomTextColor
                            ? m_customTextColor
                            : (useClickTextColor
                                ? clickTextColor
                                : defaultTextColor));
                #if IS_LAUNCHER_DIRECTIVE
                    renderer->drawStringWithColoredSections(m_text_clean, false, specialChars, this->getX() + 19, this->getY() + 45 - yOffset, 23,
                        textColor, m_focused ? starColor : selectionStarColor);
                #else
                    renderer->drawStringWithColoredSections(m_text_clean, false, specialChars, this->getX() + 19, this->getY() + 45 - yOffset, 23,
                        textColor, textSeparatorColor);
                #endif
                } else {
                    drawTruncatedText(renderer, yOffset, useClickTextColor, specialChars);
                }
        
                if (!m_value.empty()) [[likely]] {
                    drawValue(renderer, yOffset, useClickTextColor);
                }
            }
        
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                this->setBoundaries(this->getX() + 3, this->getY(), this->getWidth() + 9, m_listItemHeight);
            }
        
            virtual bool onClick(u64 keys) override {
                if (keys & KEY_A) [[likely]] {
                    
                    if (!isLocked) {
                        triggerRumbleClick.store(true, std::memory_order_release);
                        if (m_value.find(ult::CAPITAL_ON_STR) != std::string::npos)
                            triggerOffSound.store(true, std::memory_order_release);
                        else if (m_value.find(ult::CAPITAL_OFF_STR) != std::string::npos)
                            triggerOnSound.store(true, std::memory_order_release);
                        else
                            triggerEnterSound.store(true, std::memory_order_release);
                        signalFeedback();
                    } else {
                        triggerWallFeedback(true);
                    }
                    
                    if (m_flags.m_useClickAnimation)
                        triggerClickAnimation();
                } else if (keys & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) [[unlikely]] {
                    m_clickAnimationProgress = 0;
                }

                return Element::onClick(keys);
            }
        
            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) override {
                if (event == TouchEvent::Touch) [[likely]] {
                    if ((m_touched = inBounds(currX, currY))) [[likely]] {
                        m_touchStartTime_ns = ult::nowNs();
                        m_flags.m_isTouchHolding = false;  // Will be set to true when hold activates
                        m_flags.m_shortThresholdCrossed = false;
                        m_flags.m_longThresholdCrossed = false;
                        triggerNavigationFeedback();
                    }
                }
                
                if (event == TouchEvent::Hold && m_touched) [[likely]] {
                    const u64 touchDuration_ns = ult::nowNs() - m_touchStartTime_ns;
                    const float touchDurationInSeconds = static_cast<float>(touchDuration_ns) * 1e-9f;
                    
                    // Activate touch hold immediately when Hold event fires
                    if (m_flags.m_usingTouchHolding && !m_flags.m_isTouchHolding && touchDurationInSeconds >= 0.1f) {
                        m_flags.m_isTouchHolding = true;
                        // Trigger the click with KEY_A to start hold behavior
                        
                        return onClick(KEY_A);
                    }
                    
                    if (m_flags.m_useLongThreshold && !m_flags.m_longThresholdCrossed && touchDurationInSeconds >= 1.0f) [[unlikely]] {
                        m_flags.m_longThresholdCrossed = true;
                        triggerRumbleClick.store(true, std::memory_order_release);
                        signalFeedback();
                    } else if (m_flags.m_useShortThreshold && !m_flags.m_shortThresholdCrossed && touchDurationInSeconds >= 0.5f) [[unlikely]] {
                        m_flags.m_shortThresholdCrossed = true;
                        triggerRumbleClick.store(true, std::memory_order_release);
                        signalFeedback();
                    }
                    
                    return true;  // Keep handling hold
                }
            
                if (event == TouchEvent::Release && m_touched) [[likely]] {
                    m_touched = false;
                    const bool wasHolding = m_flags.m_isTouchHolding;
                    m_flags.m_isTouchHolding = false;  // Stop tracking hold on release
                    
                    if (Element::getInputMode() == InputMode::Touch) [[likely]] {
                        m_clickAnimationProgress = 0;
                        // Only trigger normal click if we weren't in a hold
                        if (!wasHolding) {
                            tsl::shiftItemFocus(this);
                            return onClick(determineKeyOnTouchRelease());
                        }
                    }
                }
                return false;
            }
            
            virtual void setFocused(bool state) override {
                if (state != m_focused) [[likely]] {
                    m_flags.m_scroll = false;
                    m_scrollOffset = 0;
                    timeIn_ns = ult::nowNs();
                    Element::setFocused(state);
                }
            }
        
            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) override {
                return this;
            }
        
            inline void setText(const std::string& text) {
                if (m_text_clean != text) [[likely]] {
                    m_text = text;
                    m_text_clean = m_text;
                    if (!m_flags.m_keepTag) ult::removeTag(m_text_clean);
                    resetTextProperties();
                    applyInitialTranslations();
                }
            }
        
            inline void setValue(const std::string& value, bool faint = false) {
                if (m_value != value || m_flags.m_faint != faint) [[likely]] {
                    m_value = value;
                    m_flags.m_faint = faint;
                    m_maxWidth = 0;
                    if (!value.empty()) applyInitialTranslations(true);
                }
            }
            
            inline void setTextColor(Color color) {
                m_customTextColor = color;
                m_flags.m_hasCustomTextColor = true;
            }
            
            inline void setValueColor(Color color) {
                m_customValueColor = color;
                m_flags.m_hasCustomValueColor = true;
            }
            
            inline void clearTextColor() {
                m_flags.m_hasCustomTextColor = false;
            }
            
            inline void clearValueColor() {
                m_flags.m_hasCustomValueColor = false;
            }

            inline void disableClickAnimation() {
                m_flags.m_useClickAnimation = false;
            }

            inline void enableClickAnimation() {
                m_flags.m_useClickAnimation = true;
            }

            inline void enableShortHoldKey() {
                m_flags.m_useShortThreshold = true;
            }

            inline void disableShortHoldKey() {
                m_flags.m_useShortThreshold = false;
            }

            inline void enableLongHoldKey() {
                m_flags.m_useLongThreshold = true;
            }

            inline void disableLongHoldKey() {
                m_flags.m_useLongThreshold = false;
            }

            inline void enableTouchHolding() {
                m_flags.m_usingTouchHolding = true;
            }

            inline void disableTouchHolding() {
                m_flags.m_usingTouchHolding = false;
            }

            inline bool isTouchHolding() const noexcept {
                return m_flags.m_isTouchHolding;
            }
            
            inline void resetTouchHold() {
                m_flags.m_isTouchHolding = false;
            }

            inline void setKeepTag(bool keep) {
                m_flags.m_keepTag = keep;
                setText(m_text);
            }
            
            inline const std::string& getText() const noexcept {
                return m_text;
            }
        
            inline const std::string& getValue() const noexcept {
                return m_value;
            }

            virtual bool matchesJumpCriteria(const std::string& jumpText, const std::string& jumpValue, bool exactMatch=true) const {
                if (jumpText.empty() && jumpValue.empty()) return false;
                
                bool textMatches, valueMatches;
                if (exactMatch) {
                    textMatches = (m_text == jumpText);
                    valueMatches = (m_value == jumpValue);
                } else { // contains check
                    textMatches = (m_text.find(jumpText) != std::string::npos);
                    valueMatches = (m_value.find(jumpValue) != std::string::npos);
                }
                
                if (jumpText.empty() && !jumpValue.empty())
                    return valueMatches;
                else if (!jumpText.empty() && jumpValue.empty())
                    return textMatches;

                return (textMatches && valueMatches);
            }
        
        protected:
            u64 timeIn_ns;
            std::string m_text;
            std::string m_text_clean;
            std::string m_value;
            std::string m_scrollText;
            std::string m_ellipsisText;
            u16 m_listItemHeight;  // Changed from u32 to u16
            
            // Bitfield for boolean flags - saves ~7 bytes per instance
            struct {
                bool m_keepTag : 1;
                bool m_scroll : 1;
                bool m_truncated : 1;
                bool m_faint : 1;
                bool m_hasCustomTextColor : 1;
                bool m_hasCustomValueColor : 1;
                bool m_useClickAnimation : 1;
                bool m_useShortThreshold : 1;
                bool m_useLongThreshold : 1;
                bool m_usingTouchHolding : 1;
                bool m_isTouchHolding: 1;
                bool m_shortThresholdCrossed : 1;
                bool m_longThresholdCrossed : 1;
            } m_flags = {};
        
            Color m_customTextColor;
            Color m_customValueColor;
        
            float m_scrollOffset = 0.0f;
            u16 m_maxWidth = 0;     // Changed from u32 to u16
            u16 m_textWidth = 0;     // Changed from u32 to u16
        
        private:
            // Consolidated scroll constants struct
            struct ScrollConstants {
                double totalCycleDuration;
                double delayDuration;
                double scrollDuration;
                double accelTime;
                double constantVelocityTime;
                double maxVelocity;
                double accelDistance;
                double constantVelocityDistance;
                double minScrollDistance;
                double invAccelTime;
                double invDecelTime;
                double invBillion;
                bool initialized = false;
            };
            
            void applyInitialTranslations(bool isValue = false) {
                std::string& target = isValue ? m_value : m_text_clean;
                ult::applyLangReplacements(target, isValue);
                ult::convertComboToUnicode(target);
                
                {
                    const std::string originalKey = target;
                    
                    std::shared_lock<std::shared_mutex> readLock(tsl::gfx::s_translationCacheMutex);
                    auto translatedIt = ult::translationCache.find(originalKey);
                    if (translatedIt != ult::translationCache.end()) {
                        target = translatedIt->second;
                    } else {
                        readLock.unlock();
                        std::unique_lock<std::shared_mutex> writeLock(tsl::gfx::s_translationCacheMutex);
                        
                        translatedIt = ult::translationCache.find(originalKey);
                        if (translatedIt != ult::translationCache.end()) {
                            target = translatedIt->second;
                        } else {
                            ult::translationCache[originalKey] = originalKey;
                        }
                    }
                }
            }
        
            void calculateWidths(gfx::Renderer* renderer) {
                if (m_value.empty()) {
                    m_maxWidth = getWidth() - 62;
                } else {
                    m_maxWidth = getWidth() - renderer->getTextDimensions(m_value, false, 20).first - 66;
                }
            
                const u16 width = renderer->getTextDimensions(m_text_clean, false, 23).first;
                m_flags.m_truncated = width > m_maxWidth + 20;
            
                if (m_flags.m_truncated) [[unlikely]] {
                    m_scrollText.clear();
                    m_scrollText.reserve(m_text_clean.size() * 2 + 8);
                    
                    m_scrollText.append(m_text_clean).append("        ");
                    m_textWidth = renderer->getTextDimensions(m_scrollText, false, 23).first;
                    m_scrollText.append(m_text_clean);
                    
                    m_ellipsisText = renderer->limitStringLength(m_text_clean, false, 23, m_maxWidth);
                } else {
                    m_textWidth = width;
                }
            }
        
            void drawTruncatedText(gfx::Renderer* renderer, s32 yOffset, bool useClickTextColor, const std::vector<std::string>& specialSymbols = {}) {
                if (m_focused) {
                    renderer->enableScissoring(getX() + 6, 97, m_maxWidth + (m_value.empty() ? 49 : 27), tsl::cfg::FramebufferHeight - 170);
                #if IS_LAUNCHER_DIRECTIVE
                    renderer->drawStringWithColoredSections(m_scrollText, false, specialSymbols, getX() + 19 - static_cast<s32>(m_scrollOffset), getY() + 45 - yOffset, 23,
                        !ult::useSelectionText ? (m_flags.m_hasCustomTextColor ? m_customTextColor : defaultTextColor): (useClickTextColor ? clickTextColor : selectedTextColor), starColor);
                #else
                    renderer->drawStringWithColoredSections(m_scrollText, false, specialSymbols, getX() + 19 - static_cast<s32>(m_scrollOffset), getY() + 45 - yOffset, 23,
                        !ult::useSelectionText ? (m_flags.m_hasCustomTextColor ? m_customTextColor : defaultTextColor): (useClickTextColor ? clickTextColor : selectedTextColor), textSeparatorColor);
                #endif
                    renderer->disableScissoring();
                    handleScrolling();
                } else {
                #if IS_LAUNCHER_DIRECTIVE
                    renderer->drawStringWithColoredSections(m_ellipsisText, false, specialSymbols, getX() + 19, getY() + 45 - yOffset, 23,
                        m_flags.m_hasCustomTextColor ? m_customTextColor : (useClickTextColor ? clickTextColor : defaultTextColor), starColor);
                #else
                    renderer->drawStringWithColoredSections(m_ellipsisText, false, specialSymbols, getX() + 19, getY() + 45 - yOffset, 23,
                        m_flags.m_hasCustomTextColor ? m_customTextColor : (useClickTextColor ? clickTextColor : defaultTextColor), textSeparatorColor);
                #endif
                }
            }
                    
            void handleScrolling() {
                static ScrollConstants sc;
                static u64 lastUpdateTime = 0;
                static float cachedScrollOffset = 0.0f;
                
                const u64 currentTime_ns = ult::nowNs();
                const u64 elapsed_ns = currentTime_ns - timeIn_ns;
                
                if (!sc.initialized || sc.minScrollDistance != static_cast<double>(m_textWidth)) {
                    sc.delayDuration = 2.0;
                    static constexpr double pauseDuration = 1.0;
                    sc.maxVelocity = 166.0;
                    sc.accelTime = 0.5;
                    static constexpr double decelTime = 0.5;
                    
                    sc.minScrollDistance = static_cast<double>(m_textWidth);
                    sc.accelDistance = 0.5 * sc.maxVelocity * sc.accelTime;
                    const double decelDistance = 0.5 * sc.maxVelocity * decelTime;
                    sc.constantVelocityDistance = std::max(0.0, sc.minScrollDistance - sc.accelDistance - decelDistance);
                    sc.constantVelocityTime = sc.constantVelocityDistance / sc.maxVelocity;
                    sc.scrollDuration = sc.accelTime + sc.constantVelocityTime + decelTime;
                    sc.totalCycleDuration = sc.delayDuration + sc.scrollDuration + pauseDuration;
                    
                    sc.invAccelTime = 1.0 / sc.accelTime;
                    sc.invDecelTime = 1.0 / decelTime;
                    sc.invBillion = 1.0 / 1000000000.0;
                    
                    sc.initialized = true;
                }
                
                const double elapsed_seconds = static_cast<double>(elapsed_ns) * sc.invBillion;
                
                if (currentTime_ns - lastUpdateTime >= 8333333ULL) {
                    const double cyclePosition = std::fmod(elapsed_seconds, sc.totalCycleDuration);
                    
                    if (cyclePosition < sc.delayDuration) [[likely]] {
                        cachedScrollOffset = 0.0f;
                    } else if (cyclePosition < sc.delayDuration + sc.scrollDuration) [[likely]] {
                        const double scrollTime = cyclePosition - sc.delayDuration;
                        double distance;
                        
                        if (scrollTime <= sc.accelTime) {
                            const double t = scrollTime * sc.invAccelTime;
                            const double smoothT = t * t;
                            distance = smoothT * sc.accelDistance;
                        } else if (scrollTime <= sc.accelTime + sc.constantVelocityTime) {
                            const double constantTime = scrollTime - sc.accelTime;
                            distance = sc.accelDistance + (constantTime * sc.maxVelocity);
                        } else {
                            const double decelStartTime = sc.accelTime + sc.constantVelocityTime;
                            const double t = (scrollTime - decelStartTime) * sc.invDecelTime;
                            const double oneMinusT = 1.0 - t;
                            const double smoothT = 1.0 - oneMinusT * oneMinusT;
                            distance = sc.accelDistance + sc.constantVelocityDistance + (smoothT * (sc.minScrollDistance - sc.accelDistance - sc.constantVelocityDistance));
                        }
                        
                        cachedScrollOffset = static_cast<float>(distance < sc.minScrollDistance ? distance : sc.minScrollDistance);
                    } else [[unlikely]] {
                        cachedScrollOffset = static_cast<float>(m_textWidth);
                    }
                    
                    lastUpdateTime = currentTime_ns;
                }
                
                m_scrollOffset = cachedScrollOffset;
                
                if (elapsed_seconds >= sc.totalCycleDuration) [[unlikely]] {
                    timeIn_ns = currentTime_ns;
                }
            }
                    
            void drawValue(gfx::Renderer* renderer, s32 yOffset, bool useClickTextColor) {
                const s32 xPosition = getX() + m_maxWidth + 47;
                const s32 yPosition = getY() + 45 - yOffset-1;
                static constexpr s32 fontSize = 20;
            
            #if IS_LAUNCHER_DIRECTIVE
                static bool lastRunningInterpreter = false;
                const auto textColor = determineValueTextColor(useClickTextColor, lastRunningInterpreter);
        
                if (m_value != ult::INPROGRESS_SYMBOL) [[likely]] {
                    renderer->drawStringWithColoredSections(m_value, false, s_dividerSpecialChars, xPosition, yPosition, fontSize, textColor, textSeparatorColor);
                } else {
                    drawThrobber(renderer, xPosition, yPosition, fontSize, textColor);
                }
                lastRunningInterpreter = ult::runningInterpreter.load(std::memory_order_acquire);
            #else
                const auto textColor = determineValueTextColor(useClickTextColor);
                if (m_value != ult::INPROGRESS_SYMBOL) [[likely]] {
                    renderer->drawStringWithColoredSections(m_value, false, s_dividerSpecialChars, xPosition, yPosition, fontSize, textColor, textSeparatorColor);
                } else {
                    drawThrobber(renderer, xPosition, yPosition, fontSize, textColor);
                }
            #endif
            }
        
        #if IS_LAUNCHER_DIRECTIVE
            Color determineValueTextColor(bool useClickTextColor, bool lastRunningInterpreter = false) const {
        #else
            Color determineValueTextColor(bool useClickTextColor) const {
        #endif
                if (m_focused && ult::useSelectionValue) {
                    if (m_value == ult::DROPDOWN_SYMBOL || m_value == ult::OPTION_SYMBOL) {
                        return useClickTextColor ? clickTextColor :
                               (m_flags.m_faint ? offTextColor : (ult::useSelectionText ? selectedTextColor : defaultTextColor));
                    }
                    // unique to focused: falls through to shared block below, but returns selectedValueTextColor at end
                } else {
                    if (m_flags.m_hasCustomValueColor) return m_customValueColor;
                    if (m_value == ult::DROPDOWN_SYMBOL || m_value == ult::OPTION_SYMBOL) {
                        return useClickTextColor ? clickTextColor :
                               (m_flags.m_faint ? offTextColor : (m_focused ? (ult::useSelectionText ? selectedTextColor : defaultTextColor) : defaultTextColor));
                    }
                }
        
                // shared logic — only reached once per path
            #if IS_LAUNCHER_DIRECTIVE
                const bool isRunning = ult::runningInterpreter.load(std::memory_order_acquire) || lastRunningInterpreter;
                if (isRunning && (m_value.find(ult::DOWNLOAD_SYMBOL) != std::string::npos ||
                                 m_value.find(ult::UNZIP_SYMBOL) != std::string::npos ||
                                 m_value.find(ult::COPY_SYMBOL) != std::string::npos))
                    return m_flags.m_faint ? offTextColor : inprogressTextColor;
            #endif
                if (m_value == ult::INPROGRESS_SYMBOL) return m_flags.m_faint ? offTextColor : inprogressTextColor;
                if (m_value == ult::CROSSMARK_SYMBOL)  return m_flags.m_faint ? offTextColor : invalidTextColor;
        
                return (m_focused && ult::useSelectionValue)
                    ? (useClickTextColor ? clickTextColor : selectedValueTextColor)
                    : (m_flags.m_faint ? offTextColor : onTextColor);
            }

            void drawThrobber(gfx::Renderer* renderer, s32 xPosition, s32 yPosition, s32 fontSize, Color textColor) {
                static size_t throbberCounter = 0;
                const auto& throbberSymbol = ult::THROBBER_SYMBOLS[(throbberCounter / 10) % ult::THROBBER_SYMBOLS.size()];
                throbberCounter = (throbberCounter + 1) % (10 * ult::THROBBER_SYMBOLS.size());
                renderer->drawString(throbberSymbol, false, xPosition, yPosition, fontSize, textColor);
            }
            
            s64 determineKeyOnTouchRelease() const {
                const u64 touchDuration_ns = ult::nowNs() - m_touchStartTime_ns;
                const float touchDurationInSeconds = static_cast<float>(touchDuration_ns) * 1e-9f;
                
                if (m_flags.m_useLongThreshold) {
                    if (touchDurationInSeconds >= 1.0f) {
                        ult::longTouchAndRelease.store(true, std::memory_order_release);
                        return m_longHoldKey;
                    }
                }
                if (m_flags.m_useShortThreshold) {
                    if (touchDurationInSeconds >= 0.5f) {
                        ult::shortTouchAndRelease.store(true, std::memory_order_release);
                        return m_shortHoldKey;
                    }
                }
                return KEY_A;
            }
        
            void resetTextProperties() {
                m_scrollText.clear();
                m_ellipsisText.clear();
                m_maxWidth = 0;
            }
        };
        
        class SilentListItem : public tsl::elm::ListItem {
        public:
            using tsl::elm::ListItem::ListItem;
            virtual bool onClick(u64 keys) override {
                // Skip all sound/rumble triggers, go straight to click listener
                if (keys & KEY_A) {
                    if (m_flags.m_useClickAnimation)
                        triggerClickAnimation();
                } else if (keys & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
                    m_clickAnimationProgress = 0;
                }
                return Element::onClick(keys);
            }
        };

        class MiniListItem : public ListItem {
        public:
            MiniListItem(const std::string& text, const std::string& value = "")
                : ListItem(text, value, true) {  // Call the parent constructor with `isMini = true`
            }
            
            // Destructor if needed (inherits default behavior from ListItem)
            virtual ~MiniListItem() {}
        };

        /**
         * @brief A wrapper item that extends ListItem with custom color support for inputs
         *        (this version uses value and faint color sourcing)
         */
        class ListItemV2 : public ListItem {
        public:
            /**
             * @brief Constructor
             *
             * @param text Initial description text
             * @param value Initial value text
             * @param valueColor Color to use for the value when not faint
             * @param faintColor Color to use for the value when faint
             * @param isMini Whether to use mini list item height
             * @param useScriptKey Whether to use script key (launcher only)
             */
            ListItemV2(const std::string& text, 
                       const std::string& value = "", 
                       Color valueColor = onTextColor, 
                       Color faintColor = offTextColor,
                       bool isMini = false)
                : ListItem(text, value, isMini),
                  m_valueColorOverride(valueColor),
                  m_faintColorOverride(faintColor),
                  m_hasColorOverrides(true) {
                
                // Set the custom value color on the base ListItem
                setValueColor(valueColor);
            }
        
            virtual ~ListItemV2() = default;
        
            /**
             * @brief Override setValue to maintain custom color behavior
             */
            inline void setValue(const std::string& value, bool faint = false) {
                // Call parent implementation
                ListItem::setValue(value, faint);
                
                // Re-apply color override based on faint state
                if (m_hasColorOverrides) {
                    setValueColor(faint ? m_faintColorOverride : m_valueColorOverride);
                }
            }
        
            /**
             * @brief Set custom value color
             */
            inline void setValueColorOverride(Color color) {
                m_valueColorOverride = color;
                m_hasColorOverrides = true;
                // Update the base class if not currently faint
                if (!m_flags.m_faint) {
                    setValueColor(color);
                }
            }
        
            /**
             * @brief Set custom faint color
             */
            inline void setFaintColorOverride(Color color) {
                m_faintColorOverride = color;
                m_hasColorOverrides = true;
                // Update the base class if currently faint
                if (m_flags.m_faint) {
                    setValueColor(color);
                }
            }
        
            /**
             * @brief Get the current value color override
             */
            inline Color getValueColorOverride() const {
                return m_valueColorOverride;
            }
        
            /**
             * @brief Get the current faint color override
             */
            inline Color getFaintColorOverride() const {
                return m_faintColorOverride;
            }
        
            /**
             * @brief Clear color overrides and revert to default behavior
             */
            inline void clearColorOverrides() {
                m_hasColorOverrides = false;
                clearValueColor();
            }
        
        protected:
            Color m_valueColorOverride;
            Color m_faintColorOverride;
            bool m_hasColorOverrides;
        };
        
        
        /**
         * @brief Mini version of ListItemV2
         */
        class MiniListItemV2 : public ListItemV2 {
        public:
            MiniListItemV2(const std::string& text, 
                           const std::string& value = "", 
                           Color valueColor = onTextColor, 
                           Color faintColor = offTextColor)
                : ListItemV2(text, value, valueColor, faintColor, true) {
            }
        
            virtual ~MiniListItemV2() {}
        };

        /**
         * @brief A toggleable list item that changes the state from On to Off when the A button gets pressed
         *
         */
        class ToggleListItem : public ListItem {
        public:
            /**
             * @brief Constructor
             *
             * @param text Initial description text
             * @param initialState Is the toggle set to On or Off initially
             * @param onValue Value drawn if the toggle is on
             * @param offValue Value drawn if the toggle is off
             */
            ToggleListItem(const std::string& text, bool initialState, const std::string& onValue = ult::ON, const std::string& offValue = ult::OFF, bool isMini = false, bool delayedHandle=false)
                : ListItem(text, "", isMini), m_state(initialState), m_onValue(onValue), m_offValue(offValue), m_delayedHandle(delayedHandle) {
                this->setState(this->m_state);
            }
            
            virtual ~ToggleListItem() {}
            
            virtual bool onClick(u64 keys) override {

                #if IS_LAUNCHER_DIRECTIVE
                if (ult::runningInterpreter.load(std::memory_order_acquire))
                    return false;
                #endif

                // Handle KEY_A for toggling
                if (keys & KEY_A) {
                    triggerRumbleClick.store(true, std::memory_order_release);
                    if (!this->m_state)
                        triggerOnSound.store(true, std::memory_order_release);
                    else
                        triggerOffSound.store(true, std::memory_order_release);
                    signalFeedback();
                    
                    
                    this->m_state = !this->m_state;
                    
                    if (!m_delayedHandle)
                        this->setState(this->m_state);
                    
                    this->m_stateChangedListener(this->m_state);
                    this->triggerClickAnimation();
                    
                    return Element::onClick(keys);
                }

                #if IS_LAUNCHER_DIRECTIVE
                // Handle SCRIPT_KEY for executing script logic
                else if (keys & SCRIPT_KEY) {
                    // Trigger the script key listener
                    if (this->m_scriptKeyListener) {
                        this->m_scriptKeyListener(this->m_state);  // Pass the current state to the script key listener
                    }
                    return ListItem::onClick(keys);
                }
                #endif
                return false;
            }
            
            /**
             * @brief Gets the current state of the toggle
             *
             * @return State
             */
            virtual bool getState() {
                return this->m_state;
            }
            
            /**
             * @brief Sets the current state of the toggle. Updates the Value
             *
             * @param state State
             */
            virtual void setState(bool state) {
                #if IS_LAUNCHER_DIRECTIVE
                if (ult::runningInterpreter.load(std::memory_order_acquire))
                    return;
                #endif

                this->m_state = state;
                this->setValue(state ? this->m_onValue : this->m_offValue, !state);
            }
            
            /**
             * @brief Adds a listener that gets called whenever the state of the toggle changes
             *
             * @param stateChangedListener Listener with the current state passed in as parameter
             */
            void setStateChangedListener(std::function<void(bool)> stateChangedListener) {
                this->m_stateChangedListener = stateChangedListener;
            }

            #if IS_LAUNCHER_DIRECTIVE
            // Attach the script key listener for SCRIPT_KEY handling
            void setScriptKeyListener(std::function<void(bool)> scriptKeyListener) {
                this->m_scriptKeyListener = scriptKeyListener;
            }
            #endif
            

        protected:
            bool m_state = true;

            std::string m_onValue, m_offValue;
            bool m_delayedHandle = false;
            
            std::function<void(bool)> m_stateChangedListener = [](bool){};

            #if IS_LAUNCHER_DIRECTIVE
            std::function<void(bool)> m_scriptKeyListener = nullptr;     // Script key listener (with state)
            #endif
        };
        
        class MiniToggleListItem : public ToggleListItem {
        public:
            // Constructor for MiniToggleListItem, with no `isMini` boolean.
            MiniToggleListItem(const std::string& text, bool initialState, const std::string& onValue = ult::ON, const std::string& offValue = ult::OFF)
                : ToggleListItem(text, initialState, onValue, offValue, true) {
            }
            
            // Destructor if needed (inherits default behavior from ListItem)
            virtual ~MiniToggleListItem() {}
        };


        class DummyListItem : public ListItem {
        public:
            DummyListItem()
                : ListItem("") { // Use an empty string for the base class constructor
                // Set the properties to indicate it's a dummy item
                this->m_text = "";
                this->m_value = "";
                this->m_maxWidth = 0;
                this->width = 0;
                this->height = 0;
                m_isItem = false;
                isLocked = true;
            }
            
            virtual ~DummyListItem() {}
            
            // Override the draw method to do nothing
            virtual void draw(gfx::Renderer* renderer) override {
                // Intentionally left blank
            }
            
            // Override the layout method to set the dimensions to zero
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                this->setBoundaries(this->getX(), this->getY(), 0, 0);
            }
            
            // Override the requestFocus method to allow this item to be focusable
            virtual Element* requestFocus(Element* oldFocus, FocusDirection direction) override {
                return this; // Allow this item to be focusable
            }
        };


        class CategoryHeader : public Element {
        public:
            CategoryHeader(const std::string &title, bool hasSeparator = true)
                : m_text(title),
                  m_value(""),
                  m_valueColor(tsl::headerTextColor),
                  m_hasSeparator(hasSeparator),
                  m_scroll(false),
                  m_truncated(false),
                  m_scrollOffset(0.0f),
                  m_maxWidth(0),
                  m_textWidth(0) {
                ult::applyLangReplacements(m_text);
                ult::convertComboToUnicode(m_text);
                m_isItem = false;
            }
        
            virtual ~CategoryHeader() {}

            // --- new setters ---
            void setValue(const std::string &value, const tsl::Color &color = tsl::headerTextColor) {
                m_value = value;
                m_valueColor = color;
            }
        
            void clearValue() {
                m_value.clear();
            }
                
            virtual void draw(gfx::Renderer* renderer) override {
                if (!m_maxWidth) calculateWidths(renderer);
            
                const int fontHeight = 16;
            
                // Keep a fixed header area for separator and text (matches old 33px height)
                const int headerTop = this->getBottomBound() - 33;
                const int textY = this->getBottomBound() - 16;
                const int textX = m_hasSeparator ? (this->getX() + 16) : this->getX();
            
                // Draw the separator rectangle on the left (fixed 22px height, 4px wide)
                if (m_hasSeparator) {
                    renderer->drawRect(
                        this->getX() + 2,
                        headerTop,
                        4,
                        22,
                        aWithOpacity(headerSeparatorColor));
                }
            
                // Draw header text
                if (m_truncated) {
                    if (!m_scroll) m_scroll = true;
                    handleScrolling();
            
                    renderer->enableScissoring(textX, ult::activeHeaderHeight-8, m_maxWidth, cfg::FramebufferHeight - 73 - (ult::activeHeaderHeight-8));
                    renderer->drawStringWithColoredSections(
                        m_scrollText, false, s_dividerSpecialChars,
                        textX - static_cast<s32>(m_scrollOffset),
                        textY,
                        fontHeight,
                        headerTextColor,
                        textSeparatorColor
                    );
                    renderer->disableScissoring();
                } else {
                    renderer->drawStringWithColoredSections(
                        m_text, false, s_dividerSpecialChars,
                        textX, textY,
                        fontHeight,
                        headerTextColor,
                        textSeparatorColor
                    );
                }
            
                // Draw optional value, right-aligned
                if (!m_value.empty()) {
                    const int valueWidth = renderer->getTextDimensions(m_value, false, fontHeight).first;
                    const int valueX = this->getX() + 2 + this->getWidth() - valueWidth;
            
                    renderer->drawString(
                        m_value,
                        false,
                        valueX,
                        textY,
                        fontHeight,
                        m_valueColor,
                        0, true, nullptr, nullptr, 0, 0, false
                    );
                }
            }
        
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                if (List *list = static_cast<List *>(this->getParent()); list != nullptr) {
                    if (list->getIndexInList(this) == 0) {
                        this->setBoundaries(this->getX(), this->getY(), this->getWidth(), 33);
                        return;
                    }
                }
        
                this->setBoundaries(
                    this->getX(),
                    this->getY(),
                    this->getWidth(),
                    tsl::style::ListItemDefaultHeight * 0.90);
            }
        
            inline void setText(const std::string &text) {
                if (m_text != text) {
                    m_text = text;
                    ult::applyLangReplacements(m_text);
                    ult::convertComboToUnicode(m_text);
        
                    resetTextProperties();
                }
            }
        
            inline const std::string &getText() const {
                return m_text;
            }
        
        private:
            std::string m_text;
            std::string m_value;
            tsl::Color m_valueColor;
            bool m_hasSeparator;
        
            bool m_scroll;
            bool m_truncated;
            float m_scrollOffset;
            u32 m_maxWidth;
            u32 m_textWidth;
        
            std::string m_scrollText;
            
            /* Delta-time animation state */
            u64 lastFrameTime_ns = 0;
            double accumulatedTime_s = 0.0;
        
            /* Cached calculations */
            bool constantsInitialized = false;
            double totalCycleDuration;
            double delayDuration;
            double scrollDuration;
            double accelTime;
            double constantVelocityTime;
            double maxVelocity;
            double accelDistance;
            double constantVelocityDistance;
            double minScrollDistance;
        
            double invAccelTime;
            double invDecelTime;
            double invBillion;
        
            float cachedScrollOffset = 0.0f;
        
            void calculateWidths(gfx::Renderer *renderer) {
                m_maxWidth = getWidth() - (m_hasSeparator ? 17 : 4);
        
                const u32 width = renderer->getTextDimensions(m_text, false, 16).first;
                m_truncated = width > m_maxWidth;
        
                if (m_truncated) {
                    m_scrollText.clear();
                    m_scrollText.reserve(m_text.size() * 2 + 8);
                    m_scrollText.append(m_text).append("        ");
                    m_textWidth = renderer->getTextDimensions(m_scrollText, false, 16).first;
                    m_scrollText.append(m_text);
                } else {
                    m_textWidth = width;
                }
        
                constantsInitialized = false;
            }
        
            void handleScrolling() {
                const u64 currentTime_ns = ult::nowNs();
        
                if (!constantsInitialized || minScrollDistance != static_cast<double>(m_textWidth)) {
                    delayDuration = 3.0;
                    static constexpr double pauseDuration = 2.0;
        
                    maxVelocity = 100.0;
                    accelTime = 0.5;
                    static constexpr double decelTime = 0.5;
        
                    minScrollDistance = static_cast<double>(m_textWidth);
                    accelDistance = 0.5 * maxVelocity * accelTime;
                    const double decelDistance = 0.5 * maxVelocity * decelTime;
        
                    constantVelocityDistance =
                        std::max(0.0, minScrollDistance - accelDistance - decelDistance);
        
                    constantVelocityTime = constantVelocityDistance / maxVelocity;
        
                    scrollDuration = accelTime + constantVelocityTime + decelTime;
                    totalCycleDuration = delayDuration + scrollDuration + pauseDuration;
        
                    invAccelTime = 1.0 / accelTime;
                    invDecelTime = 1.0 / decelTime;
                    invBillion = 1.0 / 1000000000.0;
        
                    constantsInitialized = true;
                }
        
                if (lastFrameTime_ns != 0) {
                    double delta_s =
                        static_cast<double>(currentTime_ns - lastFrameTime_ns) * invBillion;
        
                    /* Clamp large jumps (list switches / lag spikes) */
                    delta_s = std::min(delta_s, 0.05);
        
                    accumulatedTime_s += delta_s;
                }
        
                lastFrameTime_ns = currentTime_ns;
        
                const double cyclePosition =
                    std::fmod(accumulatedTime_s, totalCycleDuration);
        
                if (cyclePosition < delayDuration) {
                    cachedScrollOffset = 0.0f;
                } else if (cyclePosition < delayDuration + scrollDuration) {
                    const double scrollTime = cyclePosition - delayDuration;
                    double distance;
        
                    if (scrollTime <= accelTime) {
                        const double t = scrollTime * invAccelTime;
                        distance = (t * t) * accelDistance;
                    } else if (scrollTime <= accelTime + constantVelocityTime) {
                        const double t = scrollTime - accelTime;
                        distance = accelDistance + (t * maxVelocity);
                    } else {
                        const double decelStart = accelTime + constantVelocityTime;
                        const double t = (scrollTime - decelStart) * invDecelTime;
                        const double smooth = 1.0 - (1.0 - t) * (1.0 - t);
        
                        distance = accelDistance + constantVelocityDistance +
                                   smooth * (minScrollDistance - accelDistance - constantVelocityDistance);
                    }
        
                    cachedScrollOffset = static_cast<float>(
                        distance < minScrollDistance ? distance : minScrollDistance);
                } else {
                    cachedScrollOffset = static_cast<float>(m_textWidth);
                }
        
                m_scrollOffset = cachedScrollOffset;
            }
        
            void resetTextProperties() {
                m_scrollOffset = 0.0f;
                cachedScrollOffset = 0.0f;
        
                lastFrameTime_ns = 0;
                accumulatedTime_s = 0.0;
        
                m_maxWidth = 0;
                m_textWidth = 0;
        
                m_scroll = false;
                constantsInitialized = false;
            }
        };
        

        /**
         * @brief A customizable analog trackbar going from 0% to 100% (like the brightness slider)
         *
         */
        class TrackBar : public Element {
        public:
            /**
             * @brief Constructor
             *
             * @param icon Icon shown next to the track bar
             * @param usingStepTrackbar Whether this is a step trackbar
             * @param usingNamedStepTrackbar Whether this is a named step trackbar
             * @param useV2Style Whether to use V2 visual style (label + value instead of icon)
             * @param label Label text for V2 style
             * @param units Units text for V2 style
             */
            TrackBar(const char icon[3], bool usingStepTrackbar=false, bool usingNamedStepTrackbar = false, 
                    bool useV2Style = false, const std::string& label = "", const std::string& units = "",
                    bool unlockedTrackbar = true) 
                : m_icon(icon), m_usingStepTrackbar(usingStepTrackbar), m_usingNamedStepTrackbar(usingNamedStepTrackbar),
                  m_unlockedTrackbar(unlockedTrackbar), m_useV2Style(useV2Style), m_label(label), m_units(units) {
                m_isItem = true;
            }

            virtual ~TrackBar() {}

            virtual void triggerClickAnimation() {
                Element::triggerClickAnimation();

                // Activate the click animation
                this->m_clickAnimationStartTime = ult::nowNs();
                this->m_clickAnimationActive = true;
            }
            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) {
                return this;
            }
            
            virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
                const u64 keysReleased = m_prevKeysHeld & ~keysHeld;
                m_prevKeysHeld = keysHeld;
                
                const u64 currentTime_ns = ult::nowNs();
                
                const u64 elapsed_ns = currentTime_ns - m_lastUpdate_ns;
            
                // KEY_R + directional: shake highlight (same as V2)
                if (keysHeld & KEY_R) {
                    if (keysDown & KEY_UP && !(keysHeld & ~KEY_UP & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Up);
                    else if (keysDown & KEY_DOWN && !(keysHeld & ~KEY_DOWN & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Down);
                    else if (keysDown & KEY_LEFT && !(keysHeld & ~KEY_LEFT & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Left);
                    else if (keysDown & KEY_RIGHT && !(keysHeld & ~KEY_RIGHT & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Right);
                    return true;
                }
            
                // KEY_A: lock/unlock toggle (when locked), or click animation (when unlocked)
                if ((keysDown & KEY_A) && !(keysHeld & ~KEY_A & ALL_KEYS_MASK)) {
                    if (!m_unlockedTrackbar) {
                        ult::atomicToggle(ult::allowSlide);
                        m_holding = false;
                        if (ult::allowSlide.load(std::memory_order_acquire)) {
                            // Unlocking: rumble + on sound only, no click animation, no enter feedback
                            triggerOnFeedback();
                        } else {
                            // Locking: rumble + off sound only, no click animation
                            triggerOffFeedback();
                        }
                    } else {
                        // Always-unlocked trackbar: full click animation + enter feedback
                        this->triggerClickAnimation();
                        triggerEnterFeedback();
                    }
                    return true;
                }
            
                // Guard all movement behind lock state
                if (!m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire)) {
                    return false;
                }
            
                static s16 lastHapticSegment = -1;
                
                // Handle key release
                if ((keysReleased & KEY_LEFT) || (keysReleased & KEY_RIGHT)) {
                    lastHapticSegment = -1;
                    
                    if (m_wasLastHeld) {
                        m_wasLastHeld = false;
                        m_holding = false;
                        m_lastUpdate_ns = currentTime_ns;
                        return true;
                    } else if (m_holding) {
                        m_holding = false;
                        m_lastUpdate_ns = currentTime_ns;
                        return true;
                    }
                }
                
                // Ignore simultaneous left+right
                if (keysHeld & KEY_LEFT && keysHeld & KEY_RIGHT)
                    return true;
            
                // Handle initial key press
                const s16 sliderMax = m_maxValue - m_minValue;

                if (keysDown & KEY_LEFT || keysDown & KEY_RIGHT) {
                    triggerRumbleClick.store(true, std::memory_order_release);
                    signalFeedback();

                    m_holding = true;
                    m_wasLastHeld = false;
                    m_holdStartTime_ns = currentTime_ns;
                    m_lastUpdate_ns = currentTime_ns;
                    if (keysDown & KEY_LEFT && this->m_value > 0) {
                        this->m_value--;
                        this->m_valueChangedListener(this->m_value);
                        
                        const s16 currentSegment = (this->m_value * 10) / sliderMax;
                        if (this->m_value == 0 || currentSegment != lastHapticSegment) {
                            lastHapticSegment = currentSegment;
                            triggerNavigationFeedback();
                        }
                    } else if (keysDown & KEY_RIGHT && this->m_value < sliderMax) {
                        this->m_value++;
                        this->m_valueChangedListener(this->m_value);
                        
                        const s16 currentSegment = (this->m_value * 10) / sliderMax;
                        if (this->m_value == 0 || currentSegment != lastHapticSegment) {
                            lastHapticSegment = currentSegment;
                            triggerNavigationFeedback();
                        }
                    }
                    return true;
                }
                
                // Handle continued holding with acceleration
                if (m_holding && ((keysHeld & KEY_LEFT) || (keysHeld & KEY_RIGHT))) {
                    const u64 holdDuration_ns = currentTime_ns - m_holdStartTime_ns;
            
                    static constexpr u64 initialDelay_ns = 300000000ULL;
                    static constexpr u64 initialInterval_ns = 67000000ULL;
                    static constexpr u64 shortInterval_ns = 10000000ULL;
                    static constexpr u64 transitionPoint_ns = 1000000000ULL;
            
                    if (holdDuration_ns < initialDelay_ns) {
                        return true;
                    }
                    
                    const u64 holdDurationAfterDelay_ns = holdDuration_ns - initialDelay_ns;
                    const float t = std::min(1.0f, static_cast<float>(holdDurationAfterDelay_ns) / static_cast<float>(transitionPoint_ns));
                    const u64 currentInterval_ns = static_cast<u64>((initialInterval_ns - shortInterval_ns) * (1.0f - t) + shortInterval_ns);
                    
                    if (elapsed_ns >= currentInterval_ns) {
                        if (keysHeld & KEY_LEFT && this->m_value > 0) {
                            this->m_value--;
                            this->m_valueChangedListener(this->m_value);
                            
                            const s16 currentSegment = (this->m_value * 10) / sliderMax;
                            if (this->m_value == 0 || currentSegment != lastHapticSegment) {
                                lastHapticSegment = currentSegment;
                                triggerNavigationFeedback();
                            }
                            
                            m_lastUpdate_ns = currentTime_ns;
                            m_wasLastHeld = true;
                            return true;
                        }
                        
                        if (keysHeld & KEY_RIGHT && this->m_value < sliderMax) {
                            this->m_value++;
                            this->m_valueChangedListener(this->m_value);
                            
                            const s16 currentSegment = (this->m_value * 10) / sliderMax;
                            if (this->m_value == 0 || currentSegment != lastHapticSegment) {
                                lastHapticSegment = currentSegment;
                                triggerNavigationFeedback();
                            }
                            
                            m_lastUpdate_ns = currentTime_ns;
                            m_wasLastHeld = true;
                            return true;
                        }
                    }
                } else {
                    m_holding = false;
                }
                
                return false;
            }
                        
            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) override {
                s32 trackBarLeft = this->getX() + 59;
                s32 width        = this->getWidth() - 95;

                if (m_icon[0] != '\0') {
                    const s32 iconOffset = 14 + 23;
                    trackBarLeft += iconOffset;
                    width        -= iconOffset;
                }

                const s32 trackBarRight = trackBarLeft + width;
                const s16 sliderSpan    = m_maxValue - m_minValue;  // total steps − 1
                const u16 handlePos     = (width * this->m_value) / sliderSpan;
                const s32 circleCenterX = trackBarLeft + handlePos;
                const s32 circleCenterY = this->getY() + 40 + 16 - 3 - ((!m_usingNamedStepTrackbar && !m_useV2Style) ? 11 : 0);
                static constexpr s32 circleRadius = 16;
                static bool triggerOnce = true;
                static s16 lastHapticSegment = -1;
                static bool wasOriginallyLocked = false;

                // Use initialX/Y (where the finger first landed) for the hit-test so
                // that dragging immediately tracks position — same as TrackBarV2.
                const bool touchInCircle = (std::abs(initialX - circleCenterX) <= circleRadius) &&
                                           (std::abs(initialY - circleCenterY) <= circleRadius);
                const bool currentlyInHorizontalBounds = (currX >= trackBarLeft && currX <= trackBarRight);

                // Touch start: temporarily unlock a locked slider for the drag duration.
                if (event == TouchEvent::Touch && touchInCircle) {
                    wasOriginallyLocked = !m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire);
                    if (wasOriginallyLocked)
                        ult::allowSlide.store(true, std::memory_order_release);
                }

                if (event == TouchEvent::Release) {
                    triggerOnce = true;
                    lastHapticSegment = -1;

                    if (wasOriginallyLocked) {
                        ult::allowSlide.store(false, std::memory_order_release);
                        wasOriginallyLocked = false;
                    }

                    if (touchInSliderBounds) {
                        triggerOffFeedback(true);

                        tsl::shiftItemFocus(this);
                    }

                    touchInSliderBounds = false;
                    return false;
                }

                const bool isUnlocked = m_unlockedTrackbar || ult::allowSlide.load(std::memory_order_acquire);

                if ((touchInCircle || touchInSliderBounds) && isUnlocked) {
                    if (touchInSliderBounds && !currentlyInHorizontalBounds) {
                        // Finger dragged past an edge — clamp to that extreme.
                        this->m_value = (currX > trackBarRight) ? sliderSpan : s16(0);
                        this->m_valueChangedListener(this->getProgress());
                        touchInSliderBounds = false;
                        return false;
                    }

                    if (currentlyInHorizontalBounds) {
                        touchInSliderBounds = true;
                        if (triggerOnce) {
                            triggerOnce = false;
                            triggerOnFeedback();
                        }

                        // Round to nearest step (+0.5f) exactly as TrackBarV2 does — avoids
                        // needing to drag past the midpoint before the value advances.
                        const s16 newValue = std::max(s16(0), std::min(s16(sliderSpan),
                            s16((currX - trackBarLeft) / static_cast<float>(width) * sliderSpan + 0.5f)));

                        if (newValue != this->m_value) {
                            this->m_value = newValue;
                            this->m_valueChangedListener(this->getProgress());

                            const s16 currentSegment = (newValue * 10) / sliderSpan;
                            if (newValue == 0 || currentSegment != lastHapticSegment) {
                                lastHapticSegment = currentSegment;
                                triggerNavigationFeedback();
                            }
                        }

                        return true;
                    }
                }

                return false;
            }

            

            // Define drawBar function outside the draw method
            void drawBar(gfx::Renderer *renderer, s32 x, s32 y, u16 width, Color& color, bool isRounded = true) {
                if (isRounded) {
                    renderer->drawUniformRoundedRect(x, y, width, 7, a(color));
                } else {
                    renderer->drawRect(x, y, width, 7, a(color));
                }
            }

            virtual void draw(gfx::Renderer *renderer) override {
                
                if (touchInSliderBounds) {
                    m_drawFrameless = true;
                    drawHighlight(renderer);
                } else {
                    m_drawFrameless = false;
                }
            
                s32 xPos = this->getX() + 59;
                s32 yPos = this->getY() + 40 + 16 - 3;
                s32 width = this->getWidth() - 95;
                const int maxValue = (m_usingStepTrackbar || m_usingNamedStepTrackbar) 
                                     ? ((100 / (this->m_numSteps - 1)) * (this->m_numSteps - 1))
                                     : 100;
                u16 handlePos = width * (this->m_value) / maxValue;
            
                if (!m_usingNamedStepTrackbar && !m_useV2Style) {
                    yPos -= 11;
                }
            
                s32 iconOffset = 0;
            
                if (m_icon[0] != '\0') {
                    s32 iconWidth = 23;
                    iconOffset = 14 + iconWidth;
                    xPos += iconOffset;
                    width -= iconOffset;
                    handlePos = (width) * (this->m_value) / (m_maxValue - m_minValue);
                }
            
                // Draw step tick marks if this is a step trackbar
                if (m_usingStepTrackbar || m_usingNamedStepTrackbar) {
                    const u8 numSteps = m_numSteps;
                    const u16 baseX = xPos;
                    const u16 baseY = this->getY() + 44;
                    const u8 halfNumSteps = (numSteps - 1) / 2;
                    const u16 lastStepX = baseX + width - 1;
                    const float stepSpacing = static_cast<float>(width) / (numSteps - 1);
                    const auto stepColor = a(trackBarEmptyColor);
                    
                    u16 stepX;
                    for (u8 i = 0; i < numSteps; i++) {
                        if (i == numSteps - 1) {
                            stepX = lastStepX;
                        } else {
                            stepX = baseX + static_cast<u16>(std::round(i * stepSpacing));
                            if (i > halfNumSteps) {
                                stepX -= 1;
                            }
                        }
                        renderer->drawRect(stepX, baseY, 1, 8, stepColor);
                    }
                }
            
                // Draw track bar background
                drawBar(renderer, xPos, yPos-3, width, trackBarEmptyColor, !m_usingNamedStepTrackbar);
            
                const bool isEffectivelyUnlocked = m_unlockedTrackbar || ult::allowSlide.load(std::memory_order_acquire);
            
                if (!this->m_focused) {
                    drawBar(renderer, xPos, yPos-3, handlePos, trackBarFullColor, !m_usingNamedStepTrackbar);
                    renderer->drawCircle(xPos + handlePos, yPos, 16, true, a(m_drawFrameless ? s_highlightColor : trackBarSliderBorderColor));
                    renderer->drawCircle(xPos + handlePos, yPos, 13, true, a((isEffectivelyUnlocked || touchInSliderBounds) ? trackBarSliderMalleableColor : trackBarSliderColor));
                } else {
                    touchInSliderBounds = false;
                    if (m_unlockedTrackbar != ult::unlockedSlide.load(std::memory_order_acquire))
                        ult::unlockedSlide.store(m_unlockedTrackbar, std::memory_order_release);
                    drawBar(renderer, xPos, yPos-3, handlePos, trackBarFullColor, !m_usingNamedStepTrackbar);
                    renderer->drawCircle(xPos + x + handlePos, yPos +y, 16, true, a(s_highlightColor));
                    renderer->drawCircle(xPos + x + handlePos, yPos +y, 12, true, a(isEffectivelyUnlocked ? trackBarSliderMalleableColor : trackBarSliderColor));
                }
            
                // Draw icon (always if provided), then label + value (V2 style)
                if (m_useV2Style) {
                    std::string labelPart = this->m_label;
                    ult::removeTag(labelPart);
                
                    std::string valuePart;
                    if (!m_usingNamedStepTrackbar) {
                        // m_value is the direct offset from m_minValue (derived from the range).
                        // For default (0,100): dispVal == m_value — identical to original behaviour.
                        const int dispVal = static_cast<int>(m_minValue) + static_cast<int>(m_value);
                        const std::string dispStr =
                            (m_minValue < 0 && dispVal > 0 ? std::string("+") : std::string(""))
                            + ult::to_string(dispVal);
                        valuePart = (m_units.compare("%") == 0 || m_units.compare("°C") == 0 || m_units.compare("°F") == 0)
                                    ? dispStr + m_units
                                    : dispStr + (m_units.empty() ? "" : " ") + m_units;
                    } else {
                        valuePart = this->m_selection;
                    }
                
                    const auto valueWidth = renderer->getTextDimensions(valuePart, false, 16).first;
                    const s32 labelX = xPos;
                    const s32 valueX = xPos + width - valueWidth;
            
                    renderer->drawString(labelPart, false, labelX, this->getY() + 14 + 16, 16, 
                                       ((!this->m_focused || !ult::useSelectionText) ? defaultTextColor : selectedTextColor));
                    renderer->drawString(valuePart, false, valueX, this->getY() + 14 + 16, 16, 
                                       (this->m_focused && ult::useSelectionValue) ? selectedValueTextColor : onTextColor);
            
                    if (m_icon[0] != '\0')
                        renderer->drawString(this->m_icon, false, this->getX()+42, this->getY() + 50+2+2, 30, ((!this->m_focused || !ult::useSelectionText) ? defaultTextColor : selectedTextColor));
                } else {
                    if (m_icon[0] != '\0')
                        renderer->drawString(this->m_icon, false, this->getX()+42, this->getY() + 50+2+2, 30, ((!this->m_focused || !ult::useSelectionText) ? defaultTextColor : selectedTextColor));
                }
            
                if (m_lastBottomBound != this->getTopBound())
                    renderer->drawRect(this->getX() + 4+20-1, this->getTopBound(), this->getWidth() + 6 + 10+20 +4, 1, a(separatorColor));
                renderer->drawRect(this->getX() + 4+20-1, this->getBottomBound(), this->getWidth() + 6 + 10+20 +4, 1, a(separatorColor));
                m_lastBottomBound = this->getBottomBound();
            }

            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                this->setBoundaries(this->getX() - 16 , this->getY(), this->getWidth()+20+4, tsl::style::TrackBarDefaultHeight );
            }

            virtual void drawFocusBackground(gfx::Renderer *renderer) {
                // No background drawn here in HOS
            }

            virtual void drawHighlight(gfx::Renderer *renderer) override {
                
                const u64 currentTime_ns = ult::nowNs();
                const double time_seconds = static_cast<double>(currentTime_ns) / 1000000000.0;
                progress = (ult::cos(2.0 * ult::_M_PI * std::fmod(time_seconds, 1.0) - ult::_M_PI / 2) + 1.0) / 2.0;
            
                if (m_clickAnimationActive) {
                    Color clickColor1 = highlightColor1;
                    Color clickColor2 = clickColor;
                    
                    if (progress >= 0.5) {
                        clickColor1 = clickColor;
                        clickColor2 = highlightColor2;
                    }
                    const u64 elapsedTime_ns = currentTime_ns - this->m_clickAnimationStartTime;
                    if (elapsedTime_ns < 500000000ULL) {
                        s_highlightColor = lerpColor(clickColor1, clickColor2, progress);
                    } else {
                        m_clickAnimationActive = false;
                    }
                } else {
                    // Use dim colors when locked, bright colors when unlocked
                    if (!m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire)) {
                        s_highlightColor = lerpColor(highlightColor3, highlightColor4, progress);
                    } else {
                        s_highlightColor = lerpColor(highlightColor1, highlightColor2, progress);
                    }
                }
                
                x = 0;
                y = 0;
                
                if (this->m_highlightShaking) {
                    t_ns = currentTime_ns - this->m_highlightShakingStartTime;
                    const double t_ms = t_ns / 1000000.0;
                    
                    static constexpr double SHAKE_DURATION_MS = 200.0;
                    
                    if (t_ms >= SHAKE_DURATION_MS)
                        this->m_highlightShaking = false;
                    else {
                        const double amplitude = 6.0 + ((this->m_highlightShakingStartTime / 1000000) % 5);
                        const double progress = t_ms / SHAKE_DURATION_MS;
                        const double damping = 1.0 / (1.0 + 2.5 * progress * (1.0 + 1.3 * progress));
                        const double oscillation = ult::cos(ult::_M_PI * 4.0 * progress);
                        const double displacement = amplitude * oscillation * damping;
                        const int offset = static_cast<int>(displacement);
                        
                        switch (this->m_highlightShakingDirection) {
                            case FocusDirection::Up:    y = -offset; break;
                            case FocusDirection::Down:  y = offset; break;
                            case FocusDirection::Left:  x = -offset; break;
                            case FocusDirection::Right: x = offset; break;
                            default: break;
                        }
                    }
                }
                
                if (!m_drawFrameless) {
                    if (ult::useSelectionBG) {
                        renderer->drawRectAdaptive(this->getX() + x +19, this->getY() + y, this->getWidth()-11-4, this->getHeight(), aWithOpacity(selectionBGColor));
                    }
                    renderer->drawBorderedRoundedRect(this->getX() + x +19, this->getY() + y, this->getWidth()-11, this->getHeight(), 5, 5, a(s_highlightColor));
                } else {
                    if (ult::useSelectionBG) {
                        renderer->drawRectAdaptive(this->getX() + x +19, this->getY() + y, this->getWidth()-11-4, this->getHeight(), aWithOpacity(clickColor));
                    }
                }
            
                ult::onTrackBar.exchange(true, std::memory_order_acq_rel);
                
                if (this->m_clickAnimationActive) {
                    const u64 elapsedTime_ns = currentTime_ns - this->m_clickAnimationStartTime;
            
                    auto clickAnimationProgress = tsl::style::ListItemHighlightLength * (1.0f - (static_cast<float>(elapsedTime_ns) / 500000000.0f));
                    
                    if (clickAnimationProgress < 0.0f) {
                        clickAnimationProgress = 0.0f;
                        this->m_clickAnimationActive = false;
                    }
                
                    if (clickAnimationProgress > 0.0f) {
                        const u8 saturation = tsl::style::ListItemHighlightSaturation * (float(clickAnimationProgress) / float(tsl::style::ListItemHighlightLength));
                
                        Color animColor = {0xF, 0xF, 0xF, 0xF};
                        if (invertBGClickColor) {
                            animColor.r = 15 - saturation;
                            animColor.g = 15 - saturation;
                            animColor.b = 15 - saturation;
                        } else {
                            animColor.r = saturation;
                            animColor.g = saturation;
                            animColor.b = saturation;
                        }
                        animColor.a = selectionBGColor.a;
                        renderer->drawRect(this->getX() +22, this->getY(), this->getWidth() -22, this->getHeight(), aWithOpacity(animColor));
                    }
                }
            }

            /**
             * @brief Gets the current value of the trackbar
             *
             * @return State
             */
            virtual u16 getProgress() {
                return this->m_value;
            }

            /**
             * @brief Sets the current state of the toggle. Updates the Value
             *
             * @param state State
             */
            virtual void setProgress(u16 value) {
                this->m_value = value;
            }

            /**
             * @brief Adds a listener that gets called whenever the state of the toggle changes
             *
             * @param stateChangedListener Listener with the current state passed in as parameter
             */
            void setValueChangedListener(std::function<void(u16)> valueChangedListener) {
                this->m_valueChangedListener = valueChangedListener;
            }

            /**
             * @brief Sets the display range and resolution of the slider.
             *
             * m_value internally stores the direct offset from minValue, so the
             * slider spans (maxValue − minValue) + 1 discrete positions — one per
             * unit of the range.  The displayed value is simply minValue + m_value.
             *
             * For the default range (0, 100) the behaviour is identical to the
             * original: 101 positions, 0–100 displayed, listener receives 0–100.
             *
             * Example — audio balance (−150 % … 0 % … +150 %):
             *   setRange(-150, 150);   // 301 positions, 1 % per step
             *
             * A "+" prefix is shown automatically when minValue < 0 && dispVal > 0.
             *
             * @param minValue  Displayed value at the far-left  position (m_value == 0)
             * @param maxValue  Displayed value at the far-right position (m_value == maxValue − minValue)
             */
            void setRange(s16 minValue, s16 maxValue) {
                m_minValue = minValue;
                m_maxValue = maxValue;
            }

        protected:
            const char *m_icon = nullptr;
            s16 m_value = 0;
            bool m_interactionLocked = false;

            std::function<void(u16)> m_valueChangedListener = [](u16){};

            bool m_usingStepTrackbar = false;
            bool m_usingNamedStepTrackbar = false;
            bool m_unlockedTrackbar = true;
            bool touchInSliderBounds = false;
            
            u64 m_clickAnimationStartTime = 0;
            bool m_clickAnimationActive = false;

            u8 m_numSteps = 101;
            // V2 Style properties
            bool m_useV2Style = false;
            std::string m_label;
            std::string m_units;
            std::string m_selection; // Used for named step trackbars
            bool m_drawFrameless = false;

            // Optional display-value range — does NOT affect slider position or geometry.
            // Display-value range.  m_value holds the direct offset from m_minValue,
            // so the slider spans (m_maxValue − m_minValue + 1) discrete positions.
            // dispVal = m_minValue + m_value.  A "+" prefix is shown automatically
            // when m_minValue < 0 && dispVal > 0.
            // Defaults (0, 100) are exact no-ops — all existing subclasses unchanged.
            s16 m_minValue = 0;
            s16 m_maxValue = 100;

            float m_lastBottomBound;

            s16 m_index = 0;  // Add index tracking like V2
            u64 m_holdStartTime_ns = 0;
            u64 m_lastUpdate_ns = 0;  // Per-instance hold-repeat timer (matches TrackBarV2)
            bool m_holding = false;
            bool m_wasLastHeld = false;
            u64 m_prevKeysHeld = 0;
        };


        /**
         * @brief A customizable analog trackbar going from 0% to 100% but using discrete steps (Like the volume slider)
         *
         */
        class StepTrackBar : public TrackBar {
        public:
            /**
             * @brief Constructor
             *
             * @param icon Icon shown next to the track bar
             * @param numSteps Number of steps the track bar has
             * @param usingNamedStepTrackbar Whether this is a named step trackbar
             * @param useV2Style Whether to use V2 visual style (label + value instead of icon)
             * @param label Label text for V2 style
             * @param units Units text for V2 style
             */
            StepTrackBar(const char icon[3], size_t numSteps, bool usingNamedStepTrackbar = false,
                        bool useV2Style = false, const std::string& label = "", const std::string& units = "",
                        bool unlockedTrackbar = true)
                : TrackBar(icon, true, usingNamedStepTrackbar, useV2Style, label, units, unlockedTrackbar), m_numSteps(numSteps) {}

            virtual ~StepTrackBar() {}

            virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
                const u64 keysReleased = m_prevKeysHeld & ~keysHeld;
                m_prevKeysHeld = keysHeld;
                
                const u64 currentTime_ns = ult::nowNs();
                static u64 lastUpdate_ns = currentTime_ns;
                const u64 elapsed_ns = currentTime_ns - lastUpdate_ns;
            
                // KEY_R + directional: shake highlight
                if (keysHeld & KEY_R) {
                    if (keysDown & KEY_UP && !(keysHeld & ~KEY_UP & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Up);
                    else if (keysDown & KEY_DOWN && !(keysHeld & ~KEY_DOWN & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Down);
                    else if (keysDown & KEY_LEFT && !(keysHeld & ~KEY_LEFT & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Left);
                    else if (keysDown & KEY_RIGHT && !(keysHeld & ~KEY_RIGHT & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Right);
                    return true;
                }
                
                // KEY_A: lock/unlock toggle (when locked), or click animation (when unlocked)
                if ((keysDown & KEY_A) && !(keysHeld & ~KEY_A & ALL_KEYS_MASK)) {
                    if (!m_unlockedTrackbar) {
                        ult::atomicToggle(ult::allowSlide);
                        m_holding = false;
                        if (ult::allowSlide.load(std::memory_order_acquire)) {
                            // Unlocking: rumble + on sound only, no click animation, no enter feedback
                            triggerOnFeedback();
                        } else {
                            // Locking: rumble + off sound only, no click animation
                            triggerOffFeedback();
                        }
                    } else {
                        // Always-unlocked trackbar: full click animation + enter feedback
                        this->triggerClickAnimation();
                        triggerEnterFeedback();
                    }
                    return true;
                }
            
                // Guard all movement behind lock state
                if (!m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire)) {
                    return false;
                }
            
                // Calculate actual max value based on steps
                const int stepSize = 100 / (this->m_numSteps - 1);
                const int maxValue = stepSize * (this->m_numSteps - 1);
            
                // Handle key release
                if ((keysReleased & KEY_LEFT) || (keysReleased & KEY_RIGHT)) {
                    if (m_wasLastHeld) {
                        m_wasLastHeld = false;
                        m_holding = false;
                        lastUpdate_ns = currentTime_ns;
                        return true;
                    } else if (m_holding) {
                        m_holding = false;
                        lastUpdate_ns = currentTime_ns;
                        return true;
                    }
                }
                
                // Ignore simultaneous left+right
                if (keysHeld & KEY_LEFT && keysHeld & KEY_RIGHT)
                    return true;
            
                // Handle initial key press
                if (keysDown & KEY_LEFT || keysDown & KEY_RIGHT) {
                    m_holding = true;
                    m_wasLastHeld = false;
                    m_holdStartTime_ns = currentTime_ns;
                    lastUpdate_ns = currentTime_ns;
                    
                    if (keysDown & KEY_LEFT && this->m_value > 0) {
                        triggerNavigationFeedback();
                        this->m_value = std::max(this->m_value - stepSize, 0);
                        this->m_valueChangedListener(this->getProgress());
                    } else if (keysDown & KEY_RIGHT && this->m_value < maxValue) {
                        triggerNavigationFeedback();
                        this->m_value = std::min(this->m_value + stepSize, maxValue);
                        this->m_valueChangedListener(this->getProgress());
                    }
                    return true;
                }
                
                // Handle continued holding with acceleration
                if (m_holding && ((keysHeld & KEY_LEFT) || (keysHeld & KEY_RIGHT))) {
                    const u64 holdDuration_ns = currentTime_ns - m_holdStartTime_ns;
            
                    static constexpr u64 initialDelay_ns = 300000000ULL;
                    static constexpr u64 initialInterval_ns = 67000000ULL;
                    static constexpr u64 shortInterval_ns = 10000000ULL;
                    static constexpr u64 transitionPoint_ns = 1000000000ULL;
            
                    if (holdDuration_ns < initialDelay_ns) {
                        return true;
                    }
                    
                    const u64 holdDurationAfterDelay_ns = holdDuration_ns - initialDelay_ns;
                    const float t = std::min(1.0f, static_cast<float>(holdDurationAfterDelay_ns) / static_cast<float>(transitionPoint_ns));
                    const u64 currentInterval_ns = static_cast<u64>((initialInterval_ns - shortInterval_ns) * (1.0f - t) + shortInterval_ns);
                    
                    if (elapsed_ns >= currentInterval_ns) {
                        if (keysHeld & KEY_LEFT && this->m_value > 0) {
                            triggerNavigationFeedback();
                            this->m_value = std::max(this->m_value - stepSize, 0);
                            this->m_valueChangedListener(this->getProgress());
                            lastUpdate_ns = currentTime_ns;
                            m_wasLastHeld = true;
                            return true;
                        }
                        
                        if (keysHeld & KEY_RIGHT && this->m_value < maxValue) {
                            triggerNavigationFeedback();
                            this->m_value = std::min(this->m_value + stepSize, maxValue);
                            this->m_valueChangedListener(this->getProgress());
                            lastUpdate_ns = currentTime_ns;
                            m_wasLastHeld = true;
                            return true;
                        }
                    }
                } else {
                    m_holding = false;
                }
                
                return false;
            }


            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) override {
                const int stepSize = 100 / (this->m_numSteps - 1);
                const int maxValue = stepSize * (this->m_numSteps - 1);
            
                s32 trackBarLeft = this->getX() + 59;
                s32 width        = this->getWidth() - 95;
            
                if (m_icon[0] != '\0') {
                    const s32 iconOffset = 14 + 23;
                    trackBarLeft += iconOffset;
                    width        -= iconOffset;
                }
            
                const s32 trackBarRight = trackBarLeft + width;
                const u16 handlePos     = (width * this->m_value) / maxValue;
                const s32 circleCenterX = trackBarLeft + handlePos;
                const s32 circleCenterY = this->getY() + 40 + 16 - 3 - ((!m_usingNamedStepTrackbar && !m_useV2Style) ? 11 : 0);
                static constexpr s32 circleRadius = 16;
                static bool triggerOnce = true;
            
                const bool touchInCircle = (std::abs(currX - circleCenterX) <= circleRadius) && (std::abs(currY - circleCenterY) <= circleRadius);
                const bool currentlyInHorizontalBounds = (currX >= trackBarLeft && currX <= trackBarRight);
            
                if (event == TouchEvent::Release) {
                    triggerOnce = true;
            
                    if (touchInSliderBounds) {
                        triggerOffFeedback(true);
                        tsl::shiftItemFocus(this);
                    }
            
                    touchInSliderBounds = false;
                    return false;
                }
            
                if (touchInCircle || touchInSliderBounds) {
                    if (touchInSliderBounds && !currentlyInHorizontalBounds) {
                        if (currX > trackBarRight) {
                            this->m_value = maxValue;
                        } else if (currX < trackBarLeft) {
                            this->m_value = 0;
                        }
                        this->m_valueChangedListener(this->getProgress());
            
                        touchInSliderBounds = false;
                        return false;
                    }
            
                    if (currentlyInHorizontalBounds) {
                        if (triggerOnce) {
                            triggerOnce = false;
                            triggerOnFeedback();
                        }
                        touchInSliderBounds = true;
            
                        float rawValue = (static_cast<float>(currX - trackBarLeft) / static_cast<float>(width)) * maxValue;
                        s16 newValue;
            
                        if (rawValue < 0) {
                            newValue = 0;
                        } else if (rawValue > maxValue) {
                            newValue = maxValue;
                        } else {
                            newValue = std::round(rawValue / stepSize) * stepSize;
                            newValue = std::min(std::max(newValue, s16(0)), s16(maxValue));
                        }
            
                        if (newValue != this->m_value) {
                            triggerNavigationFeedback();
                            this->m_value = newValue;
                            this->m_valueChangedListener(this->getProgress());
                        }
            
                        return true;
                    }
                }
            
                return false;
            }
            
            /**
             * @brief Gets the current value of the trackbar
             *
             * @return State
             */
            virtual u16 getProgress() override {
                return this->m_value / (100 / (this->m_numSteps - 1));
            }

            /**
             * @brief Sets the current state of the toggle. Updates the Value
             *
             * @param state State
             */
            virtual void setProgress(u16 value) override {
                value = std::min(value, u16(this->m_numSteps - 1));
                this->m_value = value * (100 / (this->m_numSteps - 1));
            }

        protected:
            u8 m_numSteps = 1;
        };


        /**
         * @brief A customizable trackbar with multiple discrete steps with specific names. Name gets displayed above the bar
         *
         */
        class NamedStepTrackBar : public StepTrackBar {
        public:
            NamedStepTrackBar(const char icon[3], std::initializer_list<std::string> stepDescriptions,
                             bool useV2Style = false, const std::string& label = "", bool unlockedTrackbar = true)
                : StepTrackBar(icon, stepDescriptions.size(), true, useV2Style, label, "", unlockedTrackbar), 
                  m_stepDescriptions(stepDescriptions.begin(), stepDescriptions.end()) {
                this->m_usingNamedStepTrackbar = true;
                m_numSteps = m_stepDescriptions.size();
                if (!m_stepDescriptions.empty()) {
                    this->m_selection = m_stepDescriptions[0];
                }
            }
        
            virtual ~NamedStepTrackBar() {}
        
            virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, 
                                    HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
                const u16 prevProgress = this->getProgress();
                const bool result = StepTrackBar::handleInput(keysDown, keysHeld, touchPos, leftJoyStick, rightJoyStick);
                
                if (this->getProgress() != prevProgress) {
                    const u16 currentIndex = this->getProgress();
                    if (currentIndex < m_stepDescriptions.size()) {
                        this->m_selection = m_stepDescriptions[currentIndex];
                    }
                }
                
                return result;
            }
        
            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, 
                                s32 initialX, s32 initialY) override {
                const u16 prevProgress = this->getProgress();
                const bool result = StepTrackBar::onTouch(event, currX, currY, prevX, prevY, initialX, initialY);
                
                if (result && this->getProgress() != prevProgress) {
                    const u16 currentIndex = this->getProgress();
                    if (currentIndex < m_stepDescriptions.size()) {
                        this->m_selection = m_stepDescriptions[currentIndex];
                    }
                }
                
                return result;
            }
        
            virtual void setProgress(u16 value) override {
                StepTrackBar::setProgress(value);
                
                const u16 currentIndex = this->getProgress();
                if (currentIndex < m_stepDescriptions.size()) {
                    this->m_selection = m_stepDescriptions[currentIndex];
                }
            }
        
            const std::string& getSelection() const {
                return this->m_selection;
            }
        
            virtual void draw(gfx::Renderer *renderer) override {
                if (touchInSliderBounds) {
                    m_drawFrameless = true;
                    drawHighlight(renderer);
                } else {
                    m_drawFrameless = false;
                }
            
                s32 xPos = this->getX() + 59;
                s32 yPos = this->getY() + 40 + 16 - 3;
                s32 width = this->getWidth() - 95;
                const int maxValue = (100 / (this->m_numSteps - 1)) * (this->m_numSteps - 1);
                u16 handlePos = width * (this->m_value) / maxValue;
                
                s32 iconOffset = 0;
            
                if (m_icon[0] != '\0') {
                    s32 iconWidth = 23;
                    iconOffset = 14 + iconWidth;
                    xPos += iconOffset;
                    width -= iconOffset;
                    handlePos = (width) * (this->m_value) / (100);
                }
            
                // Draw step tick marks
                const u8 numSteps = m_numSteps;
                const u16 baseX = xPos;
                const u16 baseY = this->getY() + 44;
                const u8 halfNumSteps = (numSteps - 1) / 2;
                const u16 lastStepX = baseX + width - 1;
                const float stepSpacing = static_cast<float>(width) / (numSteps - 1);
                const auto stepColor = a(trackBarEmptyColor);
                
                u16 stepX;
                for (u8 i = 0; i < numSteps; i++) {
                    if (i == numSteps - 1) {
                        stepX = lastStepX;
                    } else {
                        stepX = baseX + static_cast<u16>(std::round(i * stepSpacing));
                        if (i > halfNumSteps) {
                            stepX -= 1;
                        }
                    }
                    renderer->drawRect(stepX, baseY, 1, 8, stepColor);
                }
            
                // Draw track bar background
                drawBar(renderer, xPos, yPos-3, width, trackBarEmptyColor, false);
            
                const bool isEffectivelyUnlocked = m_unlockedTrackbar || ult::allowSlide.load(std::memory_order_acquire);
            
                if (!this->m_focused) {
                    drawBar(renderer, xPos, yPos-3, handlePos, trackBarFullColor, false);
                    renderer->drawCircle(xPos + handlePos, yPos, 16, true, a(m_drawFrameless ? s_highlightColor : trackBarSliderBorderColor));
                    renderer->drawCircle(xPos + handlePos, yPos, 13, true, a((isEffectivelyUnlocked || touchInSliderBounds) ? trackBarSliderMalleableColor : trackBarSliderColor));
                } else {
                    touchInSliderBounds = false;
                    if (m_unlockedTrackbar != ult::unlockedSlide.load(std::memory_order_acquire))
                        ult::unlockedSlide.store(m_unlockedTrackbar, std::memory_order_release);
                    drawBar(renderer, xPos, yPos-3, handlePos, trackBarFullColor, false);
                    renderer->drawCircle(xPos + x + handlePos, yPos +y, 16, true, a(s_highlightColor));
                    renderer->drawCircle(xPos + x + handlePos, yPos +y, 12, true, a(isEffectivelyUnlocked ? trackBarSliderMalleableColor : trackBarSliderColor));
                }
            
                if (m_useV2Style) {
                    std::string labelPart = this->m_label;
                    ult::removeTag(labelPart);
                
                    std::string valuePart = this->m_selection;
                    const auto valueWidth = renderer->getTextDimensions(valuePart, false, 16).first;
                    const s32 labelX = xPos;
                    const s32 valueX = xPos + width - valueWidth;
            
                    renderer->drawString(labelPart, false, labelX, this->getY() + 14 + 16, 16, 
                                       ((!this->m_focused || !ult::useSelectionText) ? defaultTextColor : selectedTextColor));
                    renderer->drawString(valuePart, false, valueX, this->getY() + 14 + 16, 16, 
                                       (this->m_focused && ult::useSelectionValue) ? selectedValueTextColor : onTextColor);
            
                    if (m_icon[0] != '\0')
                        renderer->drawString(this->m_icon, false, this->getX()+42, this->getY() + 50+2+2, 30, tsl::style::color::ColorText);
                } else {
                    const auto textDimensions = renderer->getTextDimensions(this->m_selection, false, 16);
                    const s32 textWidth = textDimensions.first;
                    const s32 textX = xPos + (width / 2) - (textWidth / 2);
                    const s32 textY = this->getY() + 14 + 16;
                    
                    renderer->drawString(this->m_selection.c_str(), false, textX, textY, 16, 
                                       a(this->m_focused ? tsl::style::color::ColorHighlight : tsl::style::color::ColorText));
            
                    if (m_icon[0] != '\0')
                        renderer->drawString(this->m_icon, false, this->getX()+42, this->getY() + 50+2+2, 30, tsl::style::color::ColorText);
                }
            
                // Draw separators
                if (m_lastBottomBound != this->getTopBound())
                    renderer->drawRect(this->getX() + 4+20-1, this->getTopBound(), this->getWidth() + 6 + 10+20 +4, 1, a(separatorColor));
                renderer->drawRect(this->getX() + 4+20-1, this->getBottomBound(), this->getWidth() + 6 + 10+20 +4, 1, a(separatorColor));
                m_lastBottomBound = this->getBottomBound();
            }
        
        protected:
            std::vector<std::string> m_stepDescriptions;
        };


        /**
         * @brief A customizable analog trackbar going from minValue to maxValue
         *
         */
        class TrackBarV2 : public Element {
        public:
            using SimpleValueChangeCallback = std::function<void(s16 value, s16 index)>;

            u64 lastUpdate_ns;
        
            Color highlightColor = {0xf, 0xf, 0xf, 0xf};
            float progress;
            float counter = 0.0;
            s32 x, y;
            s32 amplitude;
            u32 descWidth, descHeight;
            
            void setScriptKeyListener(std::function<void()> listener) {
                m_scriptKeyListener = std::move(listener);
            }
        
            TrackBarV2(std::string label, std::string packagePath = "", s16 minValue = 0, s16 maxValue = 100, std::string units = "",
                     std::function<bool(std::vector<std::vector<std::string>>&&, const std::string&, const std::string&)> executeCommands = nullptr,
                     std::function<std::vector<std::vector<std::string>>(const std::vector<std::vector<std::string>>&, const std::string&, size_t, const std::string&)> sourceReplacementFunc = nullptr,
                     std::vector<std::vector<std::string>> cmd = {}, const std::string& selCmd = "", bool usingStepTrackbar = false, bool usingNamedStepTrackbar = false, s16 numSteps = -1, bool unlockedTrackbar = false, bool executeOnEveryTick = false)
                : m_label(label), m_packagePath(packagePath), m_minValue(minValue), m_maxValue(maxValue), m_units(units),
                  interpretAndExecuteCommands(executeCommands), getSourceReplacement(sourceReplacementFunc), commands(std::move(cmd)), selectedCommand(selCmd),
                  m_usingStepTrackbar(usingStepTrackbar), m_usingNamedStepTrackbar(usingNamedStepTrackbar), m_numSteps(numSteps), m_unlockedTrackbar(unlockedTrackbar), m_executeOnEveryTick(executeOnEveryTick) {
                
                m_isItem = true;
            
                if (maxValue < minValue) {
                    std::swap(minValue, maxValue);
                    m_minValue = minValue;
                    m_maxValue = maxValue;
                }
            
                if ((!usingStepTrackbar && !usingNamedStepTrackbar) || numSteps == -1) {
                    m_numSteps = (maxValue - minValue) + 1;
                }
                
                if (m_numSteps < 2) {
                    m_numSteps = 2;
                }
            
                bool loadedValue = false;
                
                if (!m_packagePath.empty()) {
                    auto configIniData = ult::getParsedDataFromIniFile(m_packagePath + "config.ini");
                    auto sectionIt = configIniData.find(m_label);
                    
                    if (sectionIt != configIniData.end()) {
                        auto indexIt = sectionIt->second.find("index");
                        if (indexIt != sectionIt->second.end() && !indexIt->second.empty()) {
                            m_index = static_cast<s16>(ult::stoi(indexIt->second));
                        }
                        
                        if (!m_usingNamedStepTrackbar) {
                            auto valueIt = sectionIt->second.find("value");
                            if (valueIt != sectionIt->second.end() && !valueIt->second.empty()) {
                                m_value = static_cast<s16>(ult::stoi(valueIt->second));
                                loadedValue = true;
                            }
                        }
                    }
                }
            
                if (m_index >= m_numSteps) m_index = m_numSteps - 1;
                if (m_index < 0) m_index = 0;
            
                if (!loadedValue) {
                    if (m_numSteps > 1) {
                        m_value = minValue + m_index * (static_cast<float>(maxValue - minValue) / (m_numSteps - 1));
                    } else {
                        m_value = minValue;
                    }
                }
                
                if (m_value > maxValue) m_value = maxValue;
                if (m_value < minValue) m_value = minValue;
            
                lastUpdate_ns = ult::nowNs();
            }
            
            virtual ~TrackBarV2() {}
            
            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) {
                return this;
            }
        
            inline void updateAndExecute(bool updateIni = true) {
                if (m_simpleCallback) {
                    m_simpleCallback(m_value, m_index);
                    return;
                }


                if (m_packagePath.empty()) {
                    return;
                }
            
                const std::string indexStr = ult::to_string(m_index);
                const std::string valueStr = m_usingNamedStepTrackbar ? m_selection : ult::to_string(m_value);
            
                if (updateIni) {
                    const std::string configPath = m_packagePath + "config.ini";
                    ult::setIniFileValue(configPath, m_label, "index", indexStr);
                    ult::setIniFileValue(configPath, m_label, "value", valueStr);
                }
                bool success = false;
            
                static const std::string valuePlaceholder = "{value}";
                static const std::string indexPlaceholder = "{index}";
                static const size_t valuePlaceholderLen = valuePlaceholder.length();
                static const size_t indexPlaceholderLen = indexPlaceholder.length();
                const size_t valueStrLen = valueStr.length();
                const size_t indexStrLen = indexStr.length();
                
                size_t tryCount = 0;
                while (!success) {
                    if (interpretAndExecuteCommands) {
                        if (tryCount > 3)
                            break;
                        auto modifiedCmds = getSourceReplacement(commands, valueStr, m_index, m_packagePath);
                        
                        for (auto& cmd : modifiedCmds) {
                            for (auto& arg : cmd) {
                                for (size_t pos = 0; (pos = arg.find(valuePlaceholder, pos)) != std::string::npos; pos += valueStrLen) {
                                    arg.replace(pos, valuePlaceholderLen, valueStr);
                                }
                                
                                if (m_usingNamedStepTrackbar) {
                                    for (size_t pos = 0; (pos = arg.find(indexPlaceholder, pos)) != std::string::npos; pos += indexStrLen) {
                                        arg.replace(pos, indexPlaceholderLen, indexStr);
                                    }
                                }
                            }
                        }
                        
                        success = interpretAndExecuteCommands(std::move(modifiedCmds), m_packagePath, selectedCommand);
                        ult::resetPercentages();
            
                        if (success)
                            break;
                        tryCount++;
                    }
                }
            }
            
            virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
                const u64 keysReleased = m_prevKeysHeld & ~keysHeld;
                m_prevKeysHeld = keysHeld;
                
                const u64 currentTime_ns = ult::nowNs();
                const u64 elapsed_ns = currentTime_ns - lastUpdate_ns;
            
                m_keyRHeld = (keysHeld & KEY_R) != 0;
            
                if ((keysHeld & KEY_R)) {
                    if (keysDown & KEY_UP && !(keysHeld & ~KEY_UP & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Up);
                    else if (keysDown & KEY_DOWN && !(keysHeld & ~KEY_DOWN & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Down);
                    else if (keysDown & KEY_LEFT && !(keysHeld & ~KEY_LEFT & ~KEY_R & ALL_KEYS_MASK)){
                        this->shakeHighlight(FocusDirection::Left);
                    }
                    else if (keysDown & KEY_RIGHT && !(keysHeld & ~KEY_RIGHT & ~KEY_R & ALL_KEYS_MASK)) {
                        this->shakeHighlight(FocusDirection::Right);
                    }
                    return true;
                }
            
                if ((keysDown & KEY_A) && !(keysHeld & ~KEY_A & ALL_KEYS_MASK)) {
                    
                    
                    if (!m_unlockedTrackbar) {
                        ult::atomicToggle(ult::allowSlide);
                        m_holding = false;

                        if (ult::allowSlide.load(std::memory_order_acquire)) {
                            triggerOnFeedback();
                        }
                    }
                    if (m_unlockedTrackbar || (!m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire))) {
                        // Only trigger click animation when unlocked
                        if (m_unlockedTrackbar || ult::allowSlide.load(std::memory_order_acquire)) {
                            triggerClick = true;
                            triggerEnterFeedback();
                        } else if (!m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire)) {
                            triggerOffFeedback();
                        }
                        updateAndExecute();
                    }
                    return true;
                }
            
                if ((keysDown & SCRIPT_KEY) && !(keysHeld & ~SCRIPT_KEY & ALL_KEYS_MASK)) {
                    if (m_scriptKeyListener) {
                        m_scriptKeyListener();
                    }
                    return true;
                }
            
                if (ult::allowSlide.load(std::memory_order_acquire) || m_unlockedTrackbar) {
                    static s16 lastHapticSegment = -1;
                    
                    // Handle key release
                    if (((keysReleased & KEY_LEFT) || (keysReleased & KEY_RIGHT))) {
                        lastHapticSegment = -1; // Reset for next interaction
                        
                        // If we were holding and repeating, just stop
                        if (m_wasLastHeld) {
                            m_wasLastHeld = false;
                            m_holding = false;
                            updateAndExecute();
                            lastUpdate_ns = ult::nowNs();
                            return true;
                        }
                        // If it was a quick tap (no repeat happened), handle the single tick
                        else if (m_holding) {
                            m_holding = false;
                            updateAndExecute();
                            lastUpdate_ns = ult::nowNs();
                            return true;
                        }
                    }
                    
                    // Ignore simultaneous left+right
                    if (keysDown & KEY_LEFT && keysDown & KEY_RIGHT)
                        return true;
                    if (keysHeld & KEY_LEFT && keysHeld & KEY_RIGHT)
                        return true;
            
                    // Handle initial key press
                    if (keysDown & KEY_LEFT || keysDown & KEY_RIGHT) {
                        triggerRumbleClick.store(true, std::memory_order_release);
                        signalFeedback();
                        m_holding = true;
                        m_wasLastHeld = false;
                        m_holdStartTime_ns = ult::nowNs();
                        lastUpdate_ns = currentTime_ns;
                        
                        // Perform the initial single tick
                        if (keysDown & KEY_LEFT && this->m_value > m_minValue) {
                            this->m_index--;
                            this->m_value--;
                            this->m_valueChangedListener(this->m_value);
                            updateAndExecute(false);
                            
                            // Calculate and store initial segment (0-10 for 11 segments)
                            const s16 currentSegment = (this->m_index * 10) / (m_numSteps - 1);
                            if (this->m_index == 0 || currentSegment != lastHapticSegment) {
                                lastHapticSegment = currentSegment;
                                triggerNavigationFeedback();
                            }
                        } else if (keysDown & KEY_RIGHT && this->m_value < m_maxValue) {
                            this->m_index++;
                            this->m_value++;
                            this->m_valueChangedListener(this->m_value);
                            updateAndExecute(false);
                            
                            // Calculate and store initial segment (0-10 for 11 segments)
                            const s16 currentSegment = (this->m_index * 10) / (m_numSteps - 1);
                            if (this->m_index == 0 || currentSegment != lastHapticSegment) {
                                lastHapticSegment = currentSegment;
                                triggerNavigationFeedback();
                            }
                        }
                        return true;
                    }
                    
                    // Handle continued holding (after initial press)
                    if (m_holding && ((keysHeld & KEY_LEFT) || (keysHeld & KEY_RIGHT))) {
                        const u64 holdDuration_ns = currentTime_ns - m_holdStartTime_ns;
            
                        // Initial delay before repeating starts (e.g., 300ms)
                        static constexpr u64 initialDelay_ns = 300000000ULL;
                        // Calculate interval with acceleration
                        static constexpr u64 initialInterval_ns = 67000000ULL;  // ~67ms
                        static constexpr u64 shortInterval_ns = 10000000ULL;    // ~10ms
                        static constexpr u64 transitionPoint_ns = 1000000000ULL; // 1 second
            
                        // If we haven't passed the initial delay, don't repeat yet
                        if (holdDuration_ns < initialDelay_ns) {
                            return true;
                        }
                        
                        const u64 holdDurationAfterDelay_ns = holdDuration_ns - initialDelay_ns;
                        const float t = std::min(1.0f, static_cast<float>(holdDurationAfterDelay_ns) / static_cast<float>(transitionPoint_ns));
                        const u64 currentInterval_ns = static_cast<u64>((initialInterval_ns - shortInterval_ns) * (1.0f - t) + shortInterval_ns);
                        
                        if (elapsed_ns >= currentInterval_ns) {
                            if (keysHeld & KEY_LEFT && this->m_value > m_minValue) {
                                this->m_index--;
                                this->m_value--;
                                this->m_valueChangedListener(this->m_value);
                                if (m_executeOnEveryTick) {
                                    updateAndExecute(false);
                                }
                                
                                // Calculate current segment (0-10 for 11 segments) and trigger haptics on segment change
                                const s16 currentSegment = (this->m_index * 10) / (m_numSteps - 1);
                                if (this->m_index == 0 || currentSegment != lastHapticSegment) {
                                    lastHapticSegment = currentSegment;
                                    triggerNavigationFeedback();
                                }
                                
                                lastUpdate_ns = currentTime_ns;
                                m_wasLastHeld = true;
                                return true;
                            }
                            
                            if (keysHeld & KEY_RIGHT && this->m_value < m_maxValue) {
                                this->m_index++;
                                this->m_value++;
                                this->m_valueChangedListener(this->m_value);
                                if (m_executeOnEveryTick) {
                                    updateAndExecute(false);
                                }
                                
                                // Calculate current segment (0-10 for 11 segments) and trigger haptics on segment change
                                const s16 currentSegment = (this->m_index * 10) / (m_numSteps - 1);
                                if (this->m_index == 0 || currentSegment != lastHapticSegment) {
                                    lastHapticSegment = currentSegment;
                                    triggerNavigationFeedback();
                                }
                                
                                lastUpdate_ns = currentTime_ns;
                                m_wasLastHeld = true;
                                return true;
                            }
                        }
                    } else {
                        m_holding = false;
                    }
                }
                
                return false;
            }
            
            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) override {
                const u16 trackBarWidth = this->getWidth() - 95;
                const u16 handlePos = (trackBarWidth * (this->m_value - m_minValue)) / (m_maxValue - m_minValue);
                const s32 circleCenterX = this->getX() + 59 + handlePos;
                const s32 circleCenterY = this->getY() + 40 + 16 - 1;
                static constexpr s32 circleRadius = 16;
                static bool triggerOnce = true;
                static s16 lastHapticSegment = -1;
                static bool wasOriginallyLocked = false;
                
                const bool touchInCircle = (std::abs(initialX - circleCenterX) <= circleRadius) && (std::abs(initialY - circleCenterY) <= circleRadius);
                
                // CRITICAL FIX: Check if current touch is within valid horizontal bounds
                // Allow vertical drift (top/bottom), only care about left/right bounds
                const s32 trackBarLeft = this->getX() + 59;
                const s32 trackBarRight = trackBarLeft + trackBarWidth;
                const bool currentlyInHorizontalBounds = (currX >= trackBarLeft && currX <= trackBarRight);
                
                // Handle touch start
                if (event == TouchEvent::Touch && touchInCircle) {
                    // Remember if it was locked before we touched it
                    wasOriginallyLocked = !m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire);
                    
                    // Temporarily unlock if it was locked
                    if (wasOriginallyLocked) {
                        ult::allowSlide.store(true, std::memory_order_release);
                    }
                }
                
                // Handle release
                if (event == TouchEvent::Release) {
                    triggerOnce = true;
                    lastHapticSegment = -1;
                    
                    // Re-lock if it was originally locked
                    if (wasOriginallyLocked) {
                        ult::allowSlide.store(false, std::memory_order_release);
                        wasOriginallyLocked = false;
                    }
                    
                    if (touchInSliderBounds) {
                        updateAndExecute();
                        touchInSliderBounds = false;
                        triggerOffFeedback(true);
                        tsl::shiftItemFocus(this);
                    }
                    return false;
                }
                
                const bool isUnlocked = m_unlockedTrackbar || ult::allowSlide.load(std::memory_order_acquire);
                
                // CRITICAL FIX: Only process touch if we're in bounds OR if we were already interacting
                // When going out of horizontal bounds, clamp to min/max value before stopping
                if ((touchInCircle || touchInSliderBounds) && isUnlocked) {
                    // If we were touching but now went out of horizontal bounds, clamp to edge value then stop
                    if (touchInSliderBounds && !currentlyInHorizontalBounds) {
                        // Clamp to max if past right edge, min if past left edge
                        if (currX > trackBarRight) {
                            this->m_value = m_maxValue;
                            this->m_index = m_numSteps - 1;
                        } else if (currX < trackBarLeft) {
                            this->m_value = m_minValue;
                            this->m_index = 0;
                        }
                        this->m_valueChangedListener(this->getProgress());
                        if (m_executeOnEveryTick) {
                            updateAndExecute(false);
                        }
                        
                        touchInSliderBounds = false;
                        return false;
                    }
                    
                    // We're in valid horizontal bounds, continue interaction
                    if (currentlyInHorizontalBounds) {
                        touchInSliderBounds = true;
                        if (triggerOnce) {
                            triggerOnce = false;
                            triggerOnFeedback();
                        }
                        
                        // Add 0.5 to round to nearest step instead of truncating
                        const s16 newIndex = std::max(static_cast<s16>(0), std::min(static_cast<s16>((currX - trackBarLeft) / static_cast<float>(trackBarWidth) * (m_numSteps - 1) + 0.5f), static_cast<s16>(m_numSteps - 1)));
                        const s16 newValue = m_minValue + newIndex * (static_cast<float>(m_maxValue - m_minValue) / (m_numSteps - 1));
                        
                        if (newValue != this->m_value || newIndex != this->m_index) {
                            this->m_value = newValue;
                            this->m_index = newIndex;
                            this->m_valueChangedListener(this->getProgress());
                            if (m_executeOnEveryTick) {
                                updateAndExecute(false);
                            }
                            
                            // Calculate which 10% segment we're in (0-10 for 11 segments)
                            const s16 currentSegment = (newIndex * 10) / (m_numSteps - 1);
                            
                            // Trigger haptics when crossing into a new 10% segment OR at index 0
                            if (newIndex == 0 || currentSegment != lastHapticSegment) {
                                lastHapticSegment = currentSegment;
                                triggerNavigationFeedback();
                            }
                        }
                
                        return true;
                    }
                }
                
                return false;
            }
                                    
            void drawBar(gfx::Renderer *renderer, s32 x, s32 y, u16 width, Color& color, bool isRounded = true) {
                if (isRounded) {
                    renderer->drawUniformRoundedRect(x, y, width, 7, a(color));
                } else {
                    renderer->drawRect(x, y, width, 7, a(color));
                }
            }
        
            virtual void draw(gfx::Renderer *renderer) override {
                const u16 handlePos = (this->getWidth() - 95) * (this->m_value - m_minValue) / (m_maxValue - m_minValue);
                const s32 xPos = this->getX() + 59;
                const s32 yPos = this->getY() + 40 + 16 - 3;
                const s32 width = this->getWidth() - 95;
        
                const bool shouldAppearLocked = m_unlockedTrackbar && m_keyRHeld;
                const bool visuallyUnlocked = (m_unlockedTrackbar && !m_keyRHeld) || touchInSliderBounds;

                if (visuallyUnlocked && touchInSliderBounds) {
                    m_drawFrameless = true;
                    drawHighlight(renderer);
                } else {
                    m_drawFrameless = false;
                }

                drawBar(renderer, xPos, yPos-3, width, trackBarEmptyColor, !m_usingNamedStepTrackbar);
                
                
                if (!this->m_focused) {
                    drawBar(renderer, xPos, yPos-3, handlePos, trackBarFullColor, !m_usingNamedStepTrackbar);
                    renderer->drawCircle(xPos + handlePos, yPos, 16, true, a(!m_drawFrameless ? trackBarSliderBorderColor : highlightColor));
                    renderer->drawCircle(xPos + handlePos, yPos, 13, true, a(visuallyUnlocked ? trackBarSliderMalleableColor : trackBarSliderColor));
                } else {
                    touchInSliderBounds = false;
                    if (m_unlockedTrackbar != ult::unlockedSlide.load(std::memory_order_acquire))
                        ult::unlockedSlide.store(m_unlockedTrackbar, std::memory_order_release);
                    drawBar(renderer, xPos, yPos-3, handlePos, trackBarFullColor, !m_usingNamedStepTrackbar);
                    renderer->drawCircle(xPos + x + handlePos, yPos +y, 16, true, a(highlightColor));
                    const bool focusedVisuallyUnlocked = (ult::allowSlide.load(std::memory_order_acquire) || m_unlockedTrackbar) && !shouldAppearLocked;
                    renderer->drawCircle(xPos + x + handlePos, yPos +y, 12, true, a(focusedVisuallyUnlocked ? trackBarSliderMalleableColor : trackBarSliderColor));
                }
                 
                std::string labelPart = this->m_label;
                ult::removeTag(labelPart);
            
                if (!m_usingNamedStepTrackbar) {
                    m_valuePart = (this->m_units.compare("%") == 0 || this->m_units.compare("°C") == 0 || this->m_units.compare("°F") == 0) 
                                  ? ult::to_string(this->m_value) + this->m_units 
                                  : ult::to_string(this->m_value) + (this->m_units.empty() ? "" : " ") + this->m_units;
                } else
                    m_valuePart = this->m_selection;
            
                const auto valueWidth = renderer->getTextDimensions(m_valuePart, false, 16).first;
            
                renderer->drawString(labelPart, false, xPos, this->getY() + 14 + 16, 16, (!this->m_focused || !ult::useSelectionText) ? defaultTextColor : selectedTextColor);
                renderer->drawString(m_valuePart, false, this->getWidth() -17 - valueWidth, this->getY() + 14 + 16, 16, 
                    (this->m_focused && ult::useSelectionValue) ? selectedValueTextColor : onTextColor);
            
                if (m_lastBottomBound != this->getTopBound())
                    renderer->drawRect(this->getX() + 4+20-1, this->getTopBound(), this->getWidth() + 6 + 10+20 +4, 1, a(separatorColor));
                renderer->drawRect(this->getX() + 4+20-1, this->getBottomBound(), this->getWidth() + 6 + 10+20 +4, 1, a(separatorColor));
                m_lastBottomBound = this->getBottomBound();
            }
            
            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                this->setBoundaries(this->getX() - 16 , this->getY(), this->getWidth()+20+4, tsl::style::TrackBarDefaultHeight );
            }
            
            virtual void drawFocusBackground(gfx::Renderer *renderer) {
            }
            
            virtual void drawHighlight(gfx::Renderer *renderer) override {
                const u64 currentTime_ns = ult::nowNs();
                const double timeInSeconds = static_cast<double>(currentTime_ns) / 1000000000.0;
                progress = ((ult::cos(2.0 * ult::_M_PI * std::fmod(timeInSeconds, 1.0) - ult::_M_PI / 2) + 1.0) / 2.0);
                
                Color clickColor1 = highlightColor1;
                Color clickColor2 = clickColor;
                
                if (triggerClick && !m_clickActive) {
                    m_clickStartTime_ns = currentTime_ns;
                    m_clickActive = true;
                    if (progress >= 0.5) {
                        clickColor1 = clickColor;
                        clickColor2 = highlightColor2;
                    }
                }
            
                if (m_lastLabel != m_label) {
                    m_clickActive = false;
                    triggerClick = false;
                }
                m_lastLabel = m_label;
            
                if (m_clickActive) {
                    const u64 elapsedTime_ns = currentTime_ns - m_clickStartTime_ns;
                    if (elapsedTime_ns < 500000000ULL) {
                        highlightColor = lerpColor(clickColor1, clickColor2, progress);
                    } else {
                        m_clickActive = false;
                        triggerClick = false;
                    }
                } else {
                    const bool shouldAppearLocked = m_unlockedTrackbar && m_keyRHeld;
                    
                    if ((ult::allowSlide.load(std::memory_order_acquire) || m_unlockedTrackbar) && !shouldAppearLocked) {
                        highlightColor = lerpColor(highlightColor1, highlightColor2, progress);
                    } else {
                        highlightColor = lerpColor(highlightColor3, highlightColor4, progress);
                    }
                }
                            
                x = 0;
                y = 0;
                
                if (this->m_highlightShaking) {
                    t_ns = currentTime_ns - this->m_highlightShakingStartTime;
                    const double t_ms = t_ns / 1000000.0;
                    
                    static constexpr double SHAKE_DURATION_MS = 200.0;
                    
                    if (t_ms >= SHAKE_DURATION_MS)
                        this->m_highlightShaking = false;
                    else {
                        // Generate random amplitude only once per shake using the start time as seed
                        const double amplitude = 6.0 + ((this->m_highlightShakingStartTime / 1000000) % 5);
                        const double progress = t_ms / SHAKE_DURATION_MS; // 0 to 1
                        
                        // Lighter damping so both bounces are visible
                        const double damping = 1.0 / (1.0 + 2.5 * progress * (1.0 + 1.3 * progress));
                        
                        // 2 full oscillations = 2 clear bounces
                        const double oscillation = ult::cos(ult::_M_PI * 4.0 * progress);
                        const double displacement = amplitude * oscillation * damping;
                        const int offset = static_cast<int>(displacement);
                        
                        switch (this->m_highlightShakingDirection) {
                            case FocusDirection::Up:    y = -offset; break;
                            case FocusDirection::Down:  y = offset; break;
                            case FocusDirection::Left:  x = -offset; break;
                            case FocusDirection::Right: x = offset; break;
                            default: break;
                        }
                    }
                }
            
                
                if (ult::useSelectionBG)
                    renderer->drawRectAdaptive(this->getX() + x +19, this->getY() + y, this->getWidth()-11-4, this->getHeight(), aWithOpacity(m_drawFrameless ? clickColor : selectionBGColor));
                if (!m_drawFrameless)
                    renderer->drawBorderedRoundedRect(this->getX() + x +19, this->getY() + y, this->getWidth()-11, this->getHeight(), 5, 5, a(highlightColor));
                
                ult::onTrackBar.store(true, std::memory_order_release);
            
                if (m_clickActive && m_useClickAnimation) {
                    const u64 elapsedTime_ns = currentTime_ns - m_clickStartTime_ns;
            
                    auto clickAnimationProgress = tsl::style::ListItemHighlightLength * (1.0f - (static_cast<float>(elapsedTime_ns) / 500000000.0f));
                    
                    if (clickAnimationProgress < 0.0f) {
                        clickAnimationProgress = 0.0f;
                    }
                
                    if (clickAnimationProgress > 0.0f) {
                        const u8 saturation = tsl::style::ListItemHighlightSaturation * (float(clickAnimationProgress) / float(tsl::style::ListItemHighlightLength));
                
                        Color animColor = {0xF, 0xF, 0xF, 0xF};
                        if (invertBGClickColor) {
                            animColor.r = 15 - saturation;
                            animColor.g = 15 - saturation;
                            animColor.b = 15 - saturation;
                        } else {
                            animColor.r = saturation;
                            animColor.g = saturation;
                            animColor.b = saturation;
                        }
                        animColor.a = selectionBGColor.a;
                        renderer->drawRect(this->getX() +22, this->getY(), this->getWidth() -22, this->getHeight(), aWithOpacity(animColor));
                    }
                }
            }
            
            virtual u8 getIndex() {
                return this->m_index;
            }

            virtual u8 getProgress() {
                return this->m_value;
            }
            
            virtual void setProgress(u8 value) {
                this->m_value = value;
            }
            
            void setValueChangedListener(std::function<void(u8)> valueChangedListener) {
                this->m_valueChangedListener = valueChangedListener;
            }

            void setSimpleCallback(SimpleValueChangeCallback callback) {
                m_simpleCallback = std::move(callback);
            }
        
            inline void disableClickAnimation() {
                m_useClickAnimation = false;
            }

        protected:
            std::string m_label;
            std::string m_packagePath;
            std::string m_selection;
            s16 m_value = 0;
            s16 m_minValue = 0;
            s16 m_maxValue = 100;
            std::string m_units;
            bool m_interactionLocked = false;
            bool m_keyRHeld = false;
            
            std::function<void(u8)> m_valueChangedListener = [](u8) {};
        
            std::function<bool(std::vector<std::vector<std::string>>&&, const std::string&, const std::string&)> interpretAndExecuteCommands;
            std::function<std::vector<std::vector<std::string>>(const std::vector<std::vector<std::string>>&, const std::string&, size_t, const std::string&)> getSourceReplacement;
            std::vector<std::vector<std::string>> commands;
            std::string selectedCommand;
        
            bool m_usingStepTrackbar = false;
            bool m_usingNamedStepTrackbar = false;
            s16 m_numSteps = 2;
            s16 m_index = 0;
            bool m_unlockedTrackbar = false;
            bool m_executeOnEveryTick = false;
            bool touchInSliderBounds = false;
            bool triggerClick = false;
            std::function<void()> m_scriptKeyListener;
            
            // Instance variables replacing static ones
            float m_lastBottomBound = 0.0f;
            std::string m_valuePart = "";
            u64 m_clickStartTime_ns = 0;
            bool m_clickActive = false;
            std::string m_lastLabel = "";
            bool m_holding = false;
            u64 m_holdStartTime_ns = 0;
            u64 m_prevKeysHeld = 0;
            bool m_wasLastHeld = false;
            bool m_drawFrameless = false;

            bool m_useClickAnimation = true;

            SimpleValueChangeCallback m_simpleCallback = nullptr;
        };
        
        
        /**
         * @brief A customizable analog trackbar going from 0% to 100% but using discrete steps (Like the volume slider)
         *
         */
        class StepTrackBarV2 : public TrackBarV2 {
        public:

            /**
             * @brief Constructor
             *
             * @param icon Icon shown next to the track bar
             * @param numSteps Number of steps the track bar has
             */
            StepTrackBarV2(std::string label, std::string packagePath, size_t numSteps, s16 minValue, s16 maxValue, std::string units,
                std::function<bool(std::vector<std::vector<std::string>>&&, const std::string&, const std::string&)> executeCommands = nullptr,
                std::function<std::vector<std::vector<std::string>>(const std::vector<std::vector<std::string>>&, const std::string&, size_t, const std::string&)> sourceReplacementFunc = nullptr,
                std::vector<std::vector<std::string>> cmd = {}, const std::string& selCmd = "", bool usingNamedStepTrackbar = false, bool unlockedTrackbar = false, bool executeOnEveryTick = false)
                : TrackBarV2(label, packagePath, minValue, maxValue, units, executeCommands, sourceReplacementFunc, cmd, selCmd, !usingNamedStepTrackbar, usingNamedStepTrackbar, numSteps, unlockedTrackbar, executeOnEveryTick) {}
            
            virtual ~StepTrackBarV2() {}
            
            virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
                static u32 tick = 0;
                static bool holding = false;
                static u64 prevKeysHeld = 0;
                const u64 keysReleased = prevKeysHeld & ~keysHeld;
                prevKeysHeld = keysHeld;
            
                static bool wasLastHeld = false;
            
                // Update KEY_R state for visual appearance
                m_keyRHeld = (keysHeld & KEY_R) != 0;
            
                if ((keysHeld & KEY_R)) {
                    if (keysDown & KEY_UP && !(keysHeld & ~KEY_UP & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Up);
                    else if (keysDown & KEY_DOWN && !(keysHeld & ~KEY_DOWN & ~KEY_R & ALL_KEYS_MASK))
                        this->shakeHighlight(FocusDirection::Down);
                    else if (keysDown & KEY_LEFT && !(keysHeld & ~KEY_LEFT & ~KEY_R & ALL_KEYS_MASK)){
                        this->shakeHighlight(FocusDirection::Left);
                    }
                    else if (keysDown & KEY_RIGHT && !(keysHeld & ~KEY_RIGHT & ~KEY_R & ALL_KEYS_MASK)) {
                        this->shakeHighlight(FocusDirection::Right);
                    }
                    return true;
                }
            
                // Check if KEY_A is pressed to toggle ult::allowSlide
                if ((keysDown & KEY_A) && !(keysHeld & ~KEY_A & ALL_KEYS_MASK)) {
                    
                    
                    if (!m_unlockedTrackbar) {
                        ult::atomicToggle(ult::allowSlide);
                        m_holding = false;

                        if (ult::allowSlide.load(std::memory_order_acquire)) {
                            triggerOnFeedback();
                        }
                    }
                    if (m_unlockedTrackbar || (!m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire))) {
                        // Only trigger click animation when unlocked
                        if (m_unlockedTrackbar || ult::allowSlide.load(std::memory_order_acquire)) {
                            triggerClick = true;
                            triggerEnterFeedback();
                        } else if (!m_unlockedTrackbar && !ult::allowSlide.load(std::memory_order_acquire)) {
                            triggerOffFeedback();
                        }
                        updateAndExecute();
                    }
                    return true;
                }
                
                // Handle SCRIPT_KEY press
                if ((keysDown & SCRIPT_KEY) && !(keysHeld & ~SCRIPT_KEY & ALL_KEYS_MASK)) {
                    if (m_scriptKeyListener) {
                        m_scriptKeyListener();
                    }
                    return true;
                }
            
                if (ult::allowSlide.load(std::memory_order_acquire) || m_unlockedTrackbar) {
                    if (((keysReleased & KEY_LEFT) || (keysReleased & KEY_RIGHT)) ||
                        (wasLastHeld && !(keysHeld & (KEY_LEFT | KEY_RIGHT)))) {
                        updateAndExecute();
                        holding = false;
                        wasLastHeld = false;
                        tick = 0;
                        return true;
                    }
                    
                    if (keysHeld & KEY_LEFT && keysHeld & KEY_RIGHT) {
                        tick = 0;
                        return true;
                    }
                    
                    if (keysHeld & (KEY_LEFT | KEY_RIGHT)) {
                        if (!holding) {
                            holding = true;
                            tick = 0;
                        }
                        
                        if ((tick == 0 || tick > 20) && (tick % 3) == 0) {
                            const float stepSize = static_cast<float>(m_maxValue - m_minValue) / (this->m_numSteps - 1);
                            if (keysHeld & KEY_LEFT && this->m_index > 0) {
                                triggerNavigationFeedback();
                                
                                this->m_index--;
                                this->m_value = static_cast<s16>(std::round(m_minValue + m_index * stepSize));
                            } else if (keysHeld & KEY_RIGHT && this->m_index < this->m_numSteps-1) {
                                triggerNavigationFeedback();
                                
                                this->m_index++;
                                this->m_value = static_cast<s16>(std::round(m_minValue + m_index * stepSize));
                            } else {
                                return false;
                            }
                            this->m_valueChangedListener(this->getProgress());
                            if (m_executeOnEveryTick)
                                updateAndExecute(false);
                            wasLastHeld = true;
                        }
                        tick++;
                        return true;
                    } else {
                        holding = false;
                        tick = 0;
                    }
                }
                
                return false;
            }
            
            
            /**
             * @brief Gets the current value of the trackbar
             *
             * @return State
             */
            virtual u8 getProgress() override {
                return this->m_value / (100 / (this->m_numSteps - 1));
            }
            
            /**
             * @brief Sets the current state of the toggle. Updates the Value
             *
             * @param state State
             */
            virtual void setProgress(u8 value) override {
                value = std::min(value, u8(this->m_numSteps - 1));
                this->m_index = value;
                
                // If using simple callback (modern API), use minValue/maxValue range
                // Otherwise use legacy 0-100 range for config.ini compatibility
                if (m_simpleCallback) {
                    const float stepSize = static_cast<float>(m_maxValue - m_minValue) / (this->m_numSteps - 1);
                    this->m_value = static_cast<s16>(std::round(m_minValue + m_index * stepSize));
                } else {
                    // Legacy behavior for command system
                    this->m_value = value * (100 / (this->m_numSteps - 1));
                }
            }
        };
        
        
        /**
         * @brief A customizable trackbar with multiple discrete steps with specific names. Name gets displayed above the bar
         *
         */
        class NamedStepTrackBarV2 : public StepTrackBarV2 {
        public:
            u16 trackBarWidth, stepWidth, currentDescIndex;
            u32 descWidth, descHeight;
            
            /**
             * @brief Constructor
             *
             * @param icon Icon shown next to the track bar
             * @param stepDescriptions Step names displayed above the track bar
             */
            NamedStepTrackBarV2(std::string label, std::string packagePath, std::vector<std::string>& stepDescriptions,
                std::function<bool(std::vector<std::vector<std::string>>&&, const std::string&, const std::string&)> executeCommands = nullptr,
                std::function<std::vector<std::vector<std::string>>(const std::vector<std::vector<std::string>>&, const std::string&, size_t, const std::string&)> sourceReplacementFunc = nullptr,
                std::vector<std::vector<std::string>> cmd = {}, const std::string& selCmd = "", bool unlockedTrackbar = false, bool executeOnEveryTick = false)
                : StepTrackBarV2(label, packagePath, stepDescriptions.size(), 0, (stepDescriptions.size()-1), "", executeCommands, sourceReplacementFunc, cmd, selCmd, true, unlockedTrackbar, executeOnEveryTick), m_stepDescriptions(stepDescriptions) {
                    // Initialize the selection with the current index
                    if (!m_stepDescriptions.empty() && m_index >= 0 && m_index < static_cast<s16>(m_stepDescriptions.size())) {
                        this->m_selection = m_stepDescriptions[m_index];
                        currentDescIndex = m_index;
                    }
                }
            
            virtual ~NamedStepTrackBarV2() {}
                        
            virtual void draw(gfx::Renderer *renderer) override {
                // Cache frequently used values
                const u16 trackBarWidth = this->getWidth() - 95;
                const u16 baseX = this->getX() + 59;
                const u16 baseY = this->getY() + 44; // 50 - 3
                const u8 numSteps = this->m_numSteps;
                const u8 halfNumSteps = (numSteps - 1) / 2;
                const u16 lastStepX = baseX + trackBarWidth - 1;
                
                // Pre-calculate step spacing
                const float stepSpacing = static_cast<float>(trackBarWidth) / (numSteps - 1);
                
                // Cache color for multiple drawRect calls
                const auto stepColor = a(trackBarEmptyColor);
                
                // Draw step rectangles - optimized loop
                u16 stepX;
                for (u8 i = 0; i < numSteps; i++) {
                    
                    if (i == numSteps - 1) {
                        // Last step - avoid overshooting
                        stepX = lastStepX;
                    } else {
                        stepX = baseX + static_cast<u16>(std::round(i * stepSpacing));
                        // Adjust for steps on right side of center
                        if (i > halfNumSteps) {
                            stepX -= 1;
                        }
                    }
                    
                    renderer->drawRect(stepX, baseY, 1, 8, stepColor);
                }
                
                // Update selection (only if index changed - optional optimization)
                if (currentDescIndex != this->m_index) {
                    currentDescIndex = this->m_index;
                    this->m_selection = this->m_stepDescriptions[currentDescIndex];
                }
                
                // Draw the parent trackbar
                StepTrackBarV2::draw(renderer);
            }

            
        protected:
            std::vector<std::string> m_stepDescriptions;
            
        };
        
    }
    

    // Global state and event system
    static inline Event notificationEvent;
    static inline std::mutex notificationJsonMutex;
    static inline std::atomic<uint32_t> notificationGeneration{0};

    // Max notifications cap (max value of 4 on limited memory, 8 otherwise)
    extern int maxNotifications;

    class NotificationPrompt {
    public:
        NotificationPrompt()
            : enabled_(true),
              generation_(notificationGeneration.load(std::memory_order_acquire))
        {}

        ~NotificationPrompt() { shutdown(); }

        enum class PromptState : u8 {
            Inactive, FadingIn, Visible, FadingOut
        };

        enum class Alignment : u8 { Center = 0, Left = 1, Right = 2 };
        enum class SplitType : u8 { Word   = 0, Char = 1 };

        struct NotifEntry {
            std::string text;
            std::string title;
            char        timestamp[10] = {};
            std::string fileName;
            u8  fontSize     = 28;
            u16 durationMs   = 3000;
            u8  priority     = 20;
            u64 arrivalNs    = 0;
            PromptState state        = PromptState::Inactive;
            u64 expireNs             = 0;
            u64 stateStartNs         = 0;
            bool showTime            = true;
            bool hasIcon             = false;
            bool iconPending         = false;
            Alignment alignment      = Alignment::Center;
            SplitType splitType      = SplitType::Word;
        };

        struct NotifCompare {
            bool operator()(const NotifEntry& a, const NotifEntry& b) const {
                if (a.priority == b.priority) return a.arrivalNs > b.arrivalNs;
                return a.priority > b.priority;
            }
        };

        struct Lines {
            static constexpr u8 MAX_LINES = 10;
            std::string buf[MAX_LINES];
            u8 count = 0;
            const std::string& operator[](s32 i) const { return buf[i]; }
        };

        static constexpr size_t TITLE_FONT       = 18;
        static constexpr s32    NOTIF_ICON_DIM   = 50;
        static constexpr size_t NOTIF_ICON_BYTES = NOTIF_ICON_DIM * NOTIF_ICON_DIM * 2;
        static constexpr int    MAX_VISIBLE      = 8;
        static constexpr s32    NOTIF_WIDTH      = 448;
        static constexpr s32    NOTIF_HEIGHT     = 88;

        // ── Public API ───────────────────────────────────────────────────────────
        void show(const std::string& msg, size_t fontSize = 26, u32 priority = 20,
                  const std::string& fileName = "", const std::string& title = "",
                  u32 durationMs = 3000,
                  bool immediately = false, bool resume = false, bool showTime = true,
                  std::string_view alignment = {},
                  std::string_view splitType = {},
                  std::string_view timestamp = {}) {

            if (msg.empty()) return;
            if (isStale()) return;

            NotifEntry data;
            data.text         = msg;
            data.title        = title;
            data.fileName     = fileName;
            data.fontSize     = static_cast<u8>(std::clamp(fontSize, size_t(8), size_t(48)));
            data.durationMs   = (durationMs == 0) ? 0
                              : static_cast<u16>(std::clamp(durationMs, 500u, 30000u));
            data.priority     = static_cast<u8>(immediately ? 0u : priority);
            data.showTime     = showTime;
            data.alignment    = parseAlignment(alignment, !title.empty());
            data.splitType    = (!splitType.empty() && splitType[0] == 'c') ? SplitType::Char : SplitType::Word;
            data.arrivalNs    = ult::nowNs();
            if (!timestamp.empty()) {
                const size_t n = std::min(timestamp.size(), sizeof(data.timestamp) - 1);
                std::memcpy(data.timestamp, timestamp.data(), n);
                data.timestamp[n] = '\0';
            } else {
                ult::formatTimestamp(time(nullptr), data.timestamp, sizeof(data.timestamp));
            }

            std::lock_guard<std::mutex> lg(state_mutex_);
            if (isStale()) return;

            if (immediately) {
                bool skipFadeIn = false;
                Slot& s0 = slots_[0];
                if (s0.flags & SLOT_ACTIVE) {
                    if (s0.flags & SLOT_SHOW_NOW) {
                        evictSlot_NoLock(0);
                        skipFadeIn = true;
                    } else {
                        // Delay bottom-most only if all slots are full
                        if (slots_[maxNotifications - 1].flags & SLOT_ACTIVE) {
                            if (pending_queue_.size() < MAX_NOTIFS)
                                pending_queue_.push(std::move(slots_[maxNotifications - 1].data));
                            slots_[maxNotifications - 1] = Slot{};
                        }
                        // Shift all slots down by one to make room at slot 0
                        for (int j = maxNotifications - 1; j >= 1; --j)
                            slots_[j] = std::move(slots_[j - 1]);
                        slots_[0] = Slot{};
                    }
                }
                placeInSlot_NoLock(0, std::move(data), true, skipFadeIn);
                repackSlots_NoLock(ult::nowNs());
            } else {
                int freeSlot = -1;
                for (int i = 0; i < maxNotifications; ++i)
                    if (!(slots_[i].flags & SLOT_ACTIVE)) { freeSlot = i; break; }
                if (freeSlot >= 0) {
                    placeInSlot_NoLock(freeSlot, std::move(data), false, resume, resume);
                } else {
                    if (pending_queue_.size() < MAX_NOTIFS)
                        pending_queue_.push(std::move(data));
                    return;
                }
            }

            eventFire(&notificationEvent);
            #if IS_STATUS_MONITOR_DIRECTIVE
            if (isRendering) {
                isRendering  = false;
                wasRendering = true;
                leventSignal(&renderingStopEvent);
            }
            #endif
        }

        void showNow(const std::string& msg, size_t fontSize = 26,
                     const std::string& title = "",
                     u32 durationMs = 2500,
                     bool showTime = true,
                     const std::string& fileName = "",
                     std::string_view alignment = {},
                     std::string_view splitType = {}) {
            show(msg, fontSize, 0u, fileName, title, durationMs, true, false, showTime, alignment, splitType);
        }

        [[nodiscard]] bool hasActiveFile(std::string_view fname) const;

        void draw(gfx::Renderer* renderer, bool promptOnly = false);
        void update();
        [[nodiscard]] bool isActive() const;
        [[nodiscard]] int activeCount() const;
        void shutdown();
        void forceShutdown() { enabled_.store(false, std::memory_order_release); }
        [[nodiscard]] bool hitTest(s32 tx, s32 ty) const;
        bool dismissAt(s32 tx, s32 ty);
        bool dismissFront();

    private:
        static constexpr size_t MAX_NOTIFS        = 30;
        static constexpr u32    FADE_DURATION_MS  = 83;
        static constexpr u32    SLIDE_DURATION_MS = 150;

        static constexpr u8 SLOT_ACTIVE        = 1 << 0;
        static constexpr u8 SLOT_SHOW_NOW      = 1 << 1;
        static constexpr u8 SLOT_SLIDING       = 1 << 2;
        static constexpr u8 SLOT_ICON_LOADED   = 1 << 3;
        static constexpr u8 SLOT_SOUND_PENDING = 1 << 4;

        struct Slot {
            NotifEntry            data;
            float                 yCurrent     = 0.f;
            float                 yTarget      = 0.f;
            float                 ySlideFrom   = 0.f;
            u64                   slideStartNs = 0;
            u8                    flags        = 0;
            std::unique_ptr<u8[]> iconBuf;
        };

        Slot               slots_[MAX_VISIBLE];
        mutable std::mutex state_mutex_;
        std::priority_queue<NotifEntry, std::vector<NotifEntry>, NotifCompare> pending_queue_;
        std::atomic<bool>  enabled_{true};
        std::atomic<u32>   generation_{0};

        bool isStale() const {
            return !enabled_.load(std::memory_order_acquire)
                || generation_.load(std::memory_order_acquire)
                        != notificationGeneration.load(std::memory_order_acquire);
        }

        // Transition a slot's data into FadingOut state. Caller holds state_mutex_.
        static void startFadeOut(NotifEntry& e, u64 now) {
            e.state        = PromptState::FadingOut;
            e.stateStartNs = now;
        }

        // Parse alignment string to enum, with a sensible title-aware default.
        static Alignment parseAlignment(std::string_view sv, bool hasTitle) {
            if (!sv.empty()) {
                if (sv[0] == 'l') return Alignment::Left;
                if (sv[0] == 'r') return Alignment::Right;
                return Alignment::Center;
            }
            return hasTitle ? Alignment::Left : Alignment::Center;
        }

        void evictSlot_NoLock(int i) {
            if (!slots_[i].data.fileName.empty())
                remove((ult::NOTIFICATIONS_PATH + slots_[i].data.fileName).c_str());
            slots_[i] = Slot{};
        }

        void clearAll_NoLock() {
            for (int i = 0; i < maxNotifications; ++i) slots_[i] = Slot{};
            while (!pending_queue_.empty()) pending_queue_.pop();
        }

        [[nodiscard]] int findHitSlot_NoLock(s32 tx, s32 ty) const;

        // Icon/text-area geometry, computed once and shared between
        // getEffectiveHeight and drawSlot.
        struct IconGeom {
            s32  baseIconPad;
            s32  iconColW;
            bool hasIconCol;
            s32  textAreaX;
            s32  textAreaW;
        };

        static IconGeom computeIconGeom(const Slot& slot, s32 x = 0) {
            IconGeom g;
            g.baseIconPad = (NOTIF_HEIGHT - NOTIF_ICON_DIM) / 2;
            g.iconColW    = g.baseIconPad + NOTIF_ICON_DIM + g.baseIconPad;
            g.hasIconCol  = (slot.data.hasIcon && (slot.flags & SLOT_ICON_LOADED))
                          || slot.data.iconPending;
            g.textAreaW   = g.hasIconCol ? NOTIF_WIDTH - g.iconColW : NOTIF_WIDTH;
            g.textAreaX   = g.hasIconCol ? x + g.iconColW : x;
            return g;
        }

        // Height contributed by n additional wrapped lines beyond the first:
        // n * (lineHeight + 3)
        static s32 extraLinesHeight(s32 n, s32 lineHeight) {
            return n * (lineHeight + 3);
        }

        static constexpr float easeInOut(float t) {
            return (t < 0.5f) ? (2*t*t) : (-1 + (4 - 2*t)*t);
        }

        static constexpr Color applyAlpha(Color c, float a) {
            c.a = static_cast<u8>(static_cast<float>(c.a) * a);
            return c;
        }

        [[gnu::noinline]]
        Lines getWrappedLines(const std::string& text, float pixelWidth,
                              size_t fontSize, u8 maxLines,
                              SplitType splitType = SplitType::Word) const;

        [[gnu::noinline]]
        s32 getEffectiveHeight(const Slot& slot) const;

        [[gnu::noinline]]
        void placeInSlot_NoLock(int idx, NotifEntry&& e, bool isShowNow,
                                bool skipFadeIn, bool suppressSound = false);

        [[gnu::noinline]]
        void repackSlots_NoLock(u64 now);

        [[gnu::noinline]]
        void applyEllipsis(Lines& lines, u8 maxLines, float pixelWidth,
                           size_t fontSize, gfx::Renderer* renderer) const;

        [[gnu::noinline]]
        void drawSlot(gfx::Renderer* renderer, const Slot& slot,
                      s32 baseY, float fadeAlpha, bool promptOnly);

        #if IS_LAUNCHER_DIRECTIVE
        [[gnu::noinline]]
        void drawUltrahandLine(gfx::Renderer* renderer, const std::string& line,
                               s32 x, s32 y, u32 fontSize, float fadeAlpha,
                               Color textColor = notificationTextColor);
        #endif
    };

    inline NotificationPrompt* notification = nullptr;


    // GUI
    
    /**
     * @brief The top level Gui class
     * @note The main menu and every sub menu are a separate Gui. Create your own Gui class that extends from this one to create your own menus
     *
     */
    class Gui {
    public:
        Gui() {
            #if !IS_LAUNCHER_DIRECTIVE
            {
                #if INITIALIZE_IN_GUI_DIRECTIVE // for different project structures
                
                // Load the bitmap file into memory
                ult::loadWallpaperFileWhenSafe();
                #endif
            }
            #endif
        }
        
        virtual ~Gui() {
            if (this->m_topElement != nullptr)
                delete this->m_topElement;

            if (this->m_bottomElement != nullptr)
                delete this->m_bottomElement;
        }
        
        /**
         * @brief Creates all elements present in this Gui
         * @note Implement this function and let it return a heap allocated element used as the top level element. This is usually some kind of frame e.g \ref OverlayFrame
         *
         * @return Top level element
         */
        virtual elm::Element* createUI() = 0;
        
        /**
         * @brief Called once per frame to update values
         *
         */
        virtual void update() {}
        
        /**
         * @brief Called once per frame with the latest HID inputs
         *
         * @param keysDown Buttons pressed in the last frame
         * @param keysHeld Buttons held down longer than one frame
         * @param touchInput Last touch position
         * @param leftJoyStick Left joystick position
         * @param rightJoyStick Right joystick position
         * @return Weather or not the input has been consumed
         */
        virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) {
            return false;
        }
        
        /**
         * @brief Gets the top level element
         *
         * @return Top level element
         */
        elm::Element* getTopElement() {
            return this->m_topElement;
        }
        
        /**
         * @brief Gets the bottom level element
         *
         * @return Bottom level element
         */
        elm::Element* getBottomElement() {
            return this->m_bottomElement;
        }

        /**
         * @brief Get the currently focused element
         *
         * @return Focused element
         */
        elm::Element* getFocusedElement() {
            return this->m_focusedElement;
        }
        
        /**
         * @brief Requests focus to a element
         * @note Use this function when focusing a element outside of a element's requestFocus function
         *
         * @param element Element to focus
         * @param direction Focus direction
         */
        inline void requestFocus(elm::Element *element, FocusDirection direction, bool shake = true) {
            elm::Element *oldFocus = this->m_focusedElement;
            
            if (element != nullptr) {
                this->m_focusedElement = element->requestFocus(oldFocus, direction);
                
                if (oldFocus != nullptr)
                    oldFocus->setFocused(false);
                
                if (this->m_focusedElement != nullptr) {
                    this->m_focusedElement->setFocused(true);
                }
            }
            
            if (shake && oldFocus == this->m_focusedElement && this->m_focusedElement != nullptr) {
                if (!tsl::elm::isTableScrolling.load(std::memory_order_acquire)) {
                    this->m_focusedElement->shakeHighlight(direction);
                }
            }
        }
        
        /**
         * @brief Removes focus from a element
         *
         * @param element Element to remove focus from. Pass nullptr to remove the focus unconditionally
         */
        inline void removeFocus(elm::Element* element = nullptr) {
            if (element == nullptr || element == this->m_focusedElement) {
                if (this->m_focusedElement != nullptr) {
                    this->m_focusedElement->setFocused(false);
                    this->m_focusedElement = nullptr;
                }
            }
        }
        
        inline void restoreFocus() {
            this->m_initialFocusSet = false;
        }
        
    protected:
        constexpr static inline auto a = &gfx::Renderer::a;
        constexpr static inline auto aWithOpacity = &gfx::Renderer::aWithOpacity;
        
    private:
        elm::Element *m_focusedElement = nullptr;
        elm::Element *m_topElement = nullptr;
        elm::Element *m_bottomElement = nullptr;

        bool m_initialFocusSet = false;
        
        friend class Overlay;
        friend class gfx::Renderer;
        
        /**
         * @brief Draws the Gui
         *
         * @param renderer
         */
        void draw(gfx::Renderer *renderer) {
            if (this->m_topElement != nullptr)
                this->m_topElement->draw(renderer);
        }
        
        inline bool initialFocusSet() {
            return this->m_initialFocusSet;
        }
        
        inline void markInitialFocusSet() {
            this->m_initialFocusSet = true;
        }
        
    };
    

    // Overlay
    
    /**
     * @brief The top level Overlay class
     * @note Every Tesla overlay should have exactly one Overlay class initializing services and loading the default Gui
     */
    class Overlay {
    protected:
        /**
         * @brief Constructor
         * @note Called once when the Overlay gets loaded
         */
        Overlay() {}
    public:

        /**
         * @brief Deconstructor
         * @note Called once when the Overlay exits
         *
         */
        virtual ~Overlay() {}


        /**
         * @brief Initializes services
         * @note Called once at the start to initializes services. You have a sm session available during this call, no need to initialize sm yourself
         */
        virtual void initServices() {}
        
        /**
         * @brief Exits services
         * @note Make sure to exit all services you initialized in \ref Overlay::initServices() here to prevent leaking handles
         */
        virtual void exitServices() {}
        
        /**
         * @brief Called before overlay changes from invisible to visible state
         *
         */
        virtual void onShow() {}
        
        /**
         * @brief Called before overlay changes from visible to invisible state
         *
         */
        virtual void onHide() {}
        
        /**
         * @brief Loads the default Gui
         * @note This function should return the initial Gui to load using the \ref Gui::initially<T>(Args.. args) function
         *       e.g `return initially<GuiMain>();`
         *
         * @return Default Gui
         */
        virtual std::unique_ptr<tsl::Gui> loadInitialGui() = 0;
        
        /**
         * @brief Gets a reference to the current Gui on top of the Gui stack
         *
         * @return Current Gui reference
         */
        std::unique_ptr<tsl::Gui>& getCurrentGui() {
            return this->m_guiStack.top();
        }
        
        /**
         * @brief Shows the Gui
         *
         */
        void show() {
            bool signalFeedbackAtEnd = false;
            if (ult::useHapticFeedback) {
                if (!ult::isHidden.load(std::memory_order_acquire)) {
                    triggerInitHaptics.store(true, std::memory_order_release);
                    signalFeedbackAtEnd = true;
                }
            }
            

            // reinitialize audio for changes from handheld to docked and vise versa
            if (!ult::limitedMemory && ult::useSoundEffects) {
                reloadIfDockedChangedNow.store(true, std::memory_order_release);
                signalFeedbackAtEnd = true;
            }

            if (this->m_disableNextAnimation) {
                this->m_animationCounter = MAX_ANIMATION_COUNTER;
                this->m_disableNextAnimation = false;
            }
            else {
                this->m_fadeInAnimationPlaying = true;
                this->m_animationCounter = 0;
            }
            
            this->onShow();

            ult::isHidden.store(false);
            
            if (ult::useHapticFeedback) {
                // Skip the click if exit feedback (double click) is about to fire on the
                // same show — the flag is set just before show() is called in that case.
                //triggerRumbleClick.store(true, std::memory_order_release);

                if (!skipRumbleClick) {
                    triggerRumbleClick.store(true, std::memory_order_release);
                }
                if (!skipRumbleClick)
                    signalFeedbackAtEnd = true;
                skipRumbleClick = false; // consume the flag regardless
            }

            if (signalFeedbackAtEnd) {
                signalFeedback();
            }
        }
        
        /**
         * @brief Hides the Gui
         *
         */
        void hide(bool useNoFade = false) {


            if (useNoFade) {
                // Immediately hide overlay
                ult::isHidden.store(true);
                this->m_shouldHide = true;
                return;
            }
            

        #if IS_STATUS_MONITOR_DIRECTIVE
            if (FullMode && !deactivateOriginalFooter) {
                
                if (this->m_disableNextAnimation) {
                    this->m_animationCounter = 0;
                    this->m_disableNextAnimation = false;
                }
                else {
                    this->m_fadeOutAnimationPlaying = true;
                    this->m_animationCounter = MAX_ANIMATION_COUNTER;
                }
                ult::isHidden.store(true);
                this->onHide();
            }
        #else

            if (this->m_disableNextAnimation) {
                this->m_animationCounter = 0;
                this->m_disableNextAnimation = false;
            }
            else {
                this->m_fadeOutAnimationPlaying = true;
                this->m_animationCounter = MAX_ANIMATION_COUNTER;
            }
            ult::isHidden.store(true);
            this->onHide();
        #endif
            triggerRumbleClick.store(true, std::memory_order_release);
            signalFeedback();
        }
        
        /**
         * @brief Returns whether fade animation is playing
         *
         * @return whether fade animation is playing
         */
        bool fadeAnimationPlaying() {
            return this->m_fadeInAnimationPlaying || this->m_fadeOutAnimationPlaying;
        }
        
        /**
         * @brief Closes the Gui
         * @note This makes the Tesla overlay exit and return back to the Tesla-Menu
         *
         */
        void close(bool forceClose = false) {
            if (!forceClose && notification && notification->isActive()) {
                this->closeAfter();
                this->hide(true);
                return;
            }
        
            this->m_shouldClose = true;
        }

        /**
         * @brief Closes the Gui
         * @note This makes the Tesla overlay exit and return back to the Tesla-Menu
         *
         */
        void closeAfter() {
            this->m_shouldCloseAfter = true;

        }
        
        /**
         * @brief Gets the Overlay instance
         *
         * @return Overlay instance
         */
        static inline Overlay* const get() {
            return Overlay::s_overlayInstance;
        }
        
        /**
         * @brief Creates the initial Gui of an Overlay and moves the object to the Gui stack
         *
         * @tparam T
         * @tparam Args
         * @param args
         * @return constexpr std::unique_ptr<T>
         */
        template<typename T, typename ... Args>
        constexpr inline std::unique_ptr<T> initially(Args&&... args) {
            return std::make_unique<T>(args...);
        }
        
    private:
        using GuiPtr = std::unique_ptr<tsl::Gui>;
        std::stack<GuiPtr, std::list<GuiPtr>> m_guiStack;
        static inline Overlay *s_overlayInstance = nullptr;
        
        bool m_fadeInAnimationPlaying = false, m_fadeOutAnimationPlaying = false;
        u8 m_animationCounter = 0;
        static constexpr int MAX_ANIMATION_COUNTER = 5; // Define the maximum animation counter value

        bool m_shouldHide = false;
        bool m_shouldClose = false;
        bool m_shouldCloseAfter = false;
        
        bool m_disableNextAnimation = false;
        
        bool m_closeOnExit;
        
        static inline std::atomic<bool> isNavigatingBackwards{false};
        bool justNavigated = false;

        /**
         * @brief Initializes the Renderer
         *
         */
        void initScreen() {
            gfx::Renderer::get().init();
        }
        
        /**
         * @brief Exits the Renderer
         *
         */
        void exitScreen() {
            gfx::Renderer::get().exit();
        }
        
        /**
         * @brief Weather or not the Gui should get hidden
         *
         * @return should hide
         */
        bool shouldHide() {
            return this->m_shouldHide;
        }
        
        /**
         * @brief Weather or not hte Gui should get closed
         *
         * @return should close
         */
        bool shouldClose() {
            return this->m_shouldClose;
        }
        
        /**
         * @brief Weather or not hte Gui should get closed after
         *
         * @return should close after
         */
        bool shouldCloseAfter() {
            return this->m_shouldCloseAfter;
        }
        

        /**
         * @brief Quadratic ease-in-out function
         *
         * @param t Normalized time (0 to 1)
         * @return Eased value
         */
        float calculateEaseInOut(float t) {
            if (t < 0.5) {
                return 2 * t * t;
            } else {
                return -1 + (4 - 2 * t) * t;
            }
        }

        /**
         * @brief Handles fade in and fade out animations of the Overlay
         *
         */
        void animationLoop() {
            
        
            if (this->m_fadeInAnimationPlaying) {
                if (this->m_animationCounter < MAX_ANIMATION_COUNTER) {
                    this->m_animationCounter++;
                }
                
                if (this->m_animationCounter >= MAX_ANIMATION_COUNTER) {
                    this->m_fadeInAnimationPlaying = false;
                }
            }
            
            if (this->m_fadeOutAnimationPlaying) {
                if (this->m_animationCounter > 0) {
                    this->m_animationCounter--;
                }
                
                if (this->m_animationCounter == 0) {
                    this->m_fadeOutAnimationPlaying = false;
                    this->m_shouldHide = true;
                }
            }
        
            // Calculate and set the opacity using an easing function
            gfx::Renderer::setOpacity(calculateEaseInOut(static_cast<float>(this->m_animationCounter) / MAX_ANIMATION_COUNTER));
        }


        
        /**
         * @brief Overlay Main loop
         *
         */
        void loop(bool promptOnly = false) {
            // Early exit check - avoid all work if shutting down
            if (ult::launchingOverlay.load(std::memory_order_acquire)) {
                return;
            }
            
            // CRITICAL: Initialize to TRUE because stacks are added in init()!
            static std::atomic<bool> screenshotStacksAdded{true};
            static std::atomic<bool> notificationCacheNeedsClearing{false};

            auto& renderer = gfx::Renderer::get();
            renderer.startFrame();
        
            // Handle main UI rendering
            if (!promptOnly) {

                // In normal mode, ensure screenshots are enabled
                // Only re-add if they were removed AND force-disable is not set
                if (!screenshotStacksAdded.load(std::memory_order_acquire) &&
                    !screenshotsAreForceDisabled.load(std::memory_order_acquire)) {
                    if (!screenshotStacksAdded.exchange(true, std::memory_order_acq_rel)) {
                        renderer.addScreenshotStacks(false);
                    }
                }
                
                this->animationLoop();
                this->getCurrentGui()->update();
                this->getCurrentGui()->draw(&renderer);

            } else {
                // Prompt-only mode - temporarily remove screenshots
                if (screenshotStacksAdded.load(std::memory_order_acquire) &&
                    !screenshotsAreDisabled.load(std::memory_order_acquire) &&
                    !screenshotsAreForceDisabled.load(std::memory_order_acquire)) {

                    if (screenshotStacksAdded.exchange(false, std::memory_order_acq_rel)) {
                        renderer.removeScreenshotStacks(false);
                    }
                }
                renderer.clearScreen();
            }
        
            // Notification handling — safe, consistent, and null-guarded
            {
                if (notification && notification->isActive()) {
                    #if IS_STATUS_MONITOR_DIRECTIVE
                    if (isRendering && !wasRendering) {
                        isRendering  = false;
                        wasRendering = true;
                        leventSignal(&renderingStopEvent);
                    }
                    #endif
                    notification->update();
                    notification->draw(&renderer, promptOnly);

                    // Only set flag if it's not already set
                    notificationCacheNeedsClearing.exchange(true, std::memory_order_acq_rel);

                } else if (notificationCacheNeedsClearing.exchange(false, std::memory_order_acq_rel)) {
                    tsl::gfx::FontManager::clearNotificationCache();
                    #if IS_STATUS_MONITOR_DIRECTIVE
                    if (wasRendering) {
                        pendingExit = false;
                        wasRendering = false;
                        isRendering = true;
                        leventClear(&renderingStopEvent);
                    }
                    #endif
                }
            }
        
            renderer.endFrame();
        }
        
        // Calculate transition using ease-in-out curve instead of linear
        float easeInOutCubic(float t) {
            return t < 0.5f ? 4.0f * t * t * t : 1.0f - pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        }
        
        

        void handleInput(u64 keysDown, u64 keysHeld, bool touchDetected, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
            if (!ult::internalTouchReleased.load(std::memory_order_acquire) || ult::launchingOverlay.load(std::memory_order_acquire))
                return;

            static HidTouchState initialTouchPos = { 0 };
            static HidTouchState oldTouchPos = { 0 };
            static bool oldTouchDetected = false;
            static elm::TouchEvent touchEvent, oldTouchEvent;
            static u64 buttonPressTime_ns = 0, lastKeyEventTime_ns = 0, keyEventInterval_ns = 67000000ULL;
            static bool singlePressHandled = false;
            static bool notificationTouchConsumed = false;
            static constexpr u64 CLICK_THRESHOLD_NS = 340000000ULL;
            static bool hasScrolled = false;
            static void* lastGuiPtr = nullptr;
            static std::array<bool, 4> lastSimulatedTouch = {};
        
            auto& currentGui = this->getCurrentGui();

            if (!currentGui) {
                elm::Element::setInputMode(InputMode::Controller);
                oldTouchPos = { 0 };
                initialTouchPos = { 0 };
                touchEvent = elm::TouchEvent::None;
                ult::stillTouching.store(false, std::memory_order_release);
                ult::interruptedTouch.store(false, std::memory_order_release);
                return;
            }

            auto currentFocus = currentGui->getFocusedElement();

            // Only suppress OK when the focused item has actually been scrolled
            // *out of* the visible viewport by a long table.  A tiny table at the
            // bottom (where the last list item is still fully on screen) must NOT
            // trigger suppression, so we check real pixel bounds rather than just
            // the isTableScrolling flag.
            const bool blockOkAction = currentFocus &&
                tsl::elm::isTableScrolling.load(std::memory_order_acquire) &&
                (currentFocus->getTopBound()    < static_cast<s32>(ult::activeHeaderHeight) ||
                 currentFocus->getBottomBound() > static_cast<s32>(tsl::cfg::FramebufferHeight) - 73);

            // Focus color debounce — snap true immediately, delay false.
            // Also treat out-of-view table-scroll as "unfocused" so the OK
            // button greys out and its footer touch is suppressed while the
            // focused item is off-screen.
            {
                static u64 focusLostTime_ns = 0;
                static constexpr u64 UNFOCUS_DELAY_NS = 10'000'000ULL;
                if (currentFocus && !blockOkAction) {
                    usingUnfocusedColor = true;
                    focusLostTime_ns = 0;
                } else {
                    if (!bypassUnfocused) {
                        const u64 now = ult::nowNs();
                        if (focusLostTime_ns == 0) focusLostTime_ns = now;
                        if (now - focusLostTime_ns >= UNFOCUS_DELAY_NS) usingUnfocusedColor = false;
                    } else {
                        usingUnfocusedColor = true;
                    }
                }
            }

            const bool interpreterIsRunning = ult::runningInterpreter.load(std::memory_order_acquire);

        #if !IS_STATUS_MONITOR_DIRECTIVE
            if (interpreterIsRunning) {
                const struct { u64 key; FocusDirection dir; } shakes[] = {
                    {KEY_UP,    FocusDirection::Up},
                    {KEY_DOWN,  FocusDirection::Down},
                    {KEY_LEFT,  FocusDirection::Left},
                    {KEY_RIGHT, FocusDirection::Right},
                };
                for (auto& s : shakes) {
                    if (keysDown & s.key && !(keysHeld & ~s.key & ALL_KEYS_MASK)) {
                        currentFocus->shakeHighlight(s.dir);
                        return;
                    }
                }
            }
        #endif

        #if IS_STATUS_MONITOR_DIRECTIVE
            if (FullMode && !deactivateOriginalFooter) {
                if ((keysDown & ALL_KEYS_MASK) && ult::stillTouching && ult::currentForeground.load(std::memory_order_acquire)) {
                    triggerWallFeedback();
                    return;
                }
                if (ult::simulatedSelect.exchange(false, std::memory_order_acq_rel)) keysDown |= KEY_A;
                if (ult::simulatedBack.exchange(false, std::memory_order_acq_rel))   keysDown |= KEY_B;
                if (!overrideBackButton) {
                    if (keysDown & KEY_B && !(keysHeld & ~KEY_B & ALL_KEYS_MASK)) {
                        if (!currentGui->handleInput(KEY_B,0,{},{},{})) {
                            this->goBack();
                            if (this->m_guiStack.size() >= 1) triggerExitFeedback();
                        }
                        return;
                    }
                }
            } else {
                ult::simulatedSelect.exchange(false, std::memory_order_acq_rel);
                ult::simulatedBack.exchange(false, std::memory_order_acq_rel);
            }
        #else
            if (ult::simulatedSelect.exchange(false, std::memory_order_acq_rel)) keysDown |= KEY_A;
            if (ult::simulatedBack.exchange(false, std::memory_order_acq_rel))   keysDown |= KEY_B;

            if ((keysDown & ALL_KEYS_MASK) && ult::stillTouching && ult::currentForeground.load(std::memory_order_acquire)) {
                triggerWallFeedback();
                return;
            }
            if (!overrideBackButton) {
                if (keysDown & KEY_B && !(keysHeld & ~KEY_B & ALL_KEYS_MASK)) {
                    if (!currentGui->handleInput(KEY_B,0,{},{},{})) {
                        this->goBack();
                        if (this->m_guiStack.size() >= 1 && !interpreterIsRunning) triggerExitFeedback();
                    }
                    return;
                }
            } else {
                #if IS_LAUNCHER_DIRECTIVE
                if (keysDown & KEY_B && !(keysHeld & ~KEY_B & ALL_KEYS_MASK)) {
                    if (this->m_guiStack.size() >= 1 && !interpreterIsRunning) triggerExitFeedback();
                }
                #endif
            }
        #endif
        
            if (currentGui.get() != lastGuiPtr) {
                hasScrolled = false;
                oldTouchEvent = elm::TouchEvent::None;
                oldTouchDetected = false;
                oldTouchPos = { 0 };
                initialTouchPos = { 0 };
                lastGuiPtr = currentGui.get();
            }
            
            auto topElement = currentGui->getTopElement();
            const u64 currentTime_ns = ult::nowNs();

            if (!currentFocus && !ult::simulatedBack.load(std::memory_order_acquire) &&
                !ult::stillTouching.load(std::memory_order_acquire) && !oldTouchDetected && !interpreterIsRunning) {
                if (!topElement) return;
                if (!currentGui->initialFocusSet() && !(keysDown & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT))) {
                    currentGui->requestFocus(topElement, FocusDirection::None);
                    currentGui->markInitialFocusSet();
                }
            }

            if (isNavigatingBackwards.load(std::memory_order_acquire) && !currentFocus && topElement &&
                keysDown & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
                currentGui->requestFocus(topElement, FocusDirection::None);
                currentGui->markInitialFocusSet();
                isNavigatingBackwards.store(false, std::memory_order_release);
                buttonPressTime_ns = lastKeyEventTime_ns = currentTime_ns;
                singlePressHandled = false;
            }
            
            if (!currentFocus && !touchDetected && (!oldTouchDetected || oldTouchEvent == elm::TouchEvent::Scroll)) {
                if (!isNavigatingBackwards.load(std::memory_order_acquire) &&
                    !ult::shortTouchAndRelease.load(std::memory_order_acquire) &&
                    !ult::longTouchAndRelease.load(std::memory_order_acquire) &&
                    !ult::simulatedSelect.load(std::memory_order_acquire) &&
                    !ult::simulatedBack.load(std::memory_order_acquire) &&
                    !ult::simulatedNextPage.load(std::memory_order_acquire) && topElement) {
                    if (oldTouchEvent == elm::TouchEvent::Scroll) hasScrolled = true;
                    if (!hasScrolled) {
                        currentGui->removeFocus();
                        currentGui->requestFocus(topElement, FocusDirection::None);
                    }
                } else if (ult::longTouchAndRelease.exchange(false, std::memory_order_acq_rel) ||
                           ult::shortTouchAndRelease.exchange(false, std::memory_order_acq_rel)) {
                    hasScrolled = true;
                }
            }
            
            // Suppress OK only when the focused item is genuinely off-screen —
            // blockOkAction is false for small tables where the item is visible.
            if (blockOkAction)
                keysDown &= ~KEY_A;

            bool handled = false;
            for (elm::Element* p = currentFocus; !handled && p; p = p->getParent())
                handled = p->onClick(keysDown) || p->handleInput(keysDown, keysHeld, touchPos, joyStickPosLeft, joyStickPosRight);
            
            if (currentGui != this->getCurrentGui()) return;
            handled |= currentGui->handleInput(keysDown, keysHeld, touchPos, joyStickPosLeft, joyStickPosRight);

            // Directional key release tracking
            {
                static bool lastDirectionPressed = true;
                const bool directionPressed = (keysHeld & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) != 0;
                if (!directionPressed && lastDirectionPressed)
                    tsl::elm::s_directionalKeyReleased.store(true, std::memory_order_release);
                else if (directionPressed && lastDirectionPressed)
                    tsl::elm::s_directionalKeyReleased.store(false, std::memory_order_release);
                lastDirectionPressed = directionPressed;
            }

            const float currentScrollVelocity = tsl::elm::s_currentScrollVelocity.load(std::memory_order_acquire);
            const u64 velMask = currentScrollVelocity != 0.0f ? (KEY_A | KEY_UP) : KEY_UP;
            const bool singleArrowKeyPress =
                ((keysHeld & KEY_UP) != 0) + ((keysHeld & KEY_DOWN) != 0) +
                ((keysHeld & KEY_LEFT) != 0) + ((keysHeld & KEY_RIGHT) != 0) == 1 &&
                !(keysHeld & ~(velMask | KEY_DOWN | KEY_LEFT | KEY_RIGHT) & ALL_KEYS_MASK);

            if (hasScrolled) {
                if (singleArrowKeyPress) {
                    buttonPressTime_ns = lastKeyEventTime_ns = currentTime_ns;
                    hasScrolled = false;
                    singlePressHandled = false;
                    isNavigatingBackwards.store(false, std::memory_order_release);
                    // On the very first directional press after touch scroll, restore focus
                    // to the nearest item and immediately navigate — no wasted frame.
                    if (topElement && currentGui && keysDown) {
                        currentGui->removeFocus();
                        currentGui->requestFocus(topElement, FocusDirection::None);
                        if      (keysHeld & KEY_UP   ) currentGui->requestFocus(topElement, FocusDirection::Up,    false);
                        else if (keysHeld & KEY_DOWN ) currentGui->requestFocus(topElement, FocusDirection::Down,  false);
                        else if (keysHeld & KEY_LEFT ) currentGui->requestFocus(topElement, FocusDirection::Left,  false);
                        else if (keysHeld & KEY_RIGHT) currentGui->requestFocus(topElement, FocusDirection::Right, false);
                    }
                }
            } else {
                if (!touchDetected && !oldTouchDetected && !handled && currentFocus &&
                    !ult::stillTouching.load(std::memory_order_acquire) && !interpreterIsRunning) {
                    static bool shouldShake = true;
                    if (singleArrowKeyPress) {
                        if (keysDown) {
                            buttonPressTime_ns = lastKeyEventTime_ns = currentTime_ns;
                            singlePressHandled = false;
                            if      (keysHeld & KEY_UP    && !(keysHeld & ~KEY_UP    & ALL_KEYS_MASK)) currentGui->requestFocus(topElement, FocusDirection::Up, shouldShake);
                            else if (keysHeld & KEY_DOWN  && !(keysHeld & ~KEY_DOWN  & ALL_KEYS_MASK)) currentGui->requestFocus(currentFocus ? currentFocus->getParent() : topElement, FocusDirection::Down, shouldShake);
                            else if (keysHeld & KEY_LEFT  && !(keysHeld & ~KEY_LEFT  & ALL_KEYS_MASK)) currentGui->requestFocus(currentFocus ? currentFocus->getParent() : topElement, FocusDirection::Left, shouldShake);
                            else if (keysHeld & KEY_RIGHT && !(keysHeld & ~KEY_RIGHT & ALL_KEYS_MASK)) currentGui->requestFocus(currentFocus ? currentFocus->getParent() : topElement, FocusDirection::Right, shouldShake);
                        }
                        if (keysHeld & ~KEY_DOWN & ~KEY_UP & ~KEY_LEFT & ~KEY_RIGHT & ALL_KEYS_MASK)
                            buttonPressTime_ns = currentTime_ns;
                        
                        const u64 durationSincePress_ns = currentTime_ns - buttonPressTime_ns;
                        if (!singlePressHandled && durationSincePress_ns >= CLICK_THRESHOLD_NS)
                            singlePressHandled = true;
                        
                        // Compute repeat interval with acceleration
                        {
                            const bool tableScroll = tsl::elm::isTableScrolling.load(std::memory_order_acquire);
                            const u64 tp  = tableScroll ? 200000000ULL  : 2000000000ULL;
                            const u64 ini = tableScroll ?  33000000ULL  :   67000000ULL;
                            const u64 sht = tableScroll ?   5000000ULL  :   10000000ULL;
                            const float t = durationSincePress_ns >= tp ? 1.0f : (float)durationSincePress_ns / (float)tp;
                            keyEventInterval_ns = (u64)((1.0f - t) * ini + t * sht);
                        }
                        
                        if (singlePressHandled && (currentTime_ns - lastKeyEventTime_ns) >= keyEventInterval_ns) {
                            lastKeyEventTime_ns = currentTime_ns;
                            const u64 upMask   = currentScrollVelocity != 0.0f ? (KEY_A | KEY_UP)   : KEY_UP;
                            const u64 downMask = currentScrollVelocity != 0.0f ? (KEY_A | KEY_DOWN) : KEY_DOWN;
                            if      (keysHeld & KEY_UP    && !(keysHeld & ~upMask   & ALL_KEYS_MASK)) currentGui->requestFocus(topElement, FocusDirection::Up, false);
                            else if (keysHeld & KEY_DOWN  && !(keysHeld & ~downMask & ALL_KEYS_MASK)) currentGui->requestFocus(currentFocus->getParent(), FocusDirection::Down, false);
                            else if (keysHeld & KEY_LEFT  && !(keysHeld & ~KEY_LEFT  & ALL_KEYS_MASK)) currentGui->requestFocus(currentFocus->getParent(), FocusDirection::Left, false);
                            else if (keysHeld & KEY_RIGHT && !(keysHeld & ~KEY_RIGHT & ALL_KEYS_MASK)) currentGui->requestFocus(currentFocus->getParent(), FocusDirection::Right, false);
                        }
        #if !IS_STATUS_MONITOR_DIRECTIVE
                    } else {
                        buttonPressTime_ns = lastKeyEventTime_ns = currentTime_ns;
        #endif
                    }
                }
            }
        
        #if !IS_STATUS_MONITOR_DIRECTIVE
            if (!touchDetected && !interpreterIsRunning && topElement) {
        #else
            if (!disableJumpTo && !touchDetected && !interpreterIsRunning && topElement) {
        #endif
                static constexpr u64 INITIAL_HOLD_THRESHOLD_NS = 400000000ULL;
                static constexpr u64 HOLD_THRESHOLD_NS         = 300000000ULL;
                static constexpr u64 RAPID_CLICK_WINDOW_NS     = 500000000ULL;
                static constexpr u64 RAPID_MODE_TIMEOUT_NS     = 1000000000ULL;
                static constexpr u64 ACCELERATION_POINT_NS     = 1500000000ULL;
                static constexpr u64 INITIAL_INTERVAL_NS       =   67000000ULL;
                static constexpr u64 FAST_INTERVAL_NS          =   10000000ULL;

                const bool lKeyPressed  = (keysHeld & KEY_L);
                const bool rKeyPressed  = (keysHeld & KEY_R);
                const bool zlKeyPressed = (keysHeld & KEY_ZL);
                const bool zrKeyPressed = (keysHeld & KEY_ZR);
                const bool notlKeyPressed  = (keysHeld & ~KEY_L  & ALL_KEYS_MASK);
                const bool notrKeyPressed  = (keysHeld & ~KEY_R  & ALL_KEYS_MASK);
                const bool notzlKeyPressed = (keysHeld & ~KEY_ZL & ALL_KEYS_MASK);
                const bool notzrKeyPressed = (keysHeld & ~KEY_ZR & ALL_KEYS_MASK);

                struct JumpButtonState { bool keyWasPressed = false, wasIsolated = false; u64 pressStart_ns = 0; };
                static JumpButtonState lState, rState;
                
                auto handleJumpButton = [&](JumpButtonState& s, bool keyPressed, bool notKeyPressed, std::atomic<bool>& jumpSignal) {
                    if (keyPressed) {
                        if (!s.keyWasPressed) { s.pressStart_ns = currentTime_ns; s.wasIsolated = !notKeyPressed; }
                        if (notKeyPressed) s.wasIsolated = false;  // another key pressed during hold — cancel
                        s.keyWasPressed = true;
                    } else {
                        if (s.keyWasPressed && s.wasIsolated && !notKeyPressed &&
                            currentTime_ns - s.pressStart_ns < INITIAL_HOLD_THRESHOLD_NS) {
                            jumpSignal.store(true, std::memory_order_release);
                            currentGui->requestFocus(topElement, FocusDirection::None);
                        }
                        s.keyWasPressed = s.wasIsolated = false;
                    }
                };
                handleJumpButton(lState, lKeyPressed, notlKeyPressed, jumpToTop);
                handleJumpButton(rState, rKeyPressed, notrKeyPressed, jumpToBottom);

                struct SkipButtonState {
                    u64  lastClickTime_ns = 0, firstClickPressStart_ns = 0, buttonPressStart_ns = 0, lastHoldTrigger_ns = 0;
                    bool keyWasPressed = false, wasIsolated = false, inRapidClickMode = false, holdTriggered = false;
                };
                static SkipButtonState zlState, zrState;
                
                auto handleSkipButton = [&](SkipButtonState& s, bool keyPressed, bool notKeyPressed, std::atomic<bool>& skipSignal) {
                    if (s.inRapidClickMode && (currentTime_ns - s.lastClickTime_ns) > RAPID_MODE_TIMEOUT_NS)
                        s.inRapidClickMode = false;
                    if (keyPressed) {
                        if (!s.keyWasPressed) {
                            s.wasIsolated = !notKeyPressed;
                            if (!s.inRapidClickMode) s.firstClickPressStart_ns = currentTime_ns;
                            if (currentTime_ns - s.lastClickTime_ns <= RAPID_CLICK_WINDOW_NS) s.inRapidClickMode = true;
                            if (s.inRapidClickMode && s.wasIsolated) {
                                skipSignal.store(true, std::memory_order_release);
                                currentGui->requestFocus(topElement, FocusDirection::None);
                                s.lastClickTime_ns = currentTime_ns;
                            }
                            s.buttonPressStart_ns = s.lastHoldTrigger_ns = currentTime_ns;
                            s.holdTriggered = false;
                        }
                        if (notKeyPressed) s.wasIsolated = false;  // another key pressed during hold — cancel
                        if (s.inRapidClickMode && s.wasIsolated) {
                            const u64 holdDuration = currentTime_ns - s.buttonPressStart_ns;
                            if (holdDuration >= HOLD_THRESHOLD_NS) {
                                const float t = holdDuration >= ACCELERATION_POINT_NS ? 1.0f : (float)holdDuration / ACCELERATION_POINT_NS;
                                const u64 interval = (u64)((1.0f - t) * INITIAL_INTERVAL_NS + t * FAST_INTERVAL_NS);
                                if (!s.holdTriggered || (currentTime_ns - s.lastHoldTrigger_ns) >= interval) {
                                    skipSignal.store(true, std::memory_order_release);
                                    currentGui->requestFocus(topElement, FocusDirection::None);
                                    s.holdTriggered = true;
                                    s.lastHoldTrigger_ns = s.lastClickTime_ns = currentTime_ns;
                                }
                            }
                        }
                        s.keyWasPressed = true;
                    } else {
                        if (s.keyWasPressed && !s.inRapidClickMode && s.wasIsolated && !notKeyPressed &&
                            currentTime_ns - s.firstClickPressStart_ns < INITIAL_HOLD_THRESHOLD_NS) {
                            skipSignal.store(true, std::memory_order_release);
                            currentGui->requestFocus(topElement, FocusDirection::None);
                            s.lastClickTime_ns = currentTime_ns;
                            s.inRapidClickMode = true;
                        }
                        s.keyWasPressed = s.wasIsolated = false;
                    }
                };
                handleSkipButton(zlState, zlKeyPressed, notzlKeyPressed, skipUp);
                handleSkipButton(zrState, zrKeyPressed, notzrKeyPressed, skipDown);
            }

            // Notification touch consume
            if (!oldTouchDetected && touchDetected) {
                if (notification && notification->hitTest(static_cast<s32>(touchPos.x), static_cast<s32>(touchPos.y))) {
                    notification->dismissAt(static_cast<s32>(touchPos.x), static_cast<s32>(touchPos.y));
                    notificationTouchConsumed = true;
                }
            }
            if (!touchDetected && oldTouchDetected) notificationTouchConsumed = false;
            if (notificationTouchConsumed) {
                oldTouchDetected = touchDetected;
                oldTouchPos      = touchPos;
                return;
            }
            
            if (!touchDetected && oldTouchDetected && currentGui && topElement)
                topElement->onTouch(elm::TouchEvent::Release, oldTouchPos.x, oldTouchPos.y, oldTouchPos.x, oldTouchPos.y, initialTouchPos.x, initialTouchPos.y);

            // Footer touch regions
            const float edgePadding    = ult::halfGap.load(std::memory_order_acquire) - 5;
            const float backLeftEdge   = edgePadding + ult::layerEdge;
            const float backRightEdge  = backLeftEdge  + ult::backWidth.load(std::memory_order_acquire);
            const float selectLeftEdge = backRightEdge;
            const float selectRightEdge = selectLeftEdge + ult::selectWidth.load(std::memory_order_acquire);
            const bool  noClickable    = ult::noClickableItems.load(std::memory_order_acquire);
            const float nextPageLeftEdge  = noClickable ? backRightEdge : selectRightEdge;
            const float nextPageRightEdge = nextPageLeftEdge + ult::nextPageWidth.load(std::memory_order_acquire);
            const float menuRightEdge  = 245.0f + ult::layerEdge - 13;
            const u32   footerY        = cfg::FramebufferHeight - 73U + 1;

            const bool backTouched = (touchPos.x >= backLeftEdge && touchPos.x < backRightEdge && touchPos.y > footerY) &&
                                     (initialTouchPos.x >= backLeftEdge && initialTouchPos.x < backRightEdge && initialTouchPos.y > footerY);
            const bool selectTouched = !noClickable &&
                                       (touchPos.x >= selectLeftEdge && touchPos.x < selectRightEdge && touchPos.y > footerY) &&
                                       (initialTouchPos.x >= selectLeftEdge && initialTouchPos.x < selectRightEdge && initialTouchPos.y > footerY);
            const bool nextPageTouched = ult::hasNextPageButton.load(std::memory_order_acquire) &&
                                         (touchPos.x >= nextPageLeftEdge && touchPos.x < nextPageRightEdge && touchPos.y > footerY) &&
                                         (initialTouchPos.x >= nextPageLeftEdge && initialTouchPos.x < nextPageRightEdge && initialTouchPos.y > footerY);
            const bool menuTouched = (touchPos.x > ult::layerEdge+7U && touchPos.x <= menuRightEdge && touchPos.y > 10U && touchPos.y <= 83U) &&
                                     (initialTouchPos.x > ult::layerEdge+7U && initialTouchPos.x <= menuRightEdge && initialTouchPos.y > 10U && initialTouchPos.y <= 83U);

            bool shouldTriggerRumble = false;
            auto checkTouched = [&](bool touched, std::atomic<bool>& state) {
                if (touched != state.exchange(touched, std::memory_order_acq_rel) && touched)
                    shouldTriggerRumble = true;
            };
            checkTouched(backTouched,     ult::touchingBack);
            if (usingUnfocusedColor) checkTouched(selectTouched, ult::touchingSelect);
            checkTouched(nextPageTouched, ult::touchingNextPage);
            if (menuTouched != ult::touchingMenu.exchange(menuTouched, std::memory_order_acq_rel)) {
                if (menuTouched && (ult::inMainMenu.load(std::memory_order_acquire) ||
                    (ult::inHiddenMode.load(std::memory_order_acquire) &&
                     !ult::inSettingsMenu.load(std::memory_order_acquire) &&
                     !ult::inSubSettingsMenu.load(std::memory_order_acquire))))
                    shouldTriggerRumble = true;
            }
            if (shouldTriggerRumble) triggerNavigationFeedback();

            if (touchDetected) {
                lastSimulatedTouch = {backTouched, selectTouched, nextPageTouched, menuTouched};
                //const bool touchInFooter = (touchPos.y > static_cast<u32>(cfg::FramebufferHeight - 73U + 1));
                ult::interruptedTouch.store(!(touchPos.y > static_cast<u32>(cfg::FramebufferHeight - 73U + 1)) && (keysHeld & ALL_KEYS_MASK) != 0, std::memory_order_release);

                const u32 xd = std::abs(static_cast<s32>(initialTouchPos.x) - static_cast<s32>(touchPos.x));
                const u32 yd = std::abs(static_cast<s32>(initialTouchPos.y) - static_cast<s32>(touchPos.y));
                if (xd*xd + yd*yd > 1000) {
                    elm::Element::setInputMode(InputMode::TouchScroll);
                    touchEvent = elm::TouchEvent::Scroll;
                } else if (touchEvent != elm::TouchEvent::Scroll) {
                    touchEvent = elm::TouchEvent::Hold;
                }
                
                if (!oldTouchDetected) {
                    initialTouchPos = touchPos;
                    elm::Element::setInputMode(InputMode::Touch);
                    if (!interpreterIsRunning) {
                        ult::touchInBounds = (initialTouchPos.y <= footerY && initialTouchPos.y > 73U &&
                                              initialTouchPos.x <= ult::layerEdge + cfg::FramebufferWidth - 30U &&
                                              initialTouchPos.x > 40U + ult::layerEdge);
                        if (ult::touchInBounds) currentGui->removeFocus();
                    }
                    touchEvent = elm::TouchEvent::Touch;
                }
                
                if (currentGui && topElement && !interpreterIsRunning) {
                    if (touchPos.x > 40U + ult::layerEdge && touchPos.x <= cfg::FramebufferWidth - 30U + ult::layerEdge &&
                        touchPos.y > 73U && touchPos.y <= footerY)
                        currentGui->removeFocus();
                    topElement->onTouch(touchEvent, touchPos.x, touchPos.y, oldTouchPos.x, oldTouchPos.y, initialTouchPos.x, initialTouchPos.y);
                }
                
                oldTouchPos = touchPos;
                if ((touchPos.x < ult::layerEdge || touchPos.x > cfg::FramebufferWidth + ult::layerEdge) &&
                    tsl::elm::Element::getInputMode() == tsl::InputMode::Touch) {
                    oldTouchPos = { 0 };
                    initialTouchPos = { 0 };
            #if IS_STATUS_MONITOR_DIRECTIVE
                    if (FullMode && !deactivateOriginalFooter) this->hide();
            #else
                    if (!disableHiding) this->hide();
            #endif
                }
                ult::stillTouching.store(true, std::memory_order_release);
            } else {
                for (int i = 0; i < 4; ++i) {
                    if (!lastSimulatedTouch[i]) continue;
                    if (!ult::interruptedTouch.load(std::memory_order_acquire) && !interpreterIsRunning) {
                        switch (i) {
                            case 0: ult::simulatedBack.store(true,     std::memory_order_release); break;
                            case 1: ult::simulatedSelect.store(true,   std::memory_order_release); break;
                            case 2: ult::simulatedNextPage.store(true, std::memory_order_release); break;
                            case 3: ult::simulatedMenu.store(true,     std::memory_order_release); break;
                        }
                    } else if (interpreterIsRunning) {
                        switch (i) {
                            case 0: this->hide(); break;
                            case 1: ult::externalAbortCommands.store(true, std::memory_order_release); break;
                        }
                    }
                }
                lastSimulatedTouch.fill(false);
                elm::Element::setInputMode(InputMode::Controller);
                oldTouchPos = { 0 };
                initialTouchPos = { 0 };
                touchEvent = elm::TouchEvent::None;
                ult::stillTouching.store(false, std::memory_order_release);
                ult::interruptedTouch.store(false, std::memory_order_release);
            }
            
            oldTouchDetected = touchDetected;
            oldTouchEvent = touchEvent;
        }
        

        /**
         * @brief Clears the screen
         *
         */
        void clearScreen() {
            auto& renderer = gfx::Renderer::get();
            
            renderer.startFrame();
            renderer.clearScreen();
            renderer.endFrame();
        }
        
        /**
         * @brief Reset hide and close flags that were previously set by \ref Overlay::close() or \ref Overlay::hide()
         *
         */
        void resetFlags() {
            this->m_shouldHide = false;
            this->m_shouldClose = false;
            this->m_shouldCloseAfter = false;
        }
        
        /**
         * @brief Disables the next animation that would play
         *
         */
        void disableNextAnimation() {
            this->m_disableNextAnimation = true;
        }
        

        /**
         * @brief Changes to a different Gui
         *
         * @param gui Gui to change to
         * @return Reference to the Gui
         */
        std::unique_ptr<tsl::Gui>& changeTo(std::unique_ptr<tsl::Gui>&& gui, bool clearGlyphCache = false) {
            if (this->m_guiStack.top() != nullptr && this->m_guiStack.top()->m_focusedElement != nullptr)
                this->m_guiStack.top()->m_focusedElement->resetClickAnimation();
            
            isNavigatingBackwards.store(false, std::memory_order_release);

            // cache frame for forward rendering using external list method (to be implemented)

            // Create the top element of the new Gui
            gui->m_topElement = gui->createUI();

            
            // Push the new Gui onto the stack
            this->m_guiStack.push(std::move(gui));

            return this->m_guiStack.top();
        }

        
        /**
         * @brief Creates a new Gui and changes to it
         *
         * @tparam G Gui to create
         * @tparam Args Arguments to pass to the Gui
         * @param args Arguments to pass to the Gui
         * @return Reference to the newly created Gui
         */
        // Template version without clearGlyphCache (for backward compatibility)
        template<typename G, typename ...Args>
        std::unique_ptr<tsl::Gui>& changeTo(Args&&... args) {
            return this->changeTo(std::make_unique<G>(std::forward<Args>(args)...), false);
        }
        

        /**
         * @brief Swaps to a different Gui
         *
         * @param gui Gui to change to
         * @return Reference to the Gui
         */
        std::unique_ptr<tsl::Gui>& swapTo(std::unique_ptr<tsl::Gui>&& gui, u32 count = 1) {

            isNavigatingBackwards.store(true, std::memory_order_release);
            
            // Clamp count to available stack size to prevent underflow
            const u32 actualCount = std::min(count, static_cast<u32>(this->m_guiStack.size()));
            
            if (actualCount > 1) {
                // Pop the specified number of GUIs
                for (u32 i = 0; i < actualCount; ++i) {
                    this->m_guiStack.pop();
                }
            } else {
                this->m_guiStack.pop();
            }



            if (this->m_guiStack.top() != nullptr && this->m_guiStack.top()->m_focusedElement != nullptr)
                this->m_guiStack.top()->m_focusedElement->resetClickAnimation();
            
            isNavigatingBackwards.store(false, std::memory_order_release);

            // Create the top element of the new Gui
            gui->m_topElement = gui->createUI();

            
            // Push the new Gui onto the stack
            this->m_guiStack.push(std::move(gui));

            return this->m_guiStack.top();
        }

        /**
         * @brief Creates a new Gui and changes to it
         *
         * @tparam G Gui to create
         * @tparam Args Arguments to pass to the Gui
         * @param args Arguments to pass to the Gui
         * @return Reference to the newly created Gui
         */
        // Template version without clearGlyphCache (for backward compatibility)
        template<typename G, typename ...Args>
        std::unique_ptr<tsl::Gui>& swapTo(SwapDepth depth, Args&&... args) {
            return this->swapTo(std::make_unique<G>(std::forward<Args>(args)...), depth.value);
        }
        
        template<typename G, typename ...Args>
        std::unique_ptr<tsl::Gui>& swapTo(Args&&... args) {
            return this->swapTo(std::make_unique<G>(std::forward<Args>(args)...), 1);
        }
        
        /**
         * @brief Pops the top Gui(s) from the stack and goes back count number of times
         * @param count Number of Guis to pop from the stack (default: 1)
         * @note The Overlay gets closed once there are no more Guis on the stack
         */
        void goBack(u32 count = 1) {
            if (ult::stillTouching && ult::currentForeground.load(std::memory_order_acquire)) {
                triggerWallFeedback();
                return;
            }

            // If there is exactly one GUI and an active notification, handle that first
            if (this->m_guiStack.size() == 1 && notification && notification->isActive()) {
                this->close(); 
                return;
            }
        
            isNavigatingBackwards.store(true, std::memory_order_release);
        
            // Clamp count to available stack size to prevent underflow
            const u32 actualCount = std::min(count, static_cast<u32>(this->m_guiStack.size()));
        
            // Special case: if we don't close on exit and popping everything would leave us with 0 or 1 GUI
            if (!this->m_closeOnExit && this->m_guiStack.size() <= actualCount) {
                this->hide();
                return;
            }
        
            // Pop the specified number of GUIs
            for (u32 i = 0; i < actualCount && !this->m_guiStack.empty(); ++i) {
                this->m_guiStack.pop();
            }
            
            // Close overlay if stack is empty
            if (this->m_guiStack.empty()) {
                this->close();
            }

        }

        void pop(u32 count = 1) {

            if (ult::stillTouching && ult::currentForeground.load(std::memory_order_acquire)) {
                triggerWallFeedback();
                return;
            }

            isNavigatingBackwards.store(true, std::memory_order_release);
            
            // Clamp count to available stack size to prevent underflow
            const u32 actualCount = std::min(count, static_cast<u32>(this->m_guiStack.size()));

            if (actualCount > 1) {
                // Pop the specified number of GUIs
                for (u32 i = 0; i < actualCount; ++i) {
                    this->m_guiStack.pop();
                }
            } else {
                this->m_guiStack.pop();
            }
        }


        
        template<typename G, typename ...Args>
        friend std::unique_ptr<tsl::Gui>& changeTo(Args&&... args);
        template<typename G, typename ...Args>
        friend std::unique_ptr<tsl::Gui>& swapTo(Args&&... args);
        
        template<typename G, typename ...Args>
        friend std::unique_ptr<tsl::Gui>& swapTo(SwapDepth depth, Args&&... args);
        
        friend void goBack(u32 count);
        friend void pop(u32 count);
        
        template<typename, tsl::impl::LaunchFlags>
        friend int loop(int argc, char** argv);
        
        friend class tsl::Gui;
    };
    
    
    namespace impl {
        static constexpr const char* TESLA_CONFIG_FILE = "/config/tesla/config.ini";
        static constexpr const char* ULTRAHAND_CONFIG_FILE = "/config/ultrahand/config.ini";
        
        /**
         * @brief Data shared between the different ult::renderThreads
         *
         */
        struct SharedThreadData {
            std::atomic<bool> running = false;
            
            Event comboEvent = { 0 };
            
            std::atomic<bool> overlayOpen = false;
            
            std::mutex dataMutex;
            u64 keysDown = 0;
            u64 keysDownPending = 0;
            u64 keysHeld = 0;
            HidTouchScreenState touchState = { 0 };
            HidAnalogStickState joyStickPosLeft = { 0 }, joyStickPosRight = { 0 };
        };
        
        
        /**
         * @brief Extract values from Tesla settings file
         *
         */
        void parseOverlaySettings();

        /**
         * @brief Update and save launch combo keys
         *
         * @param keys the new combo keys
         */
        static void updateCombo(u64 keys) {
            tsl::cfg::launchCombo = keys;
            const std::string comboStr = tsl::hlp::keysToComboString(keys);
            ult::setIniFileValue(ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME, ult::KEY_COMBO_STR, comboStr);
            ult::setIniFileValue(ult::TESLA_CONFIG_INI_PATH,     ult::TESLA_STR,              ult::KEY_COMBO_STR, comboStr);
        }
        
        static auto currentUnderscanPixels = std::make_pair(0, 0);

        /**
         * @brief Background event polling loop thread
         *
         * @param args Used to pass in a pointer to a \ref SharedThreadData struct
         */
        static void backgroundEventPoller(void *args) {
            requiresLNY2 = amsVersionAtLeast(1,10,0);     // Detect if using HOS 21+

            // Initialize the audio service
            if (ult::useSoundEffects && !ult::limitedMemory) {
                ult::Audio::initialize();
            }

            tsl::hlp::loadEntryKeyCombos();
            ult::launchingOverlay.store(false, std::memory_order_release);
        
            SharedThreadData *shData = static_cast<SharedThreadData*>(args);

            const auto fireLaunch = [&]() __attribute__((noinline)) {
                ult::launchingOverlay.store(true, std::memory_order_release);
                tsl::Overlay::get()->close();
                eventFire(&shData->comboEvent);
                launchComboHasTriggered.store(true, std::memory_order_release);
            };
            
            // To prevent focus glitchout, close the overlay immediately when the home button gets pressed
            Event homeButtonPressEvent = {};
            hidsysAcquireHomeButtonEventHandle(&homeButtonPressEvent, false);
            eventClear(&homeButtonPressEvent);
            tsl::hlp::ScopeGuard homeButtonEventGuard([&] { eventClose(&homeButtonPressEvent); });
            
            // To prevent focus glitchout, close the overlay immediately when the power button gets pressed
            Event powerButtonPressEvent = {};
            hidsysAcquireSleepButtonEventHandle(&powerButtonPressEvent, false);
            eventClear(&powerButtonPressEvent);
            tsl::hlp::ScopeGuard powerButtonEventGuard([&] { eventClose(&powerButtonPressEvent); });
            
        
            // For handling screenshots color alpha
            Event captureButtonPressEvent = {};
            for (int i = 0; i < 2; i++) {
                hidsysAcquireCaptureButtonEventHandle(&captureButtonPressEvent, false);
                eventClear(&captureButtonPressEvent);
            }
            tsl::hlp::ScopeGuard captureButtonEventGuard([&] { eventClose(&captureButtonPressEvent); });
            
        
            // Allow only Player 1 and handheld mode
            HidNpadIdType id_list[2] = { HidNpadIdType_No1, HidNpadIdType_Handheld };
            
            // Configure HID system to only listen to these IDs
            hidSetSupportedNpadIdType(id_list, 2);
            
            // Configure input for up to 2 supported controllers (P1 + Handheld)
            padConfigureInput(2, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadSystemExt);
            
            // Initialize separate pad states for both controllers
            auto pad_p1_ptr = std::make_unique<PadState>();
            auto pad_handheld_ptr = std::make_unique<PadState>();
            PadState& pad_p1 = *pad_p1_ptr;
            PadState& pad_handheld = *pad_handheld_ptr;
            padInitialize(&pad_p1, HidNpadIdType_No1);
            padInitialize(&pad_handheld, HidNpadIdType_Handheld);
            
            // Touch screen init
            hidInitializeTouchScreen();
            
            // Clear any stale input from both controllers
            padUpdate(&pad_p1);
            padUpdate(&pad_handheld);

            //ult::initHaptics(); // initialize rumble
            
            enum WaiterObject {
                WaiterObject_HomeButton,
                WaiterObject_PowerButton,
                WaiterObject_CaptureButton,
                WaiterObject_Count
            };
            
            // Construct waiter
            Waiter objects[3] = {
                [WaiterObject_HomeButton] = waiterForEvent(&homeButtonPressEvent),
                [WaiterObject_PowerButton] = waiterForEvent(&powerButtonPressEvent),
                [WaiterObject_CaptureButton] = waiterForEvent(&captureButtonPressEvent),
            };
            
            u64 currentTouchTick = 0;
            auto lastTouchX = 0;
            auto lastTouchY = 0;
        
            // Preset touch boundaries
            constexpr int SWIPE_RIGHT_BOUND = 16;  // 16 + 80
            constexpr int SWIPE_LEFT_BOUND = (1280 - 16);
            constexpr u64 TOUCH_THRESHOLD_NS = 150'000'000ULL; // 150ms in nanoseconds
            constexpr u64 FAST_SWAP_THRESHOLD_NS = 150'000'000ULL;
        
            // Global underscan monitoring - run at most once every 300ms
            auto lastUnderscanPixels = std::make_pair(0, 0);
            bool firstUnderscanCheck = true;
            u64 lastUnderscanCheckNs = 0;  // store last execution in nanoseconds
            constexpr u64 UNDERSCAN_INTERVAL_NS = 300'000'000ULL; // 300ms in ns

            s32 idx;
            Result rc;
            
            std::string currentTitleID;
        
            u64 lastPollTick = 0;
            u64 resetStartTick = armGetSystemTick();
            const u64 startNs = armTicksToNs(resetStartTick);

            ult::lastTitleID = ult::getTitleIdAsString();

            // Notification variables
            u64 lastNotifCheck = 0;
            u64 minusHoldStartTick = 0;
            bool minusHoldArmed = false;
            
            while (shData->running.load(std::memory_order_acquire)) {

                u64 nowTick = armGetSystemTick();
                u64 nowNs = armTicksToNs(nowTick);
                
                // Scan for input changes from both controllers
                padUpdate(&pad_p1);
                padUpdate(&pad_handheld);
                
                // Read in HID values
                {

                    // Poll Title ID every 1 seconds
                    if (!ult::resetForegroundCheck.load(std::memory_order_acquire)) {
                        const u64 elapsedNs = armTicksToNs(nowTick - lastPollTick);
                        if (elapsedNs >= 1'000'000'000ULL) {
                            lastPollTick = nowTick;
                            
                            currentTitleID = ult::getTitleIdAsString();
                            if (currentTitleID != ult::lastTitleID) {
                                ult::lastTitleID = currentTitleID;
                                if (currentTitleID != ult::NULL_STR) {
                                    ult::resetForegroundCheck.store(true, std::memory_order_release);
                                    resetStartTick = nowTick;
                                }
                            }
                        }
                    }

                    // If a reset is scheduled, trigger after 3.5s delay
                    if (ult::resetForegroundCheck.load(std::memory_order_acquire)) {
                        const u64 resetElapsedNs = armTicksToNs(nowTick - resetStartTick);
                        if (resetElapsedNs >= 3'500'000'000ULL) {
                            if (shData->overlayOpen && ult::currentForeground.load(std::memory_order_acquire)) {
                                #if IS_STATUS_MONITOR_DIRECTIVE
                                if (!isValidOverlayMode())
                                    hlp::requestForeground(true, false);
                                #else
                                hlp::requestForeground(true, false);
                                #endif
                            }
                            ult::resetForegroundCheck.store(false, std::memory_order_release);
                        }
                    }
                    
                    if (firstUnderscanCheck || (nowNs - lastUnderscanCheckNs) >= UNDERSCAN_INTERVAL_NS) {
                        currentUnderscanPixels = tsl::gfx::getUnderscanPixels();
                    
                        if (firstUnderscanCheck || currentUnderscanPixels != lastUnderscanPixels) {
                            // Update layer dimensions without destroying state
                            tsl::gfx::Renderer::get().updateLayerSize();
                            
                            lastUnderscanPixels = currentUnderscanPixels;
                            firstUnderscanCheck = false;
                        }
                    
                        lastUnderscanCheckNs = nowNs;
                    }

                    // Process notification files every 300ms
                    {
                        std::lock_guard<std::mutex> jsonLock(notificationJsonMutex);

                        if (armTicksToNs(nowTick - lastNotifCheck) >= 300'000'000ULL) {
                            lastNotifCheck = nowTick;

                            DIR* dir = opendir(ult::NOTIFICATIONS_PATH.c_str());
                            if (dir) {

                                if (ult::useNotifications) {
                                    static u32 seenGeneration = UINT32_MAX;
                                    const u32 curGen = notificationGeneration.load(std::memory_order_acquire);
                                    const bool firstPoll = (seenGeneration != curGen);
                                    if (firstPoll) seenGeneration = curGen;
                                    const std::string& notifPath = ult::NOTIFICATIONS_PATH;

                                    if (!firstPoll && notification && notification->activeCount() >= maxNotifications) {
                                        closedir(dir);
                                    } else {

                                        // ── Single-pass: stat + JSON read + top-N insertion ─────────────
                                        // No candidates vector — filenames never heap-copied unless they
                                        // beat the current worst slot. Safe on small heap threads.
                                        struct NotifData {
                                            std::string fname, text, title;
                                            struct timespec mtime;
                                            int priority, fontSize;
                                            int duration = 0;
                                            bool showTime;
                                            std::string alignment;
                                            bool splitChar  = false;
                                            char timestamp[10] = {}; 
                                        };
                                        static NotifData topSlots[NotificationPrompt::MAX_VISIBLE];
                                        static int topCount = 0;
                                        topCount = 0;

                                        const int activeNow   = notification ? notification->activeCount() : 0;
                                        const int slotsWanted = firstPoll
                                            ? (maxNotifications - activeNow)
                                            : 1;

                                        auto isBetter = [](const NotifData& a, const NotifData& b) noexcept {
                                            if (a.priority != b.priority) return a.priority > b.priority;
                                            if (a.mtime.tv_sec  != b.mtime.tv_sec)  return a.mtime.tv_sec  < b.mtime.tv_sec;
                                            return a.mtime.tv_nsec < b.mtime.tv_nsec;
                                        };

                                        static std::string fullPath;
                                        struct dirent* entry;

                                        while ((entry = readdir(dir)) != nullptr) {
                                            if (entry->d_type != DT_REG) continue;

                                            const char*  fname = entry->d_name;
                                            const size_t len   = strlen(fname);

                                            if (len <= 7 || strcmp(fname + len - 7, ".notify") != 0)
                                                continue;

                                            if (notification && notification->hasActiveFile(fname))
                                                continue;
                                            
                                            // ── Flag check: suppress & delete if {APP_NAME}.flag exists ──────
                                            {
                                                const char* dash = strchr(fname, '-');
                                                if (dash) {
                                                    static std::string flagPath;
                                                    flagPath  = ult::NOTIFICATIONS_FLAGS_PATH;
                                                    flagPath.append(fname, dash - fname);  // extract APP_NAME
                                                    flagPath += ".flag";
                                            
                                                    if (ult::isFile(flagPath)) {
                                                        fullPath  = notifPath;
                                                        fullPath += fname;
                                                        remove(fullPath.c_str());
                                                        continue;
                                                    }
                                                }
                                            }
                                            // ── End flag check ────────────────────────────────────────────────
                                            
                                            fullPath  = notifPath;
                                            fullPath += fname;
                                            
                                            struct stat fileStat;
                                            struct timespec mtime = {0, 0};
                                            char timestampBuf[10] = {};
                                            if (stat(fullPath.c_str(), &fileStat) == 0) {
                                                mtime = fileStat.st_mtim;
                                                ult::formatTimestamp(mtime.tv_sec, timestampBuf, sizeof(timestampBuf));
                                            }

                                            std::unique_ptr<ult::json_t, ult::JsonDeleter> r(
                                                ult::readJsonFromFile(fullPath), ult::JsonDeleter());
                                            if (!r) continue;

                                            cJSON* cr = reinterpret_cast<cJSON*>(r.get());

                                            const cJSON* textObj = cJSON_GetObjectItemCaseSensitive(cr, "text");
                                            if (!cJSON_IsString(textObj) || !textObj->valuestring || !textObj->valuestring[0])
                                                continue;

                                            const cJSON* priorityObj = cJSON_GetObjectItemCaseSensitive(cr, "priority");

                                            NotifData nd;
                                            nd.priority = cJSON_IsNumber(priorityObj) ? (int)priorityObj->valuedouble : 20;
                                            nd.mtime    = mtime;

                                            // Prune before copying strings — skip heap allocs for non-qualifiers.
                                            if (topCount == slotsWanted) {
                                                if (!isBetter(nd, topSlots[topCount - 1])) continue;
                                            }

                                            const cJSON* titleObj    = cJSON_GetObjectItemCaseSensitive(cr, "title");
                                            const cJSON* fontSizeObj = cJSON_GetObjectItemCaseSensitive(cr, "font_size");
                                            const cJSON* durationObj = cJSON_GetObjectItemCaseSensitive(cr, "duration");
                                            const cJSON* showTimeObj = cJSON_GetObjectItemCaseSensitive(cr, "show_time");
                                            const cJSON* alignmentObj = cJSON_GetObjectItemCaseSensitive(cr, "alignment");
                                            const cJSON* splitTypeObj = cJSON_GetObjectItemCaseSensitive(cr, "split_type");

                                            nd.showTime = !(cJSON_IsString(showTimeObj) && showTimeObj->valuestring && strcmp(showTimeObj->valuestring, ult::FALSE_STR.c_str()) == 0);
                                            nd.fname    = fname;
                                            nd.text     = textObj->valuestring;
                                            nd.title    = (cJSON_IsString(titleObj) && titleObj->valuestring) ? titleObj->valuestring : "";
                                            nd.fontSize = cJSON_IsNumber(fontSizeObj) ? std::clamp((int)fontSizeObj->valuedouble, 8, 48)
                                                                                      : (nd.title.empty() ? 26 : 24);
                                            nd.duration = cJSON_IsNumber(durationObj)
                                                ? ((int)durationObj->valuedouble == 0 ? -1 : std::clamp((int)durationObj->valuedouble, 500, 30000))
                                                : 0;

                                            const bool alignmentExplicit = cJSON_IsString(alignmentObj) && alignmentObj->valuestring && alignmentObj->valuestring[0];
                                            nd.alignment = alignmentExplicit ? alignmentObj->valuestring
                                                                             : (nd.title.empty() ? "" : ult::LEFT_STR);
                                            nd.splitChar = cJSON_IsString(splitTypeObj) && splitTypeObj->valuestring
                                                           && strcmp(splitTypeObj->valuestring, ult::CHAR_STR.c_str()) == 0;

                                            int pos = topCount;
                                            while (pos > 0 && isBetter(nd, topSlots[pos - 1])) --pos;

                                            if (pos < slotsWanted) {
                                                if (topCount < slotsWanted) ++topCount;
                                                for (int i = topCount - 1; i > pos; --i)
                                                    topSlots[i] = std::move(topSlots[i - 1]);
                                                topSlots[pos] = std::move(nd);
                                            }
                                        }
                                        closedir(dir);

                                        // ── Dispatch ────────────────────────────────────────────────────
                                        if (notification) {
                                            for (int i = 0; i < topCount; ++i) {
                                                if (notification->activeCount() >= maxNotifications) break;
                                                const NotifData& nd = topSlots[i];
                                                // Resume: stagger expiry so earlier slots fade first.
                                                // Slot 0 of N loses (N-1) seconds, slot 1 loses (N-2), …, last loses 0.
                                                const int baseDuration = nd.duration > 0 ? nd.duration : 4000;
                                                const u32 duration = nd.duration == -1 ? 0u
                                                    : (firstPoll && topCount > 1)
                                                        ? static_cast<u32>(std::max(500, baseDuration - (topCount - 1 - i) * 200))
                                                        : static_cast<u32>(baseDuration);
                                                notification->show(nd.text, nd.fontSize, nd.priority,
                                                                   nd.fname, nd.title, duration,
                                                                   false, firstPoll, nd.showTime,
                                                                   nd.alignment,
                                                                   nd.splitChar ? ult::CHAR_STR : ult::WORD_STR,
                                                                   nd.timestamp);
                                            }
                                        }

                                    } // end dispatch branch

                                } else {
                                    // Notifications disabled: delete all .notify files.
                                    struct dirent* entry;
                                    static std::string fullPath;

                                    while ((entry = readdir(dir)) != nullptr) {
                                        if (entry->d_type != DT_REG) continue;

                                        const char*  fname = entry->d_name;
                                        const size_t len   = strlen(fname);

                                        if (len > 7 && strcmp(fname + len - 7, ".notify") == 0) {
                                            fullPath.clear();
                                            fullPath = ult::NOTIFICATIONS_PATH;
                                            fullPath.append(fname, len);
                                            remove(fullPath.c_str());
                                        }
                                    }
                                    closedir(dir);
                                }
                            }
                        }
                    }
                    
                    // Combine inputs from both controllers
                    const u64 kDown_p1 = padGetButtonsDown(&pad_p1);
                    const u64 kHeld_p1 = padGetButtons(&pad_p1);
                    const u64 kDown_handheld = padGetButtonsDown(&pad_handheld);
                    const u64 kHeld_handheld = padGetButtons(&pad_handheld);
                    
                    // For joysticks, prioritize handheld if available, otherwise use P1
                    const HidAnalogStickState leftStick_handheld = padGetStickPos(&pad_handheld, 0);
                    const HidAnalogStickState rightStick_handheld = padGetStickPos(&pad_handheld, 1);
                    
                    // Check if handheld has any stick input (not at center position)
                    const bool handheldHasInput = (leftStick_handheld.x != 0 || leftStick_handheld.y != 0 || 
                                                  rightStick_handheld.x != 0 || rightStick_handheld.y != 0);

                    // Read touch before acquiring lock (static = BSS, not stack)
                    static HidTouchScreenState newTouchState;
                    newTouchState = { 0 };
                    const bool hasTouchNow = hidGetTouchScreenStates(&newTouchState, 1) > 0;
                    const HidTouchState& currentTouch = newTouchState.touches[0];

                    // Swipe detection (uses only local variables, safe outside lock)
                    if (hasTouchNow) {
                        const u64 elapsedTime_ns = armTicksToNs(nowTick - currentTouchTick);
                        if (ult::useSwipeToOpen && elapsedTime_ns <= TOUCH_THRESHOLD_NS) {
                            if ((lastTouchX != 0 && lastTouchY != 0) && (currentTouch.x != 0 || currentTouch.y != 0)) {
                                if (ult::layerEdge == 0 && currentTouch.x > SWIPE_RIGHT_BOUND + 84 && lastTouchX <= SWIPE_RIGHT_BOUND) {
                                    eventFire(&shData->comboEvent);
                                    mainComboHasTriggered.store(true, std::memory_order_release);
                                }
                                else if (ult::layerEdge > 0 && currentTouch.x < SWIPE_LEFT_BOUND - 84 && lastTouchX >= SWIPE_LEFT_BOUND) {
                                    eventFire(&shData->comboEvent);
                                    mainComboHasTriggered.store(true, std::memory_order_release);
                                }
                            }
                        }

                        if (currentTouch.x == 0 && currentTouch.y == 0) {
                            ult::internalTouchReleased.store(true, std::memory_order_release);
                            lastTouchX = 0;
                            lastTouchY = 0;
                        } else if ((lastTouchX == 0 && lastTouchY == 0) && (currentTouch.x != 0 || currentTouch.y != 0)) {
                            currentTouchTick = nowTick;
                            lastTouchX = currentTouch.x;
                            lastTouchY = currentTouch.y;
                        }

                        if (!shData->overlayOpen)
                            ult::internalTouchReleased.store(false, std::memory_order_release);
                    } else {
                        ult::internalTouchReleased.store(true, std::memory_order_release);
                        lastTouchX = 0;
                        lastTouchY = 0;
                    }

                    // Write all shared input state under the mutex
                    {
                        std::lock_guard<std::mutex> lock(shData->dataMutex);
                        shData->keysDown = kDown_p1 | kDown_handheld;
                        shData->keysHeld = kHeld_p1 | kHeld_handheld;
                        if (handheldHasInput) {
                            shData->joyStickPosLeft  = leftStick_handheld;
                            shData->joyStickPosRight = rightStick_handheld;
                        } else {
                            shData->joyStickPosLeft  = padGetStickPos(&pad_p1, 0);
                            shData->joyStickPosRight = padGetStickPos(&pad_p1, 1);
                        }
                        shData->touchState = hasTouchNow ? newTouchState : HidTouchScreenState{ 0 };
                    }

                    #if IS_STATUS_MONITOR_DIRECTIVE
                    if (triggerExitNow) {
                        ult::launchingOverlay.store(true, std::memory_order_release);
                        ult::setIniFileValue(
                            ult::ULTRAHAND_CONFIG_INI_PATH,
                            ult::ULTRAHAND_PROJECT_NAME,
                            ult::IN_OVERLAY_STR,
                            ult::FALSE_STR
                        );
                        tsl::setNextOverlay(
                            returnOverlayPath
                        );
                        tsl::Overlay::get()->close();
                        break;
                    }
                    #endif
                    
                    // KEY_MINUS: dismiss topmost notification, consume key
                    // Fires whether overlay is open or hidden.
                    // Skipped if MINUS is (part of) a launch combo, or if
                    // other buttons are also held (which means it's mid-combo).
                    if ((shData->keysDown & KEY_MINUS)
                        && !(shData->keysHeld & ~KEY_MINUS & ALL_KEYS_MASK)
                        && tsl::cfg::launchCombo  != KEY_MINUS
                        && tsl::cfg::launchCombo2 != KEY_MINUS
                        && notification && notification->isActive()) {
                        notification->dismissFront();
                        shData->keysDown &= ~KEY_MINUS;
                        shData->keysHeld &= ~KEY_MINUS;
                    }

                    // KEY_MINUS: hold 3s to toggle notifications
                    if (ult::useNotificationsHotkey && tsl::cfg::launchCombo  != KEY_MINUS
                                                    && tsl::cfg::launchCombo2 != KEY_MINUS) {
                        const bool minusAlone = (shData->keysHeld & KEY_MINUS)
                                             && !(shData->keysHeld & ~KEY_MINUS & ALL_KEYS_MASK);
                        if (minusAlone) {
                            if (!minusHoldArmed) {
                                minusHoldArmed     = true;
                                minusHoldStartTick = nowTick;
                            } else if (armTicksToNs(nowTick - minusHoldStartTick) >= 3'000'000'000ULL) {
                                minusHoldArmed = false;
                                ult::useNotifications = !ult::useNotifications;
                                ult::setIniFileValue(
                                    ult::ULTRAHAND_CONFIG_INI_PATH,
                                    ult::ULTRAHAND_PROJECT_NAME,
                                    "notifications",
                                    ult::useNotifications ? ult::TRUE_STR : ult::FALSE_STR
                                );
                                if (ult::useNotifications) {
                                    if (!ult::isFile(ult::NOTIFICATIONS_FLAG_FILEPATH)) {
                                        if (FILE* f = std::fopen(ult::NOTIFICATIONS_FLAG_FILEPATH.c_str(), "w"))
                                            std::fclose(f);
                                    }
                                    if (notification)
                                        notification->show(ult::NOTIFY_HEADER+"API notifications enabled.", 22, 0);
                                } else {
                                    ult::deleteFileOrDirectory(ult::NOTIFICATIONS_FLAG_FILEPATH);
                                    if (notification)
                                        notification->show(ult::NOTIFY_HEADER+"API notifications disabled.", 22, 0);
                                }
                                shData->keysDown &= ~KEY_MINUS;
                                shData->keysHeld &= ~KEY_MINUS;
                            }
                        } else {
                            minusHoldArmed = false;
                        }
                    }

                    // Check main launch combo first (highest priority)
                    if ((((shData->keysHeld & tsl::cfg::launchCombo) == tsl::cfg::launchCombo) && shData->keysDown & tsl::cfg::launchCombo)) {
                    #if IS_LAUNCHER_DIRECTIVE
                        if (ult::updateMenuCombos) {
                            ult::setIniFileValue(ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME, ult::KEY_COMBO_STR , ult::ULTRAHAND_COMBO_STR);
                            ult::setIniFileValue(ult::TESLA_CONFIG_INI_PATH, ult::TESLA_STR, ult::KEY_COMBO_STR , ult::ULTRAHAND_COMBO_STR);
                            ult::updateMenuCombos = false;
                        }
                    #endif
                        
                        #if IS_STATUS_MONITOR_DIRECTIVE
                        isRendering = false;
                        leventSignal(&renderingStopEvent);  // always — wakes frame limiter immediately
                        #endif
                        
                        if (shData->overlayOpen) {
                            if (!disableHiding) {
                        #if IS_STATUS_MONITOR_DIRECTIVE
                                if (!isValidOverlayMode()) {  // only guard the hide
                        #endif
                                    tsl::Overlay::get()->hide();
                                    shData->overlayOpen = false;
                        #if IS_STATUS_MONITOR_DIRECTIVE
                                }
                        #endif
                            }
                        }
                        else {
                            eventFire(&shData->comboEvent);
                            mainComboHasTriggered.store(true, std::memory_order_release);
                        }
                    }
                #if IS_LAUNCHER_DIRECTIVE
                    else if (ult::updateMenuCombos && (((shData->keysHeld & tsl::cfg::launchCombo2) == tsl::cfg::launchCombo2) && shData->keysDown & tsl::cfg::launchCombo2)) {
                        std::swap(tsl::cfg::launchCombo, tsl::cfg::launchCombo2); // Swap the two launch combos
                        ult::setIniFileValue(ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME, ult::KEY_COMBO_STR , ult::TESLA_COMBO_STR);
                        ult::setIniFileValue(ult::TESLA_CONFIG_INI_PATH, ult::TESLA_STR, ult::KEY_COMBO_STR , ult::TESLA_COMBO_STR);
                        eventFire(&shData->comboEvent);
                        mainComboHasTriggered.store(true, std::memory_order_release);
                        ult::updateMenuCombos = false;
                    }
                    else if (ult::overlayLaunchRequested.load(std::memory_order_acquire) && !ult::runningInterpreter.load(std::memory_order_acquire) && ult::settingsInitialized.load(std::memory_order_acquire) && (nowNs - startNs) >= FAST_SWAP_THRESHOLD_NS) {
                        std::string requestedPath, requestedArgs;
                        
                        // Get the request data safely
                        {
                            std::lock_guard<std::mutex> lock(ult::overlayLaunchMutex);
                            requestedPath = ult::requestedOverlayPath;
                            requestedArgs = ult::requestedOverlayArgs;
                            ult::overlayLaunchRequested.store(false, std::memory_order_release);
                        }
                        
                        if (!requestedPath.empty()) {

                            const std::string overlayFileName = ult::getNameFromPath(requestedPath);
                            
                            // Set overlay state for ovlmenu.ovl
        
                            // OPTIMIZED: Batch INI file writes
                            {
                                ult::setIniFileValue(ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME, ult::IN_OVERLAY_STR, ult::TRUE_STR);
                                ult::setIniFileValue(ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME, "to_packages", ult::TRUE_STR);
                            }
        
                            // Reset navigation state variables (these control slide navigation)
                            ult::allowSlide.store(false, std::memory_order_release);
                            ult::unlockedSlide.store(false, std::memory_order_release);

                            // Launch the overlay using the same mechanism as key combos
                            tsl::setNextOverlay(requestedPath, requestedArgs+" --direct");
                            fireLaunch();
                            return;
                        }
                    }
                #endif
                    // Check overlay key combos (only when overlay is not open, keys are pressed, and not conflicting with main combos)
                    else if (shData->keysDown != 0 && ult::useLaunchCombos) {
                        if (shData->keysHeld != tsl::cfg::launchCombo) {
                            // Lookup both path and optional mode launch args
                            const auto comboInfo = tsl::hlp::getEntryForKeyCombo(shData->keysHeld);
                            const std::string& overlayPath = comboInfo.path;
                            
                    
                    #if IS_LAUNCHER_DIRECTIVE
                            if (!overlayPath.empty() && (shData->keysHeld) && !ult::runningInterpreter.load(std::memory_order_acquire) && ult::settingsInitialized.load(std::memory_order_acquire) && (armTicksToNs(nowTick) - startNs) >= FAST_SWAP_THRESHOLD_NS) {
                    #else
                            if (!overlayPath.empty() && (shData->keysHeld) && (nowNs - startNs) >= FAST_SWAP_THRESHOLD_NS) {
                    #endif

                                const std::string& modeArg = comboInfo.launchArg;
                                const std::string overlayFileName = ult::getNameFromPath(overlayPath);
                    
                                // Check HOS21 support before doing anything
                                if (requiresLNY2 && !usingLNY2(overlayPath)) {
                                    // Skip launch if not supported
                                    const auto forceSupportStatus = ult::parseValueFromIniSection(
                                        ult::OVERLAYS_INI_FILEPATH, overlayFileName, "force_support");
                                    if (forceSupportStatus != ult::TRUE_STR) {
                                        if (tsl::notification) {
                                            tsl::notification->showNow(ult::NOTIFY_HEADER+ult::INCOMPATIBLE_WARNING, 22);
                                        }
                                        continue;
                                    }
                                }

                                // hideHidden check
                                if (hideHidden) {
                                    const auto hideStatus = ult::parseValueFromIniSection(
                                        ult::OVERLAYS_INI_FILEPATH, overlayFileName, ult::HIDE_STR);
                                    if (hideStatus == ult::TRUE_STR) {
                                        shData->keysDownPending |= shData->keysDown;
                                        continue;
                                    }
                                }
        
                                #if IS_STATUS_MONITOR_DIRECTIVE
                                isRendering = false;
                                leventSignal(&renderingStopEvent);
                                #endif
                    
                    #if !IS_LAUNCHER_DIRECTIVE
                                if (lastOverlayFilename == overlayFileName && lastOverlayMode == modeArg) {
                    #else
                                if (lastOverlayFilename == overlayFileName  && lastOverlayMode == modeArg && lastOverlayMode.find("--package") != std::string::npos) {
                    #endif
                                    ult::setIniFileValue(
                                        ult::ULTRAHAND_CONFIG_INI_PATH,
                                        ult::ULTRAHAND_PROJECT_NAME,
                                        ult::IN_OVERLAY_STR,
                                        ult::TRUE_STR
                                    );

                                    tsl::setNextOverlay(returnOverlayPath, "--direct --comboReturn");
                                    fireLaunch();
                                    return;
                                }
                                
                                // Compose launch args
                                std::string finalArgs;
                                if (!modeArg.empty()) {
                                    finalArgs = modeArg;
                                } else {
                                    // Only check overlay-specific launch args for non-ovlmenu entries
                                    if (overlayFileName.compare("ovlmenu.ovl") != 0) {
                                        // OPTIMIZED: Single INI read for both values
                                        const std::string useArgs = ult::parseValueFromIniSection(ult::OVERLAYS_INI_FILEPATH, overlayFileName, ult::USE_LAUNCH_ARGS_STR);
                                        const std::string launchArgs = ult::parseValueFromIniSection(ult::OVERLAYS_INI_FILEPATH, overlayFileName, ult::LAUNCH_ARGS_STR);
                                        
                                        if (useArgs == ult::TRUE_STR) {
                                            finalArgs = launchArgs;
                                            ult::removeQuotes(finalArgs);
                                        }
                                    }
                                }
                                if (finalArgs.empty()) {
                                    finalArgs = "--direct";
                                } else {
                                    finalArgs += " --direct";
                                }
        
                                if (overlayFileName.compare("ovlmenu.ovl") == 0) {
                                    finalArgs += " --comboReturn";
                                    ult::setIniFileValue(ult::ULTRAHAND_CONFIG_INI_PATH, ult::ULTRAHAND_PROJECT_NAME, ult::IN_OVERLAY_STR, ult::TRUE_STR);
                                }

                                tsl::setNextOverlay(overlayPath, finalArgs);
                                fireLaunch();
                                return;
                            }
                        }
                    }
                //#endif
                    
                    shData->keysDownPending |= shData->keysDown;
                }

                //20 ms
                //s32 idx = 0;
                rc = waitObjects(&idx, objects, WaiterObject_Count, 20'000'000ul);
                if (R_SUCCEEDED(rc)) {
        
        #if IS_STATUS_MONITOR_DIRECTIVE
                    if (idx == WaiterObject_HomeButton || idx == WaiterObject_PowerButton) { // Changed condition to exclude capture button
                        if (shData->overlayOpen && !isValidOverlayMode()) {
                            tsl::Overlay::get()->hide();
                            shData->overlayOpen = false;
                        }
                    }
        #else
                    if (idx == WaiterObject_HomeButton || idx == WaiterObject_PowerButton) { // Changed condition to exclude capture button
                        if (shData->overlayOpen && !disableHiding) {
                            tsl::Overlay::get()->hide();
                            shData->overlayOpen = false;
                        } else if (idx == WaiterObject_HomeButton && disableHiding) {
                            // Game session — normal hide is suppressed.  Signal the player
                            // GUI to release foreground (if held) so HID returns to the
                            // background title.  Consumed by process_home_foreground_release().
                            homeButtonPressedInGame.store(true, std::memory_order_release);
                        }
                    }
        #endif
                    
                    switch (idx) {
                        case WaiterObject_HomeButton:
                            eventClear(&homeButtonPressEvent);
                            break;
                        case WaiterObject_PowerButton:
                            eventClear(&powerButtonPressEvent);

                            // Block feedback thread from touching HID during reinit
                            hidReinitInProgress.store(true, std::memory_order_seq_cst);
                            svcSleepThread(20'000'000ULL); // 20ms — let feedback thread finish its current iteration
        
                            // Perform any necessary cleanup
                            hidExit();
        
                            // Reinitialize resources
                            ASSERT_FATAL(hidInitialize()); // Reinitialize HID to reset states
                            
                            // Reinitialize both controllers
                            padInitialize(&pad_p1, HidNpadIdType_No1);
                            padInitialize(&pad_handheld, HidNpadIdType_Handheld);
                            hidInitializeTouchScreen();
                            
                            // Update both controllers
                            padUpdate(&pad_p1);
                            padUpdate(&pad_handheld);

                            // Clear shared input state so wake doesn't see phantom held keys
                            {
                                std::lock_guard<std::mutex> lock(shData->dataMutex);
                                shData->keysDown        = 0;
                                shData->keysHeld        = 0;
                                shData->keysDownPending = 0;
                                shData->touchState      = { 0 };
                            }

                            triggerInitHaptics.store(true, std::memory_order_release);
                            hidReinitInProgress.store(false, std::memory_order_seq_cst);
                            signalFeedback();   // wake poller to run initHaptics
                            break;
                            
                            
                        case WaiterObject_CaptureButton:
                            if (screenshotsAreDisabled) {
                                eventClear(&captureButtonPressEvent);
                                break;
                            }

                            #if IS_STATUS_MONITOR_DIRECTIVE
                            const bool inOverlayMode = isValidOverlayMode();
                            if (inOverlayMode) {
                                delayUpdate = true;
                                isRendering = false;
                                leventSignal(&renderingStopEvent);
                            }
                            #endif
        
                            ult::disableTransparency = true;
                            eventClear(&captureButtonPressEvent);
                            svcSleepThread(1'500'000'000);
                            ult::disableTransparency = false;
        
                            #if IS_STATUS_MONITOR_DIRECTIVE
                            if (inOverlayMode) {
                                if (notification && notification->isActive()) {
                                    // Notification is still animating — don't re-enable the frame limiter yet.
                                    // Restore wasRendering so the notification draw loop handles re-enabling when done.
                                    wasRendering = true;
                                } else {
                                    isRendering = true;
                                    leventClear(&renderingStopEvent);
                                }
                                delayUpdate = false;
                            }
                            #endif
        
                            break;
                    }
                } else if (rc != KERNELRESULT(TimedOut)) {
                    ASSERT_FATAL(rc);
                }
            }
            //hidExit();

        }

        /**
         * @brief Background event polling loop thread
         *
         * @param args Used to pass in a pointer to a \ref SharedThreadData struct
         */
        // ── Haptics thread ────────────────────────────────────────────────────────
        // Dedicated thread for haptic feedback. Calls the blocking standalone
        // rumble functions directly — no timer state machine needed.
        static void backgroundHapticsPoller(void* /*args*/) {
            // Initialize haptics once on thread start — equivalent to the call that
            // lived in backgroundEventPoller before haptics moved to their own thread.
            // Ensures cachedHandheldStyle/cachedPlayer1Style are valid before the
            // first rumble trigger arrives. checkAndReinitHaptics() in the loop
            // handles controller reconnects after this.
            ult::initHaptics();

            while (true) {

                leventWait(&hapticsEvent, UINT64_MAX);
                leventClear(&hapticsEvent);

                // Capture both exit conditions before processing triggers so that
                // exit/launch feedback (double click on return, click on reload)
                // always completes its full sequence before the thread tears down.
                const bool stopping  = feedbackPollerStop.load(std::memory_order_acquire);
                const bool launching = ult::launchingOverlay.load(std::memory_order_acquire);

                if (!ult::useHapticFeedback || disableHaptics.load(std::memory_order_acquire)
                    || hidReinitInProgress.load(std::memory_order_acquire)) {
                    triggerInitHaptics.store(false, std::memory_order_release);
                    triggerRumbleClick.store(false, std::memory_order_release);
                    triggerRumbleDoubleClick.store(false, std::memory_order_release);
                } else {
                    if (triggerInitHaptics.exchange(false, std::memory_order_acq_rel)) {
                        ult::initHaptics();
                    } else if (!stopping && !launching) {
                        ult::checkAndReinitHaptics();
                    }

                    if (triggerRumbleDoubleClick.exchange(false, std::memory_order_acq_rel)) {
                        triggerRumbleClick.store(false, std::memory_order_release);
                        ult::rumbleDoubleClickStandalone();
                    } else if (triggerRumbleClick.exchange(false, std::memory_order_acq_rel)) {
                        ult::rumbleClickStandalone();
                    }
                }

                // Break only after triggers are drained — never mid-sequence.
                if (stopping || launching) break;
            }
            ult::deinitHaptics();
        }

        // ── Sound thread ──────────────────────────────────────────────────────────
        // Dedicated thread for audio playback. Sleeps until signalled; audio calls
        // may block freely without affecting haptic timing.
        static void backgroundSoundPoller(void* /*args*/) {
            while (!feedbackPollerStop.load(std::memory_order_acquire)) {

                leventWait(&soundEvent, UINT64_MAX);
                leventClear(&soundEvent);

                if (feedbackPollerStop.load(std::memory_order_acquire)) break;
                if (ult::launchingOverlay.load(std::memory_order_acquire))  break;

                if (ult::limitedMemory) continue;

                if (!ult::useSoundEffects || disableSound.load(std::memory_order_acquire)) {
                    triggerNavigationSound.store(false, std::memory_order_release);
                    triggerEnterSound.store(false, std::memory_order_release);
                    triggerExitSound.store(false, std::memory_order_release);
                    triggerWallSound.store(false, std::memory_order_release);
                    triggerOnSound.store(false, std::memory_order_release);
                    triggerOffSound.store(false, std::memory_order_release);
                    triggerSettingsSound.store(false, std::memory_order_release);
                    triggerMoveSound.store(false, std::memory_order_release);
                    triggerNotificationSound.store(false, std::memory_order_release);
                    continue;
                }

                if (reloadIfDockedChangedNow.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::reloadIfDockedChanged();
                if (reloadSoundCacheNow.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::reloadAllSounds();

                if (triggerNavigationSound.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::playNavigateSound();
                else if (triggerEnterSound.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::playEnterSound();
                else if (triggerExitSound.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::playExitSound();
                else if (triggerWallSound.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::playWallSound();
                else if (triggerOnSound.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::playOnSound();
                else if (triggerOffSound.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::playOffSound();
                else if (triggerSettingsSound.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::playSettingsSound();
                else if (triggerMoveSound.exchange(false, std::memory_order_acq_rel))
                    ult::Audio::playMoveSound();
                else if (triggerNotificationSound.exchange(false, std::memory_order_acq_rel) && !ult::silenceNotifications)
                    ult::Audio::playNotificationSound();
            }
        }
    }
    
    /**
     * @brief Creates a new Gui and changes to it
     *
     * @tparam G Gui to create
     * @tparam Args Arguments to pass to the Gui
     * @param args Arguments to pass to the Gui
     * @return Reference to the newly created Gui
     */
    template<typename G, typename ...Args>
    std::unique_ptr<tsl::Gui>& changeTo(Args&&... args) {
        return Overlay::get()->changeTo<G, Args...>(std::forward<Args>(args)...);
    }

    template<typename G, typename ...Args>
    std::unique_ptr<tsl::Gui>& swapTo(Args&&... args) {
        return Overlay::get()->swapTo<G, Args...>(std::forward<Args>(args)...);
    }

    template<typename G, typename ...Args>
    std::unique_ptr<tsl::Gui>& swapTo(SwapDepth depth, Args&&... args) {
        return Overlay::get()->swapTo<G, Args...>(depth, std::forward<Args>(args)...);
    }
    

    /**
     * @brief Pops the top Gui from the stack and goes back to the last one
     * @note The Overlay gets closed once there are no more Guis on the stack
     */
    inline void goBack(u32 count) {
        Overlay::get()->goBack(count);
    }
    
    inline void pop(u32 count) {
        Overlay::get()->pop(count);
    }
        
    /**
     * @brief Shifts focus to a specific UI element
     * 
     * Requests focus on the provided element without directional navigation.
     * Uses FocusDirection::None to set focus directly on the target element,
     * typically centering it in the viewport without triggering navigation effects.
     * 
     * Useful for jumping to specific items programmatically (e.g., after search,
     * restoring saved position, or responding to external events).
     * 
     * @param element The element to receive focus
     */
    inline void shiftItemFocus(elm::Element* element) {
        if (auto& currentGui = Overlay::get()->getCurrentGui()) {
            currentGui->requestFocus(element, FocusDirection::None);
        }
    }
    
    inline std::mutex setNextOverlayMutex;
    inline std::string nextOverlayName;

    
    __attribute__((noinline)) inline void setNextOverlay(const std::string& ovlPath, std::string origArgs) {
        std::lock_guard lk(setNextOverlayMutex);
        char buffer[1024];
        char* p       = buffer;
        char* bufferEnd = buffer + sizeof(buffer) - 1;
    
        const std::string filenameStr = ult::getNameFromPath(ovlPath);
        nextOverlayName = filenameStr;
    
        const char* filename = filenameStr.c_str();
        while (*filename && p < bufferEnd) *p++ = *filename++;
        if (p < bufferEnd) *p++ = ' ';
    
        const char* src = origArgs.c_str();
        const char* end = src + origArgs.length();
        bool hasSkipCombo = false;
    
        while (src < end && p < bufferEnd) {
            while (src < end && *src == ' ' && p < bufferEnd) *p++ = *src++;
            if (src >= end || p >= bufferEnd) break;
    
            if (src[0] == '-' && src[1] == '-') {
                if (strncmp(src, "--skipCombo", 11) == 0 &&
                    (src[11] == ' ' || src[11] == '\0')) {
                    hasSkipCombo = true;
                    while (src < end && *src != ' ' && p < bufferEnd) *p++ = *src++;
                }
                else if (strncmp(src, "--foregroundFix", 15) == 0) {
                    src += 15;
                    while (src < end && *src == ' ') src++;
                    if (src < end && (*src == '0' || *src == '1')) src++;
                }
                else if (strncmp(src, "--lastTitleID", 13) == 0) {
                    src += 13;
                    while (src < end && *src == ' ') src++;
                    while (src < end && *src != ' ' && *src != '\0') src++;
                }
                else {
                    while (src < end && *src != ' ' && p < bufferEnd) *p++ = *src++;
                }
            }
            else {
                while (src < end && *src != ' ' && p < bufferEnd) *p++ = *src++;
            }
        }
    
        if (!hasSkipCombo && (p + 12) < bufferEnd) {
            memcpy(p, " --skipCombo", 12); p += 12;
        }
    
        if ((p + 17) < bufferEnd) {
            memcpy(p, " --foregroundFix ", 17); p += 17;
            if (p < bufferEnd) {
                *p++ = (ult::resetForegroundCheck.load(std::memory_order_acquire) ||
                        ult::lastTitleID != ult::getTitleIdAsString()) ? '1' : '0';
            }
        }
    
        if ((p + 15 + (ptrdiff_t)ult::lastTitleID.length()) < bufferEnd) {
            memcpy(p, " --lastTitleID ", 15); p += 15;
            const char* titleId = ult::lastTitleID.c_str();
            while (*titleId && p < bufferEnd) *p++ = *titleId++;
        }
    
        if (p >= bufferEnd) p = bufferEnd;
        *p = '\0';
    
        envSetNextLoad(ovlPath.c_str(), buffer);
    }
    
    

    struct option_entry {
        const char* name;
        u8 len;
        u8 action;
    };
    
    static constexpr struct option_entry options[] = {
        {"direct", 6, 1},
        {"skipCombo", 9, 2},
        {"lastTitleID", 11, 3}, 
        {"foregroundFix", 13, 4},
        {"package", 7, 5},
        {"lastSelectedItem", 16, 6},
        {"comboReturn", 11, 7} // new option
    };


    /**
     * @brief libtesla's main function
     * @note Call it directly from main passing in argc and argv and returning it e.g `return tsl::loop<OverlayTest>(argc, argv);`
     *
     * @tparam TOverlay Your overlay class
     * @tparam launchFlags \ref LaunchFlags
     * @param argc argc
     * @param argv argv
     * @return int result
     */
    template<typename TOverlay, impl::LaunchFlags launchFlags>
    static inline int loop(int argc, char** argv) {
        static_assert(std::is_base_of_v<tsl::Overlay, TOverlay>, "tsl::loop expects a type derived from tsl::Overlay");

    #if IS_STATUS_MONITOR_DIRECTIVE
        leventClear(&renderingStopEvent);
        // Status monitor will load heap settings directly in main, so bypass here in loop
    #else

        ult::currentHeapSize = ult::getCurrentHeapSize();
        ult::expandedMemory = ult::currentHeapSize >= ult::OverlayHeapSize::Size_8MB;
        ult::limitedMemory = ult::currentHeapSize == ult::OverlayHeapSize::Size_4MB;


        // Initialize buffer sizes based on expanded memory setting
        if (ult::expandedMemory) {
            ult::furtherExpandedMemory = ult::currentHeapSize > ult::OverlayHeapSize::Size_8MB;
            
            if (!ult::furtherExpandedMemory) {
                ult::loaderTitle += "+";
                ult::COPY_BUFFER_SIZE = 262144;
                ult::HEX_BUFFER_SIZE = 8192;
                ult::UNZIP_READ_BUFFER = 262144;
                ult::UNZIP_WRITE_BUFFER = 131072;
                ult::DOWNLOAD_READ_BUFFER = 131072;
                ult::DOWNLOAD_WRITE_BUFFER = 131072;
            } else {
                ult::loaderTitle += "×";
                ult::COPY_BUFFER_SIZE = 262144*2;
                ult::HEX_BUFFER_SIZE = 8192;
                ult::UNZIP_READ_BUFFER = 262144*2;
                ult::UNZIP_WRITE_BUFFER = 131072*4;
                ult::DOWNLOAD_READ_BUFFER = 131072*4;
                ult::DOWNLOAD_WRITE_BUFFER = 131072*4;
            }
        } else if (ult::limitedMemory) {
            ult::loaderTitle += "-";
            ult::DOWNLOAD_READ_BUFFER = 16*1024;
            ult::UNZIP_READ_BUFFER = 16*1024;
        }
    #endif
    
        if (argc > 0) {
            lastOverlayFilename = ult::getNameFromPath(argv[0]);
    
            lastOverlayMode.clear();
            bool skip;
            for (u8 arg = 1; arg < argc; arg++) {
                const char* s = argv[arg];

                skip = false;
    
                if (arg > 1) {
                    const char* prev = argv[arg - 1];
                    if (prev[0] == '-' && prev[1] == '-') {
                        if (strcmp(prev, "--lastTitleID") == 0 || strcmp(prev, "--foregroundFix") == 0) {
                            skip = true;
                        }
                    }
                }
    
                if (!skip && s[0] == '-' && s[1] == '-') {
                    if (strcmp(s, "--direct") == 0 ||
                        strcmp(s, "--skipCombo") == 0 ||
                        strcmp(s, "--lastTitleID") == 0 ||
                        strcmp(s, "--foregroundFix") == 0) {
                        skip = true;
                    }
                }
    
                if (!skip) {
                    if (strcmp(s, "--package") == 0) {
                        lastOverlayMode = "--package";
                        arg++;
                        if (arg < argc) {
                            lastOverlayMode += " ";
                            lastOverlayMode += argv[arg];
                            arg++;
                            while (arg < argc && argv[arg][0] != '-') {
                                lastOverlayMode += " ";
                                lastOverlayMode += argv[arg];
                                arg++;
                            }
                        }
                    } else {
                        lastOverlayMode = s;
                    }
                    break;
                }
            }
        }
    
        // Detect -returning: overlay was launched as a return from a sub-mode
        // (e.g. windowed → normal).  Uses exit feedback instead of enter feedback.
        // Only needed in the non-STATUS_MONITOR, non-LAUNCHER path below.
        #if !IS_STATUS_MONITOR_DIRECTIVE && !IS_LAUNCHER_DIRECTIVE
        bool isReturningLaunch = false;
        for (u8 arg = 1; arg < argc; arg++) {
            if (strcmp(argv[arg], "-returning") == 0) {
                isReturningLaunch = true;
                break;
            }
        }
        #endif

        bool skipCombo = false;
    #if IS_LAUNCHER_DIRECTIVE
        bool comboReturn = false;
        bool directMode = true;
    #else
        bool directMode = false;
    #endif
        bool usingPackageLauncher = false;
        
    
        for (u8 arg = 0; arg < argc; arg++) {
            const char* s = argv[arg];
            if (s[0] != '-' || s[1] != '-') continue;
            const char* opt = s + 2;
    
            for (u8 i = 0; i < 7; i++) { // now 6 instead of 5
                if (memcmp(opt, options[i].name, options[i].len) == 0 && opt[options[i].len] == '\0') {
                    switch (options[i].action) {
                        case 1: // direct
                            directMode = true;
                            jumpItemName = "";
                            jumpItemValue = "";
                            jumpItemExactMatch.store(true, std::memory_order_release);
                            break;
                        case 2: // skipCombo
                            skipCombo = true;
                            ult::firstBoot = false;
                            break;
                        case 3: // lastTitleID
                            if (++arg < argc) {
                                const char* providedID = argv[arg];
                                if (ult::getTitleIdAsString() != providedID) {
                                    ult::resetForegroundCheck.store(true, std::memory_order_release);
                                }
                            }
                            break;
                        case 4: // foregroundFix
                            if (++arg < argc) {
                                ult::resetForegroundCheck.store(
                                    ult::resetForegroundCheck.load(std::memory_order_acquire) ||
                                    (argv[arg][0] == '1'), std::memory_order_release);
                            }
                            break;
                        case 5: // package
                            usingPackageLauncher = true;
                            break;
                        case 6: // lastSelectedItem
                            #if IS_STATUS_MONITOR_DIRECTIVE
                            lastMode = "returning";
                            #endif
                            break;
                        case 7: // comboReturn
                            #if IS_LAUNCHER_DIRECTIVE
                            comboReturn = true;
                            #endif
                            break;
                    }
                }
            }
        }
    
        impl::SharedThreadData shData;
        shData.running.store(true, std::memory_order_release);
    
        auto& overlay = tsl::Overlay::s_overlayInstance;
        overlay = new TOverlay();
        overlay->m_closeOnExit = (u8(launchFlags) & u8(impl::LaunchFlags::CloseOnExit)) == u8(impl::LaunchFlags::CloseOnExit);

        // Parse Tesla settings
        impl::parseOverlaySettings();

        // Initialize overlay services & screen
        tsl::hlp::doWithSmSession([&overlay]{
            overlay->initServices();
        });
        overlay->initScreen();

        eventCreate(&shData.comboEvent, false);

        Thread backgroundHapticsThread;
        threadCreate(&backgroundHapticsThread, impl::backgroundHapticsPoller, nullptr, nullptr, 0x1000, 0x2c, -2);
        threadStart(&backgroundHapticsThread);

        Thread backgroundSoundThread;
        if (!ult::limitedMemory) {
            threadCreate(&backgroundSoundThread, impl::backgroundSoundPoller, nullptr, nullptr, 0x1000, 0x2c, -2);
            threadStart(&backgroundSoundThread);
        }
        
        Thread backgroundEventThread;
        threadCreate(&backgroundEventThread, impl::backgroundEventPoller, &shData, nullptr, 0x2000, 0x2c, -2);
        threadStart(&backgroundEventThread);
        

        bool shouldFireEvent = false;

    #if IS_LAUNCHER_DIRECTIVE
       
        {
            auto configData = ult::getParsedDataFromIniFile(ult::ULTRAHAND_CONFIG_INI_PATH);
            bool needsUpdate = false;
        
            // Get reference to project section (create if missing)
            auto& project = configData[ult::ULTRAHAND_PROJECT_NAME];
        
            // Determine current overlay state
            bool inOverlay = true;
            auto it = project.find(ult::IN_OVERLAY_STR);
            if (it != project.end()) {
                inOverlay = (it->second != ult::FALSE_STR);
            }
        
            // Only update the overlay key once, for either firstBoot or skipCombo
            if (ult::firstBoot || (inOverlay && skipCombo)) {
                project[ult::IN_OVERLAY_STR] = ult::FALSE_STR;
                needsUpdate = true;
                if (inOverlay && skipCombo) {
                    shouldFireEvent = true;
                }
            }
        
            // Write INI only if we changed something
            if (needsUpdate) {
                ult::saveIniFileData(ult::ULTRAHAND_CONFIG_INI_PATH, configData);
            }
        
            // Fire event if needed
            if (shouldFireEvent) {
                eventFire(&shData.comboEvent);
            } else {
                lastOverlayFilename = "";
            }
        }
    #else
        {
            auto configData = ult::getParsedDataFromIniFile(ult::ULTRAHAND_CONFIG_INI_PATH);
        
            auto projectIt = configData.find(ult::ULTRAHAND_PROJECT_NAME);
            if (projectIt != configData.end()) {
                auto& project = projectIt->second;
        
                auto overlayIt = project.find(ult::IN_OVERLAY_STR);
                const bool inOverlay = (overlayIt == project.end() ||
                                        overlayIt->second != ult::FALSE_STR);
        
                if (inOverlay && directMode) {
                    project[ult::IN_OVERLAY_STR] = ult::FALSE_STR;
                    ult::saveIniFileData(ult::ULTRAHAND_CONFIG_INI_PATH, configData);
                }
            }
        }

        if (skipCombo) {
            eventFire(&shData.comboEvent);
            shouldFireEvent = true;
        }
    #endif

        overlay->changeTo(overlay->loadInitialGui());
        overlay->disableNextAnimation();
        
        {
            const Handle handles[2] = { shData.comboEvent.revent, notificationEvent.revent };
            s32 index = -1;
            
            bool exitAfterPrompt = false;
            bool comboBreakout = false;
            bool firstLoop = !ult::firstBoot;
            
            auto exitLaunching = [&]() __attribute__((noinline)) {
                shData.running.store(false, std::memory_order_release);
                shData.overlayOpen.store(false, std::memory_order_release);
            };

            while (shData.running.load(std::memory_order_acquire)) {
                // Early exit if launching new overlay
                if (ult::launchingOverlay.load(std::memory_order_acquire)) {
                    exitLaunching(); break;
                }
                
                // Wait for events only if no active notification
                if (!(notification && notification->isActive())) {
                    svcWaitSynchronization(&index, handles, 2, UINT64_MAX);
                }
                eventClear(&notificationEvent);
                eventClear(&shData.comboEvent);
                
                if ((notification && notification->isActive() && !firstLoop) || index == 1) {
                    comboBreakout = false;
                    
                    while (shData.running.load(std::memory_order_acquire)) {
                        {
                            if (ult::launchingOverlay.load(std::memory_order_acquire)) {
                                exitLaunching(); break;
                            }
                            overlay->loop(true); // Draw prompts while hidden

                            // ── Notification touch-dismiss while overlay is hidden ──────────────
                            {
                                static bool hiddenTouchWasDown = false;
                                bool touchNow = false;
                                HidTouchState tp = {};
                            
                                {
                                    std::scoped_lock lock(shData.dataMutex);
                                    touchNow = shData.touchState.count > 0;
                                    if (touchNow)
                                        tp = shData.touchState.touches[0];
                                }
                                // dataMutex released — safe to call notification methods
                                // which internally acquire state_mutex_
                                if (!hiddenTouchWasDown && touchNow) {
                                    if (notification && notification->hitTest(
                                            static_cast<s32>(tp.x), static_cast<s32>(tp.y))) {
                                        notification->dismissAt(
                                            static_cast<s32>(tp.x), static_cast<s32>(tp.y));
                                    }
                                }
                                hiddenTouchWasDown = touchNow;
                            }
                            // ───────────────────────────────────────────────────────────────────
                        }
                        
                        if (mainComboHasTriggered.exchange(false, std::memory_order_acq_rel)) {
                            comboBreakout = true;
                            exitAfterPrompt = false;
                            break;
                        }
                        
                        if (launchComboHasTriggered.load(std::memory_order_acquire)) {
                            exitAfterPrompt = true;
                            usingPackageLauncher = false;
                            directMode = false;
                            break;
                        }
                        
                        if (!(notification && notification->isActive())) {
                            break;
                        }
                    }
                    
                    if (!comboBreakout || !shData.running.load(std::memory_order_acquire)) {
                        {
                            if (!ult::launchingOverlay.load(std::memory_order_acquire)) {
                                overlay->clearScreen();
                            }
                        }
                        if (exitAfterPrompt) {
                            std::scoped_lock lock(shData.dataMutex);
                            exitAfterPrompt = false;
                            exitLaunching();
                            ult::launchingOverlay.store(true, std::memory_order_release);
                            launchComboHasTriggered.store(true, std::memory_order_release); // for isolating sound effect
    
                            if (usingPackageLauncher || directMode) {
                                tsl::setNextOverlay(returnOverlayPath);
                            }
                            
                            hlp::requestForeground(false);
                            break;
                        }
                        continue;
                    }
                }
    
                {
                    if (ult::launchingOverlay.load(std::memory_order_acquire)) {
                        exitLaunching(); break;
                    }
                    firstLoop = false;
                    shData.overlayOpen.store(true, std::memory_order_release);
    
    #if IS_STATUS_MONITOR_DIRECTIVE
                    if (!isValidOverlayMode())
                        hlp::requestForeground(true);
    #else
                    hlp::requestForeground(true);
    #endif
    
                    // Suppress the click in show() when exit feedback (double click)
                    // is about to fire on the first loop iteration — let the double
                    // click stand alone rather than stacking a click on top of it.
                    #if IS_LAUNCHER_DIRECTIVE
                    skipRumbleClick = shouldFireEvent && !comboReturn;
                    #elif IS_STATUS_MONITOR_DIRECTIVE
                    skipRumbleClick = (lastMode.compare("returning") == 0);
                    #else
                    skipRumbleClick = isReturningLaunch;
                    #endif
                    overlay->show();
                    if (!comboBreakout && !(notification && notification->isActive()))
                        overlay->clearScreen();

                    {
                        std::scoped_lock lock(shData.dataMutex);
                        // Clear derived states that the overlay loop will use
                        shData.keysDown = 0;
                        shData.keysHeld = 0;
                        // Clear any queued pending keys so nothing gets processed one frame late
                        shData.keysDownPending = 0;
                    }
                }
                
                while (shData.running.load(std::memory_order_acquire)) {
                    {
                        
                        if (ult::launchingOverlay.load(std::memory_order_acquire)) {
                            shData.running.store(false, std::memory_order_release);
                            shData.overlayOpen.store(false, std::memory_order_release);
                            break;
                        }
                        overlay->loop();
                        {
                            std::scoped_lock lock(shData.dataMutex);
                            if (!overlay->fadeAnimationPlaying()) {
                                overlay->handleInput(shData.keysDownPending, shData.keysHeld, shData.touchState.count, shData.touchState.touches[0], shData.joyStickPosLeft, shData.joyStickPosRight);
                            }
                            shData.keysDownPending = 0;
                        }
                        #if IS_LAUNCHER_DIRECTIVE
                        if (shouldFireEvent) {
                            shouldFireEvent = false;
                            
                            if (!comboReturn) {
                                triggerExitFeedback();
                            }
                        }
                        #else
                        if (!directMode && shouldFireEvent) {
                            shouldFireEvent = false;
                            #if IS_STATUS_MONITOR_DIRECTIVE
                            if (lastMode.compare("returning") == 0) {
                                triggerExitFeedback();
                            } else {
                                triggerEnterFeedback();
                            }
                            #else
                            if (isReturningLaunch) {
                                triggerExitFeedback();
                            } else {
                                triggerEnterFeedback();
                            }
                            #endif
                        }
                        #endif
                    }
    
                #if IS_STATUS_MONITOR_DIRECTIVE
                    if (pendingExit && wasRendering) {
                        pendingExit = false;
                        if (!(notification && notification->isActive())) {
                            wasRendering = false;
                            isRendering = true;
                            leventClear(&renderingStopEvent);
                        }
                    }
                 #endif
    
                    if (overlay->shouldHide()) {
                        if (overlay->shouldCloseAfter()) {
                            if (!directMode) {
                                shData.running.store(false, std::memory_order_release);
                                shData.overlayOpen.store(false, std::memory_order_release);
                                break;
                            } else {
                                exitAfterPrompt = true;
                            #if IS_STATUS_MONITOR_DIRECTIVE
                                pendingExit = true;
                            #endif
                            }
                        }
                        break;
                    }
    
                    if (overlay->shouldClose()) {
                        shData.running.store(false, std::memory_order_release);
                        shData.overlayOpen.store(false, std::memory_order_release);

                        break;
                    }
                }
    
                if (shData.running.load(std::memory_order_acquire)) {
                    if (!(notification && notification->isActive()))
                        overlay->clearScreen();
                    overlay->resetFlags();
                    hlp::requestForeground(false);
                    shData.overlayOpen.store(false, std::memory_order_release);
                    mainComboHasTriggered.store(false, std::memory_order_release);
                    eventClear(&shData.comboEvent);
                }
            }

            // Ensure background thread is fully stopped before overlay cleanup
            shData.running.store(false, std::memory_order_release);
            threadWaitForExit(&backgroundEventThread);
            threadClose(&backgroundEventThread);
            
            // Cleanup overlay resources
            hlp::requestForeground(false);
            overlay->exitScreen();
            overlay->exitServices();
            delete overlay;
            
            // Stop feedback threads after overlay is closed / deleted.
            feedbackPollerStop.store(true, std::memory_order_release);
            signalFeedback();   // wake both threads so they see the stop flag
            threadWaitForExit(&backgroundHapticsThread);
            threadClose(&backgroundHapticsThread);

            if (!ult::limitedMemory) {
                threadWaitForExit(&backgroundSoundThread);
                threadClose(&backgroundSoundThread);
            }
            
            eventClose(&shData.comboEvent);


            //if (directMode && !launchComboHasTriggered.load(std::memory_order_acquire)) {
            //    if (!disableSound.load(std::memory_order_acquire) && ult::useSoundEffects)
            //        ult::Audio::playExitSound();
            //    if (ult::useHapticFeedback && !skipRumbleDoubleClick) {
            //        ult::rumbleDoubleClickStandalone();
            //    }
            //}
    
            return 0;
        }
    }

}


#ifdef TESLA_INIT_IMPL

namespace tsl::cfg {
    u16 LayerWidth  = 0;
    u16 LayerHeight = 0;
    u16 LayerPosX   = 0;
    u16 LayerPosY   = 0;
    u16 FramebufferWidth  = 0;
    u16 FramebufferHeight = 0;
    u64 launchCombo = KEY_ZL | KEY_ZR | KEY_DDOWN;
    u64 launchCombo2 = KEY_L | KEY_DDOWN | KEY_RSTICK;
}

extern "C" void __libnx_init_time(void);

extern "C" {
    
    u32 __nx_applet_type = AppletType_None;
    u32 __nx_fs_num_sessions = 1;
    u32  __nx_nv_transfermem_size = 0x15000;
    ViLayerFlags __nx_vi_stray_layer_flags = (ViLayerFlags)0;
    
    /**
     * @brief libtesla service initializing function to override libnx's
     *
     */
    void __appInit(void) {
        ASSERT_FATAL(smInitialize()); // needed to prevent issues with powering device into sleep
        
        ASSERT_FATAL(fsInitialize());
        ASSERT_FATAL(hidInitialize());                          // Controller inputs and Touch
        if (hosversionAtLeast(16,0,0)) {
            ASSERT_FATAL(plInitialize(PlServiceType_User));     // Font data. Use pl:u for 16.0.0+
        } else {
            ASSERT_FATAL(plInitialize(PlServiceType_System));   // Use pl:s for 15.0.1 and below to prevent qlaunch/overlaydisp session exhaustion
        }
        ASSERT_FATAL(pmdmntInitialize());                       // PID querying
        ASSERT_FATAL(hidsysInitialize());                       // Focus control
        ASSERT_FATAL(setsysInitialize());                       // Settings querying
        
        // Time initializations
        if R_SUCCEEDED(timeInitialize()) {
            __libnx_init_time();
            timeExit();
        }

        #if USING_WIDGET_DIRECTIVE
        ult::powerInit();
        i2cInitialize();
        #endif

        fsdevMountSdmc();
        splInitialize();
        spsmInitialize();

        #if IS_STATUS_MONITOR_DIRECTIVE
        Service *plSrv = plGetServiceSession();
        Service plClone;
        ASSERT_FATAL(serviceClone(plSrv, &plClone));
        serviceClose(plSrv);
        *plSrv = plClone;
        #endif

        eventCreate(&tsl::notificationEvent, false);
        tsl::notification = new tsl::NotificationPrompt();
    }
    
    /**
     * @brief libtesla service exiting function to override libnx's
     *
     */
    void __appExit(void) {
        delete tsl::notification;
        eventClose(&tsl::notificationEvent);
        if (!ult::limitedMemory)
            ult::Audio::exit();

        spsmExit();
        splExit();
        fsdevUnmountAll();
        
        #if USING_WIDGET_DIRECTIVE
        i2cExit();
        ult::powerExit();
        #endif
        
        fsExit();
        hidExit();
        plExit();
        pmdmntExit();
        hidsysExit();
        setsysExit();
        smExit();

        // Final cleanup
        tsl::gfx::FontManager::cleanup();
    }

}

//#pragma GCC diagnostic pop
#endif