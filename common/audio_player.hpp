#pragma once
#include <switch.h>
#include <vector>
#include <string>

class AudioPlayer {
public:
    enum class SoundType {
        Navigate,
        Enter,
        Exit,
        Count
    };

    struct SoundConfig {
        float frequency;
        float duration;
        float volume;
        float attack;
        float decay;
        bool useEnvelope;
        std::string name;
    };

    struct CachedSound {
        void* buffer = nullptr;
        size_t bufferSize = 0;
        bool playing; // <- new
    };

    static bool initialize();
    static void exit();
    static void playSound(SoundType type);
    static void playNavigateSound(); // Tesla compatibility
    static void playEnterSound(); // Tesla compatibility
    static void playExitSound(); // Tesla compatibility
    static void setMasterVolume(float volume);
    static void setEnabled(bool enabled);
    static bool isEnabled();

private:
    static bool m_initialized;
    static bool m_enabled;
    static float m_masterVolume;
    static std::vector<SoundConfig> m_soundConfigs;
    static std::vector<CachedSound> m_cachedSounds;

    static void initializeDefaultConfigs();
    //static void pregenerateSoundBuffers();
    //static void generateTone(void* buffer, size_t size, const SoundConfig& config);
    //static void applyEnvelope(float* sample, float time, float duration, float attack, float decay);
    static void playAudioBuffer(void* buffer, size_t bufferSize);
    static bool loadSoundFromWav(SoundType type, const char* path);
};