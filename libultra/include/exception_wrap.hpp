/********************************************************************************
 * File: exception_wrap.hpp
 * Author: ppkantorski
 * Description:
 *   This header defines linker-level exception interception hooks used in the
 *   Ultrahand Overlay project. It provides declarations for __wrap_* functions
 *   that override the C++ exception and unwind runtime when linked with
 *   -Wl,-wrap flags.
 *
 *   These hooks are intended for deterministic fail-fast behavior in embedded
 *   environments (Nintendo Switch homebrew), where C++ exceptions are disabled
 *   and runtime exception handling is treated as a fatal error condition.
 *
 *   This module is optional and must be explicitly enabled via build system
 *   linker flags (-Wl,-wrap,__cxa_throw, etc.).
 *
 *   Special thanks to @masagrator.
 * 
 *   For the latest updates and contributions, visit the project's GitHub repository:
 *   GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay
 *
 *   Note: This notice is integral to the project's documentation and must not be
 *   altered or removed.
 *
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2026 ppkantorski
 ********************************************************************************/


#pragma once
#include <switch.h>
#include <unwind.h>

namespace ult::wrap {

extern "C" void __wrap___cxa_throw(void*, void*, void (*)(void*));
extern "C" void __wrap__Unwind_Resume(void*);
extern "C" _Unwind_Reason_Code __wrap___gxx_personality_v0(
    int, _Unwind_Action, uint64_t,
    _Unwind_Exception*,
    _Unwind_Context*);

}