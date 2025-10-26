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
    
private:
    static bool m_initialized;
    static bool m_enabled;
    static float m_masterVolume;
    static std::vector<CachedSound> m_cachedSounds;
    static std::mutex m_audioMutex;
    static bool m_isPlaying;
    static AudioOutBuffer m_activeBuffer;
    
    static void playAudioBuffer(void* buffer, uint32_t bufferSize);
    static bool loadSoundFromWav(SoundType type, const char* path);
};