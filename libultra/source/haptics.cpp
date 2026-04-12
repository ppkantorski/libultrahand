/********************************************************************************
 * File: haptics.cpp
 * Author: ppkantorski
 * Description:
 *   This source file provides implementations for the functions declared in
 *   haptics.hpp. These functions manage haptic feedback for the Ultrahand Overlay
 *   using libnx's vibration interfaces. The dedicated haptics thread calls the
 *   standalone (blocking) functions directly, so no timer state machine or
 *   atomic flags are needed here.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)
 *
 *   Note: Please be aware that this notice cannot be altered or removed. It is a part
 *   of the project's documentation and must remain intact.
 * 
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2025-2026 ppkantorski
 ********************************************************************************/

#include "haptics.hpp"

namespace ult {

    // ===== Internal state =====
    static HidVibrationDeviceHandle vibHandheldLeft;
    static HidVibrationDeviceHandle vibHandheldRight;
    static HidVibrationDeviceHandle vibPlayer1Left;
    static HidVibrationDeviceHandle vibPlayer1Right;

    static u32 cachedHandheldStyle = 0;
    static u32 cachedPlayer1Style  = 0;

    // ===== Constants =====
    static constexpr u64 RUMBLE_DURATION_NS = 30'000'000ULL;
    static constexpr u64 DOUBLE_CLICK_PULSE_DURATION_NS = 30'000'000ULL;
    static constexpr u64 DOUBLE_CLICK_GAP_NS = 100'000'000ULL;

    static constexpr HidVibrationValue defaultHapticsPreset = {
        .amp_low   = 0.80f,
        .freq_low  = 210.0f,
        .amp_high  = 0.00f,
        .freq_high = 210.0f
    };

    static constexpr HidVibrationValue handheldHapticsPreset = {
        .amp_low   = 0.76f,
        .freq_low  = 210.0f,
        .amp_high  = 0.00f,
        .freq_high = 210.0f
    };

    static constexpr HidVibrationValue vibrationStop{0};

    // ===== Internal helpers =====

    // Returns the appropriate preset based on the current controller mode.
    // Handheld mode uses handheldHapticsPreset; all others use defaultHapticsPreset.
    static inline const HidVibrationValue& getHapticsPreset() {
        return cachedHandheldStyle ? handheldHapticsPreset : defaultHapticsPreset;
    }

    static inline void sendVibration(const HidVibrationValue* value) {
        if (cachedPlayer1Style) {
            const HidVibrationDeviceHandle handles[2] = { vibPlayer1Left, vibPlayer1Right };
            const HidVibrationValue values[2] = { *value, *value };
            hidSendVibrationValues(handles, values, 2);
        }
        else if (cachedHandheldStyle) {
            const HidVibrationDeviceHandle handles[2] = { vibHandheldLeft, vibHandheldRight };
            const HidVibrationValue values[2] = { *value, *value };
            hidSendVibrationValues(handles, values, 2);
        }
    }

    static inline void sendVibration2x(const HidVibrationValue* value) {
        sendVibration(value);
        sendVibration(value);
    }

    // ===== Public API =====
    void initHaptics() {
        const u32 handheldStyle = hidGetNpadStyleSet(HidNpadIdType_Handheld);
        const u32 player1Style  = hidGetNpadStyleSet(HidNpadIdType_No1);

        vibHandheldLeft = vibHandheldRight = vibPlayer1Left = vibPlayer1Right =
            (HidVibrationDeviceHandle)0;

        static HidVibrationDeviceHandle tmp[2];

        if (handheldStyle) {
            hidInitializeVibrationDevices(tmp, 2, HidNpadIdType_Handheld,
                                          (HidNpadStyleTag)handheldStyle);
            vibHandheldLeft  = tmp[0];
            vibHandheldRight = tmp[1];
        }
        if (player1Style) {
            hidInitializeVibrationDevices(tmp, 2, HidNpadIdType_No1,
                                          (HidNpadStyleTag)player1Style);
            vibPlayer1Left  = tmp[0];
            vibPlayer1Right = tmp[1];
        }

        cachedHandheldStyle = handheldStyle;
        cachedPlayer1Style  = player1Style;
    }

    void deinitHaptics() {
        sendVibration(&vibrationStop);
    }

    void checkAndReinitHaptics() {
        static u32 lastHandheldStyle = 0;
        static u32 lastPlayer1Style  = 0;

        const u32 currentHandheldStyle = hidGetNpadStyleSet(HidNpadIdType_Handheld);
        const u32 currentPlayer1Style  = hidGetNpadStyleSet(HidNpadIdType_No1);

        if ((currentHandheldStyle != lastHandheldStyle) || (currentPlayer1Style != lastPlayer1Style))
            initHaptics();

        lastHandheldStyle   = currentHandheldStyle;
        lastPlayer1Style    = currentPlayer1Style;
        cachedHandheldStyle = currentHandheldStyle;
        cachedPlayer1Style  = currentPlayer1Style;
    }

    void rumbleClickStandalone() {
        const HidVibrationValue& preset = getHapticsPreset();
        sendVibration(&vibrationStop);
        //sendVibration(&preset);
        sendVibration2x(&preset);
        svcSleepThread(RUMBLE_DURATION_NS);
        sendVibration(&vibrationStop);
    }

    void rumbleDoubleClickStandalone() {
        const HidVibrationValue& preset = getHapticsPreset();
        sendVibration(&vibrationStop);
        //sendVibration(&preset);
        sendVibration2x(&preset);
        svcSleepThread(DOUBLE_CLICK_PULSE_DURATION_NS);

        sendVibration(&vibrationStop);
        svcSleepThread(DOUBLE_CLICK_GAP_NS);

        //sendVibration(&preset);
        sendVibration2x(&preset);
        svcSleepThread(DOUBLE_CLICK_PULSE_DURATION_NS);

        sendVibration(&vibrationStop);
    }
}