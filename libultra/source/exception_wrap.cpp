/********************************************************************************
 * File: exception_wrap.cpp
 * Author: ppkantorski
 * Description:
 *   This source file implements linker-wrapped C++ runtime exception handlers
 *   for the Ultrahand Overlay project.
 *
 *   These functions override the default libstdc++ exception and unwind paths
 *   via the GNU linker --wrap mechanism, forcing an immediate fatal break
 *   (svcBreak) whenever an exception is thrown or unwind is triggered.
 *
 *   This design enforces a strict no-exception runtime model, ensuring that
 *   any unexpected C++ exception behavior results in deterministic failure
 *   rather than stack unwinding or recovery.
 *
 *   Intended use is for embedded Nintendo Switch homebrew environments where
 *   exceptions are disabled and fail-fast debugging is preferred.
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

#include "exception_wrap.hpp"

using namespace ult::wrap;

extern "C" void __wrap___cxa_throw(void* e, void* t, void (*d)(void*)) {
    (void)e; (void)t; (void)d;
    svcBreak(0xDEAD, 0, 0);
    __builtin_trap();
}

extern "C" void __wrap__Unwind_Resume(void* obj) {
    (void)obj;
    svcBreak(0xBEEF, 0, 0);
    __builtin_trap();
}

extern "C" _Unwind_Reason_Code __wrap___gxx_personality_v0(
    int v, _Unwind_Action a, uint64_t c,
    _Unwind_Exception* e,
    _Unwind_Context* ctx) {

    (void)v; (void)a; (void)c; (void)e; (void)ctx;
    svcBreak(0xCAFE, 0, 0);
    __builtin_trap();
    return _URC_FATAL_PHASE1_ERROR;
}