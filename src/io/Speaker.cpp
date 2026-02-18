#include "Speaker.h"
#include <driver/i2s.h>
#include <math.h>

Speaker speaker;

// I2S Configuration constants (Matching standard library defaults)
#define I2S_NUM         I2S_NUM_0
#define SAMPLE_RATE     44100
#define BITS_PER_SAMPLE 16

Speaker::Speaker() : _volume(12), _savedVolume(12), _isMuted(false),
                     _i2sBclk(-1), _i2sLrc(-1), _i2sDin(-1) {
}

void Speaker::setup(int bclk, int lrc, int din) {
    _i2sBclk = bclk;
    _i2sLrc  = lrc;
    _i2sDin  = din;

    if (_i2sBclk < 0 || _i2sLrc < 0 || _i2sDin < 0) {
        ESP_LOGW("SPK", "Speaker pins not set — skipping I2S init");
        return;
    }

    // Explicitly install I2S driver for beep() functionality (Direct I2S Write)
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = _i2sBclk,
        .ws_io_num    = _i2sLrc,
        .data_out_num = _i2sDin,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    // Install driver (ignore error if already installed)
    esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        ESP_LOGE("SPK", "I2S driver install failed: %d", err);
    }
    ESP_LOGI("SPK", "I2S driver installed");

    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM);

    // Startup chirp removed (verified working)
}

void Speaker::loop() {
    // Placeholder for future non-blocking audio
}

void Speaker::setVolume(uint8_t volume) {
    if (volume > 21) volume = 21; // Library max is usually 21
    _volume = volume;
    if (!_isMuted) {
        // _audio.setVolume(_volume);
    }
}

void Speaker::mute() {
    if (!_isMuted) {
        _savedVolume = _volume;
        // _audio.setVolume(0);
        _isMuted = true;
    }
}

void Speaker::unmute() {
    if (_isMuted) {
        _volume = _savedVolume;
        // _audio.setVolume(_volume);
        _isMuted = false;
    }
}

void Speaker::playFile(const char* path) {
    // Not implemented yet
}

// Generate a "click" beep (tuned for audibility)
void Speaker::beep() {
    // Subtle click: Short duration (10ms) and moderate frequency (1000Hz)
    // Reduce volume temporarily for the click if needed, but short duration usually limits perceived loudness.
    tone(1000, 15); 
}

void Speaker::alarm() {
    // Two-tone siren effect
    for (int i = 0; i < 3; i++) {
        tone(800, 300);
        tone(1200, 300);
    }
}

// Generate a tone by writing sine wave samples directly to I2S
void Speaker::tone(uint16_t frequency, uint16_t durationMs) {
    if (_isMuted || _volume == 0) {
        ESP_LOGW("SPK", "Tone skipped: Muted=%d Vol=%d", _isMuted, _volume);
        return;
    }
    ESP_LOGI("SPK", "Tone: freq=%d dur=%d vol=%d", frequency, durationMs, _volume);

    // Use a small buffer for the sine wave
    const int buffer_size = 512;
    int16_t samples[buffer_size * 2]; // Stereo buffer
    
    // Calculate sine wave parameters
    double phase_increment = (2.0 * M_PI * frequency) / SAMPLE_RATE;
    double phase = 0;
    
    // Calculate total bytes
    size_t bytes_to_write = (SAMPLE_RATE * durationMs / 1000) * 4; // 16-bit stereo = 4 bytes/frame
    size_t total_written = 0;

    // Volume scaling
    // Library volume 0-21. Map to amplitude.
    // Max amplitude for int16 is 32767. 
    // 21 * 1500 = 31500 (safe max)
    int16_t amplitude = _volume * 1500; 
    
    while (total_written < bytes_to_write) {
        for (int i = 0; i < buffer_size; i++) {
            int16_t sample_val = (int16_t)(amplitude * sin(phase));
            samples[i * 2] = sample_val;     // Left
            samples[i * 2 + 1] = sample_val; // Right
            phase += phase_increment;
            if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
        }

        size_t bytes_written_chunk = 0;
        i2s_write(I2S_NUM, samples, sizeof(samples), &bytes_written_chunk, portMAX_DELAY);
        total_written += bytes_written_chunk;
    }
    
    // Silence at end to prevent pop
    int16_t silence[64] = {0};
    size_t w;
    i2s_write(I2S_NUM, silence, sizeof(silence), &w, portMAX_DELAY);
}
