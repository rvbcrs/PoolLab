#pragma once

#include <Arduino.h>
// #include <Audio.h>

class Speaker {
public:
    Speaker();
    void setup(int bclk = -1, int lrc = -1, int din = -1);
    void loop();
    
    // Core functions
    void beep(); // Default short beep
    void tone(uint16_t frequency, uint16_t durationMs);
    void alarm(); // Siren pattern
    void setVolume(uint8_t volume); // 0-21
    void mute();
    void unmute();
    
    // Future expansion for files
    void playFile(const char* path);
    void stop();

    // Accessor for the underlying Audio object if needed
    // Audio* getAudio() { return &_audio; }

private:
    // Audio _audio;
    uint8_t _volume;
    uint8_t _savedVolume;
    bool _isMuted;
    
    // Pin Definitions (MAX98357A) — set at runtime via setup()
    int _i2sBclk;
    int _i2sLrc;
    int _i2sDin;
};

extern Speaker speaker;
