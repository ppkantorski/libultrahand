#pragma once
#include <switch.h>
#include <atomic>
#include <cstdint>

namespace ult {
	
	extern bool rumbleInitialized;
	extern std::atomic<bool> rumbleActive;
	extern std::atomic<bool> doubleClickActive;
	
	void initRumble();
	void deinitRumble();
	void checkAndReinitRumble();
	
	void rumbleClick();
	void rumbleDoubleClick();
	void processRumbleStop(u64 nowNs);
	void processRumbleDoubleClick(u64 nowNs);
	void rumbleDoubleClickStandalone();
}