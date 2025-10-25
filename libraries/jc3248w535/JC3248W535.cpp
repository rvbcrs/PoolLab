#include "JC3248W535.h"
#include "jc3248w535.h"
#include "esp_log.h"

static const char *TAG = "JC3248W535";

// Static members
bool JC3248W535::_initialized = false;
jc3248w535_handles_t JC3248W535::_handles = {nullptr, nullptr};

bool JC3248W535::begin(int rotation, int backlight) {
    if (_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    jc3248w535_config_t config = JC3248W535_DEFAULT_CONFIG(rotation);
    config.backlight_percent = backlight;
    
    esp_err_t ret = jc3248w535_begin(&config, &_handles);
    if (ret == ESP_OK) {
        _initialized = true;
        ESP_LOGI(TAG, "Initialized successfully (rotation: %d°, backlight: %d%%)", rotation, backlight);
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to initialize: %s", esp_err_to_name(ret));
        return false;
    }
}

bool JC3248W535::begin(const jc3248w535_config_t* config, jc3248w535_handles_t* handles) {
    if (_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Config is null");
        return false;
    }
    
    jc3248w535_handles_t* target_handles = handles ? handles : &_handles;
    
    esp_err_t ret = jc3248w535_begin(config, target_handles);
    if (ret == ESP_OK) {
        _initialized = true;
        if (!handles) {
            _handles = *target_handles;
        }
        ESP_LOGI(TAG, "Initialized successfully with custom config");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to initialize: %s", esp_err_to_name(ret));
        return false;
    }
}

void JC3248W535::setBacklight(int percent) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    jc3248w535_backlight_set(percent);
    ESP_LOGI(TAG, "Backlight set to %d%%", percent);
}

void JC3248W535::setRotation(int degrees) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    // Note: This would require reinitializing the display
    // For now, just log a warning
    ESP_LOGW(TAG, "Rotation change not supported after initialization");
}

bool JC3248W535::lock(uint32_t timeout_ms) {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return false;
    }
    
    return jc3248w535_lock(timeout_ms);
}

void JC3248W535::unlock() {
    if (!_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    jc3248w535_unlock();
}

lv_disp_t* JC3248W535::getDisplay() {
    return _initialized ? _handles.disp : nullptr;
}

lv_indev_t* JC3248W535::getInputDevice() {
    return _initialized ? _handles.indev : nullptr;
}

bool JC3248W535::isInitialized() {
    return _initialized;
}

// C API implementation
extern "C" {

bool jc3248w535_init(int rotation, int backlight) {
    return JC3248W535::begin(rotation, backlight);
}

void jc3248w535_set_backlight(int percent) {
    JC3248W535::setBacklight(percent);
}

void jc3248w535_set_rotation(int degrees) {
    JC3248W535::setRotation(degrees);
}

bool jc3248w535_lock(uint32_t timeout_ms) {
    return JC3248W535::lock(timeout_ms);
}

void jc3248w535_unlock(void) {
    JC3248W535::unlock();
}

lv_disp_t* jc3248w535_get_display(void) {
    return JC3248W535::getDisplay();
}

lv_indev_t* jc3248w535_get_input_device(void) {
    return JC3248W535::getInputDevice();
}

bool jc3248w535_is_initialized(void) {
    return JC3248W535::isInitialized();
}

} // extern "C"










