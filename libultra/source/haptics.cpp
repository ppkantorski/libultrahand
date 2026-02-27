/********************************************************************************
 * File: haptics.cpp
 * Author: ppkantorski
 * Description:
 *   This source file provides implementations for the functions declared in
 *   haptics.hpp. These functions manage haptic feedback for the Ultrahand Overlay
 *   using libnx’s vibration interfaces. It includes routines for initializing
 *   rumble devices, sending vibration patterns, and handling single or double
 *   click feedback with timing control. Thread safety is maintained through
 *   atomic operations and synchronization mechanisms.
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
    
    // ===== Internal state (private to this file) =====
    static HidVibrationDeviceHandle vibHandheldLeft;
    static HidVibrationDeviceHandle vibHandheldRight;
    static HidVibrationDeviceHandle vibPlayer1Left;
    static HidVibrationDeviceHandle vibPlayer1Right;
    static u64 rumbleStartTick = 0;
    static u64 doubleClickTick = 0;
    static u8  doubleClickPulse = 0;
    
    static u32 cachedHandheldStyle = 0;
    static u32 cachedPlayer1Style  = 0;
    
    // ===== Shared flags (accessible globally) =====
    std::atomic<bool> clickActive{false};
    std::atomic<bool> doubleClickActive{false};
    
    // ===== Constants =====
    static constexpr u64 RUMBLE_DURATION_NS = 30'000'000ULL;
    static constexpr u64 DOUBLE_CLICK_PULSE_DURATION_NS = 30'000'000ULL;
    static constexpr u64 DOUBLE_CLICK_GAP_NS = 100'000'000ULL;
    
    
    static constexpr HidVibrationValue hapticsPreset = {
        .amp_low  = 0.20f,
        .freq_low = 100.0f,
        .amp_high = 0.80f,
        .freq_high = 300.0f
    };

    static constexpr HidVibrationValue vibrationStop{0};
    
    // ===== Internal helpers =====
    static inline void sendVibration(const HidVibrationValue* value) {
        if (cachedHandheldStyle) {
            hidSendVibrationValue(vibHandheldLeft,  value);
            hidSendVibrationValue(vibHandheldRight, value);
        }
    
        if (cachedPlayer1Style) {
            hidSendVibrationValue(vibPlayer1Left,  value);
            hidSendVibrationValue(vibPlayer1Right, value);
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
    
        HidVibrationDeviceHandle tmp[2];
    
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
    
    void checkAndReinitHaptics() {
        static u32 lastHandheldStyle = 0;
        static u32 lastPlayer1Style  = 0;
        
        const u32 currentHandheldStyle = hidGetNpadStyleSet(HidNpadIdType_Handheld);
        const u32 currentPlayer1Style  = hidGetNpadStyleSet(HidNpadIdType_No1);
        
        // Reinitialize only if something changed (appearance/disappearance or style change)
        if ((currentHandheldStyle != lastHandheldStyle) || (currentPlayer1Style != lastPlayer1Style)) {
            initHaptics();
        }
        
        // Update last-known styles for change detection
        lastHandheldStyle = currentHandheldStyle;
        lastPlayer1Style  = currentPlayer1Style;
        
        // Update cached styles used by sendVibration()/rumble paths
        cachedHandheldStyle = currentHandheldStyle;
        cachedPlayer1Style  = currentPlayer1Style;
    }

    
    void rumbleClick() {
        // Use cached style bit instead of querying hid each call
        sendVibration(&vibrationStop);
        sendVibration2x(&hapticsPreset);
        clickActive.store(true, std::memory_order_release);
        rumbleStartTick = armGetSystemTick();

    }
    
    void rumbleDoubleClick() {
        sendVibration(&vibrationStop);
        sendVibration2x(&hapticsPreset);
        doubleClickActive.store(true, std::memory_order_release);
        doubleClickPulse = 1;
        doubleClickTick = armGetSystemTick();  // Set ONCE
    }

    
    void processRumbleStop(u64 nowNs) {
        if (clickActive.load(std::memory_order_acquire) &&
            nowNs - armTicksToNs(rumbleStartTick) >= RUMBLE_DURATION_NS) {
            sendVibration(&vibrationStop);
            clickActive.store(false, std::memory_order_release);
        }
    }
    
    
    void processRumbleDoubleClick(u64 nowNs) {
        if (!doubleClickActive.load(std::memory_order_acquire)) return;
    
        const u64 elapsed = nowNs - armTicksToNs(doubleClickTick);  // Always from original start
    
        switch (doubleClickPulse) {
            case 1:
                if (elapsed >= DOUBLE_CLICK_PULSE_DURATION_NS) {
                    sendVibration(&vibrationStop);
                    doubleClickPulse = 2;
                    // Don't reset tick!
                }
                break;
    
            case 2:
                if (elapsed >= DOUBLE_CLICK_PULSE_DURATION_NS + DOUBLE_CLICK_GAP_NS) {
                    sendVibration2x(&hapticsPreset);
                    doubleClickPulse = 3;
                    // Don't reset tick!
                }
                break;
    
            case 3:
                if (elapsed >= (DOUBLE_CLICK_PULSE_DURATION_NS * 2) + DOUBLE_CLICK_GAP_NS) {
                    sendVibration(&vibrationStop);
                    doubleClickActive.store(false, std::memory_order_release);
                    doubleClickPulse = 0;
                }
                break;
        }
    }


    void rumbleDoubleClickStandalone() {
        // Standalone uses sleeps, but still use cached style for decision
        sendVibration(&vibrationStop);
        sendVibration2x(&hapticsPreset);
        svcSleepThread(DOUBLE_CLICK_PULSE_DURATION_NS);
    
        sendVibration(&vibrationStop);
        svcSleepThread(DOUBLE_CLICK_GAP_NS);

        sendVibration2x(&hapticsPreset);
        svcSleepThread(DOUBLE_CLICK_PULSE_DURATION_NS);
    
        sendVibration(&vibrationStop);
    }
}