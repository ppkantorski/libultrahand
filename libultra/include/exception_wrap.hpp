/********************************************************************************
 * File: exception_wrap.hpp
 * Author: ppkantorski
 * Description:
 *   This header defines linker-level exception interception hooks used in the
 *   Ultrahand Overlay project.
 *
 *   It declares __wrap_* functions used by the GNU linker --wrap mechanism to
 *   override libstdc++ exception and unwind entry points.
 *
 *   This module is intended to disable C++ exceptions in a controlled and
 *   deterministic way for Nintendo Switch homebrew environments where
 *   exceptions are not used.
 *
 *   IMPORTANT:
 *   This is a runtime-level override. It must only be enabled when the build
 *   is compiled with -fno-exceptions and corresponding -Wl,-wrap flags.
 *
 *   Special thanks to @masagrator.
 *
 *   https://github.com/ppkantorski/Ultrahand-Overlay
 *
 *  Licensed under GPLv2 + CC-BY-4.0
 *  Copyright (c) 2026 ppkantorski
 ********************************************************************************/

#pragma once

#include <switch.h>
#include <unwind.h>

#if USE_EXCEPTION_WRAP
namespace ult::wrap {
extern "C" {

void __wrap___cxa_throw(void*, void*, void (*)(void*));
void __wrap__Unwind_Resume(void*);
_Unwind_Reason_Code __wrap___gxx_personality_v0(
    int, _Unwind_Action, uint64_t,
    _Unwind_Exception*,
    _Unwind_Context*);

}
}
#endif