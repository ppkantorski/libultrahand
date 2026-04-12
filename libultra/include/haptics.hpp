/********************************************************************************
 * File: haptics.hpp
 * Author: ppkantorski
 * Description:
 *   This header declares functions for managing haptic feedback in the
 *   Ultrahand Overlay. It provides interfaces for initializing vibration
 *   devices and triggering blocking rumble patterns used by the dedicated
 *   haptics thread.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)
 *
 *   Note: Please be aware that this notice cannot be altered or removed. It is a part
 *   of the project's documentation and must remain intact.
 * 
 *   Licensed under both GPLv2 and CC-BY-4.0
 *   Copyright (c) 2025-2026 ppkantorski
 ********************************************************************************/

#pragma once
#include <switch.h>

namespace ult {

    void initHaptics();
    void deinitHaptics();
    void checkAndReinitHaptics();

    // Blocking rumble patterns — safe to call from a dedicated haptics thread.
    // Both functions return only after the full haptic sequence has completed.
    void rumbleClickStandalone();
    void rumbleDoubleClickStandalone();
}