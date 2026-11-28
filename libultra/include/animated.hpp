/********************************************************************************
 * File: animated.hpp
 * Author: ppkantorski
 * Description: 
 *   'animated.hpp' declares animated wallpapers (.uaw, "Ultrahand animated
 *   wallpaper") for the Ultrahand Overlay project. A .uaw file is made on a PC with
 *   extra/make_uaw.py and streamed from the SD card while it plays: a low-priority
 *   reader thread keeps a small ring of upcoming frame records, and the render thread
 *   decodes each due frame straight into the regular wallpaper buffer (RGBA4444, or
 *   8-bit RGB with full-colour output). It follows the same rules as a still
 *   wallpaper. The implementation lives in 'animated.cpp'.
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
#include <atomic>
#include <string>

namespace ult::anim {

    // The active animated wallpaper is named in wallpaper.ini ([wallpaper] animated=<.uaw
    // path>, see ult::wallpaperIniPath). The .uaw itself stays in the wallpapers folder.

    extern std::atomic<bool> s_active;             // a .uaw is playing into wallpaperData
    // A smaller read-ahead, for an overlay with less room than its heap size gives (0: none).
    // Still at least 256 KB and the clip's largest record.
    extern u32 s_ringCapLimit;

    /**
     * @brief Starts playing the .uaw at path into wallpaperData.
     *
     * wallpaperData is sized and filled here: with the frame the last overlay left off on
     * if resume is set and its saved state matches, otherwise with frame 0. The wallpaper
     * adjustments (image_fx.hpp) are baked into every decoded block.
     * The caller holds wallpaperMutex with no frame drawing (the wallpaper load / reload
     * protocol).
     *
     * @param path   The .uaw file.
     * @param resume Continue from the saved state if it matches.
     * @return False if the path is empty or the file can't be played.
     */
    bool startFromPath(const std::string& path, bool resume = true);

    /**
     * @brief Stops playback (reader thread, file and ring). wallpaperData is left to the caller.
     */
    void stop();

    /**
     * @brief Before a reboot or power off (any thread): stops the reader and closes the .uaw, so
     * nothing of ours reads the SD card while the system shuts down. The picture stays as it is.
     */
    void stopForPowerOff();

    /**
     * @brief Saves the picture on screen, its baked-in adjustments and the playback position.
     *
     * The next overlay built with this library then continues from the same frame
     * (startFromPath resumes it if the .uaw and the colour format match). Called when an
     * overlay exits: it stops the reader thread first (playback ends here).
     */
    void saveState();

    /**
     * @brief Deletes the saved state, so the next .uaw starts at frame 0 (the wallpaper was changed).
     */
    void clearState();

    /**
     * @brief Decodes the frames that are due into wallpaperData.
     *
     * Render thread, once per frame before anything is drawn. Cheap when nothing is due.
     */
    void frame();

} // namespace ult::anim
