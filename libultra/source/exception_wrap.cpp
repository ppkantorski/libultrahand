/********************************************************************************
 * File: exception_wrap.cpp
 * Author: ppkantorski
 * Description:
 *   Implements linker-wrapped C++ exception and unwind handlers for Ultrahand.
 *
 *   This module disables C++ exception propagation by overriding libstdc++
 *   runtime hooks using the GNU linker --wrap mechanism.
 *
 *   Behavior:
 *   - __cxa_throw: immediate abort (no exception propagation allowed)
 *   - __Unwind_Resume: abort (prevents invalid stack unwind continuation)
 *   - __gxx_personality_v0: abort (prevents unwind metadata traversal)
 *
 *   This ensures a strict no-exception runtime model suitable for embedded
 *   Nintendo Switch homebrew environments.
 *
 *   Special thanks to @masagrator.
 *
 *   https://github.com/ppkantorski/Ultrahand-Overlay
 *
 *  Licensed under GPLv2 + CC-BY-4.0
 *  Copyright (c) 2026 ppkantorski
 ********************************************************************************/

#include "exception_wrap.hpp"
#include <cstdlib>

#if USE_EXCEPTION_WRAP
using namespace ult::wrap;

extern "C" void __wrap___cxa_throw(void* e, void* t, void (*d)(void*)) {
    (void)e;
    (void)t;
    (void)d;

    // Exception usage is not allowed in this build
    abort();
}

extern "C" void __wrap__Unwind_Resume(void* obj) {
    (void)obj;

    // Unwind continuation is unsafe in no-exception builds
    abort();
}

extern "C" _Unwind_Reason_Code __wrap___gxx_personality_v0(
    int v,
    _Unwind_Action a,
    uint64_t c,
    _Unwind_Exception* e,
    _Unwind_Context* ctx) {

    (void)v;
    (void)a;
    (void)c;
    (void)e;
    (void)ctx;

    // Personality routine should never be reached in valid builds
    abort();

    return _URC_FATAL_PHASE1_ERROR;
}
#endif