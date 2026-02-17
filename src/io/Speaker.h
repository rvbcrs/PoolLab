#pragma once

#include <Arduino.h>
// #include <Audio.h>

class Speaker {
public:
    Speaker();
    void setup();
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
    
    // Pin Definitions (MAX98357A)
    static const int I2S_BCLK = 42;
    static const int I2S_LRC = 2;
    static const int I2S_DIN = 41;
};

extern Speaker speaker;
