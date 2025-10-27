#pragma once
#include <switch.h>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <mutex>

class AudioPlayer {
public:
    enum class SoundType {
        Navigate,
        Enter,
        Exit,
        Wall,
        On,
        Off,
        Settings,
        Move,
        Count
    };
    
    struct CachedSound {
        void* buffer = nullptr;
        uint32_t bufferSize = 0;
        uint32_t dataSize = 0;
    };
    
    static bool initialize();
    static void exit();
    static void playSound(SoundType type);
    static void playNavigateSound();
    static void playEnterSound();
    static void playExitSound();
    static void playWallSound();
    static void playOnSound();
    static void playOffSound();
    static void playSettingsSound();
    static void playMoveSound();
    static void setMasterVolume(float volume);
    static void setEnabled(bool enabled);
    static bool isEnabled();
    static bool isDocked();
    static bool reloadIfDockedChanged();
    
private:
    static bool m_initialized;
    static bool m_enabled;
    static float m_masterVolume;
    static std::vector<CachedSound> m_cachedSounds;
    static std::mutex m_audioMutex;
    static bool m_lastDockedState;
    
    // DECLARE but don't define in header - definition goes in .cpp
    inline static constexpr const char* m_soundPaths[static_cast<size_t>(SoundType::Count)] = {
        "sdmc:/config/ultrahand/sounds/tick.wav",
        "sdmc:/config/ultrahand/sounds/enter.wav",
        "sdmc:/config/ultrahand/sounds/exit.wav",
        "sdmc:/config/ultrahand/sounds/wall.wav",
        "sdmc:/config/ultrahand/sounds/on.wav",
        "sdmc:/config/ultrahand/sounds/off.wav",
        "sdmc:/config/ultrahand/sounds/settings.wav",
        "sdmc:/config/ultrahand/sounds/move.wav"
    };
    
    static void playAudioBuffer(void* buffer, uint32_t bufferSize);
    static bool loadSoundFromWav(SoundType type, const char* path);
    static void reloadAllSounds();
};
