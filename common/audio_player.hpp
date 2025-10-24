#pragma once
#include <switch.h>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>


class AudioPlayer {
public:
    enum class SoundType {
        Navigate,
        Enter,
        Exit,
        Wall,
        Count
    };
    
    struct CachedSound {
        void* buffer = nullptr;
        size_t bufferSize = 0;
        bool playing; // <- new
    };

    static bool initialize();
    static void exit();
    static void playSound(SoundType type);
    static void playNavigateSound();
    static void playEnterSound();
    static void playExitSound();
    static void playWallSound();
    static void setMasterVolume(float volume);
    static void setEnabled(bool enabled);
    static bool isEnabled();

private:
    static bool m_initialized;
    static bool m_enabled;
    static float m_masterVolume;
    static std::vector<CachedSound> m_cachedSounds;

    static void initializeDefaultConfigs();
    static void playAudioBuffer(void* buffer, size_t bufferSize);
    static bool loadSoundFromWav(SoundType type, const char* path);
};