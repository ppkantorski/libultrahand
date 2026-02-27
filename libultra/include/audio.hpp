/********************************************************************************
 * File: audio.hpp
 * Author: ppkantorski
 * Description:
 *   This header defines the Audio class and related structures used for
 *   handling sound playback within the Ultrahand Overlay. It provides interfaces
 *   for loading, caching, and playing WAV audio through libnx's audout service,
 *   along with basic sound type management and synchronization support.
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
#include <cstdio>
#include <algorithm>
#include <vector>
#include <atomic>
#include <cstring>
#include <mutex>
#include "tsl_utils.hpp"

namespace ult {
    class Audio {
    public:
        enum class SoundType : uint8_t {
            Navigate,
            Enter,
            Exit,
            Wall,
            On,
            Off,
            Settings,
            Move,
            Notification,
            Count
        };

        struct CachedSound {
            // Compact raw PCM — native channel count, 16-bit, no volume applied.
            // Retained in memory so volume/dock changes can rebake without SD I/O.
            void*    rawBuf  = nullptr;
            uint32_t rawSize = 0;   // actual data bytes
            uint32_t rawCap  = 0;   // allocated (aligned) bytes

            // Pre-baked DMA-ready stereo PCM — mono expanded L+R, volume applied.
            // Submitted directly to audout so playSound has zero per-sample work.
            void*    sterBuf  = nullptr;
            uint32_t sterSize = 0;   // actual stereo data bytes
            uint32_t sterCap  = 0;   // allocated (aligned) bytes

            AudioOutBuffer audoutBuf = {};  // owns the descriptor submitted to audout

            bool isMono = false;
            bool stale  = true;  // sterBuf needs rebaking (set after volume/dock change)
        };

        static bool initialize();
        static void exit();

        static inline bool allSoundsExist() {
            for (const auto& path : m_soundPaths)
                if (!isFile(path)) return false;
            return true;
        }

        static void playSound(SoundType type);

        static inline void playNavigateSound()     { playSound(SoundType::Navigate);     }
        static inline void playEnterSound()        { playSound(SoundType::Enter);        }
        static inline void playExitSound()         { playSound(SoundType::Exit);         }
        static inline void playWallSound()         { playSound(SoundType::Wall);         }
        static inline void playOnSound()           { playSound(SoundType::On);           }
        static inline void playOffSound()          { playSound(SoundType::Off);          }
        static inline void playSettingsSound()     { playSound(SoundType::Settings);     }
        static inline void playMoveSound()         { playSound(SoundType::Move);         }
        static inline void playNotificationSound() { playSound(SoundType::Notification); }

        static void setMasterVolume(float volume);
        static void setEnabled(bool enabled);
        static bool isEnabled();
        static bool reloadIfDockedChanged();
        static void reloadAllSounds();
        static void unloadAllSounds(const std::initializer_list<SoundType>& excludeSounds = {});
        static std::mutex m_audioMutex;

    private:
        static bool                     m_initialized;
        static std::atomic<bool>        m_enabled;
        static std::atomic<int32_t>     m_masterVolumeFixed;  // volume as 0–256 fixed-point
        static bool                     m_lastDockedState;
        static std::vector<CachedSound> m_cachedSounds;

        inline static constexpr const char* m_soundPaths[static_cast<size_t>(SoundType::Count)] = {
            "sdmc:/config/ultrahand/sounds/tick.wav",
            "sdmc:/config/ultrahand/sounds/enter.wav",
            "sdmc:/config/ultrahand/sounds/exit.wav",
            "sdmc:/config/ultrahand/sounds/wall.wav",
            "sdmc:/config/ultrahand/sounds/on.wav",
            "sdmc:/config/ultrahand/sounds/off.wav",
            "sdmc:/config/ultrahand/sounds/settings.wav",
            "sdmc:/config/ultrahand/sounds/move.wav",
            "sdmc:/config/ultrahand/sounds/notification.wav"
        };

        // Loads WAV into rawBuf (16-bit native channels, no volume),
        // then immediately bakes sterBuf. Must hold m_audioMutex.
        static bool loadSoundFromWav(SoundType type, const char* path);

        // Writes rawBuf → sterBuf with current volume. Must hold m_audioMutex.
        // sterBuf capacity grows on first call and reuses the same allocation
        // for all subsequent rebakes of the same sound (same sample count → same size).
        // Returns false only on initial allocation failure.
        static bool bakeStereo(CachedSound& s);
    };
}