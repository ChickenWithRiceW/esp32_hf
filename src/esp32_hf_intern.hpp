#pragma once

// Internal header: shared state and helpers for the library sources. Not part of the public API.

#include "esp32_hf.hpp"
#include <cstdint>

#if ARDUINO
#include <Arduino.h>
#endif

extern "C" {
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "esp32-hal-alloc-bt-classic-mem.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "ESP_I2S.h"
}

// Constants use 'constexpr'. Macros are only used for code (the ASSERT/ERROR macros
// below) and for the log tag, which needs to be a literal.

// Sample rate of wide-band (mSBC) calls. The speaker is set up with this rate.
static constexpr uint32_t MSBC_RATE = 16000;

// Stack size in bytes of the short tasks that run user callbacks.
static constexpr uint32_t TASK_SIZE = 4096;

// Speaker volume range used by the phone: 0 (quiet) to 15 (loud).
static constexpr int VOLUME_MAX = 15;

// Used at the start of public functions. Reject the call (return false) depending on
// whether begin() has run: setters need it NOT to, actions need it to.
#define ASSERT_INIT                                                                   \
    if (!this->_impl->initialised) {                                                  \
        ESP_LOGE(BT_HF_TAG, "Can't call function without begin being called first."); \
        return false;                                                                 \
    }
#define ASSERT_NOT_INIT                                                          \
    if (this->_impl->initialised) {                                              \
        ESP_LOGE(BT_HF_TAG, "Can't call function if begin was called earlier."); \
        return false;                                                            \
    }

// Log 'msg' and return false from the calling function if an ESP-IDF call failed.
#define ERROR_CHECK(fn, msg)      \
    if (fn != ESP_OK) {           \
        ESP_LOGE(BT_HF_TAG, msg); \
        return false;             \
    }

// Room for "+" + 15 digits + NUL. Longer numbers are truncated.
static constexpr size_t PHONE_NUMBER_LEN = 20;

// Arguments for a call callback. One heap copy is made per event and freed by the callback task.
struct t_call {
    void (*fn)(char phone_number[15], bool sim_call) = nullptr;
    char phone_number[PHONE_NUMBER_LEN] = {};
    bool is_simcard_call = false;
};

// Arguments for the secure pairing callback. Same ownership as t_call.
struct t_auth {
    void (*fn)(char[7]) = nullptr;
    char auth_number[7] = {}; // 6 digits + NUL
};

// Pairing settings.
struct t_bluetooth {
    // Name shown in the phone's Bluetooth menu. Copied by the setter, empty means not set.
    char device_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {};
    // PIN for legacy pairing (old phones, or when SSP is off). Empty means not set.
    char legacy_pairing_pin[ESP_BT_PIN_CODE_LEN + 1] = {};
    // Secure pairing. If disabled, pairing falls back to the legacy PIN.
    bool ssp_enabled = true;
};

// Speaker output.
struct t_speaker {
    // The I2S object audio is written to (user supplied or the one below). Null means no speaker.
    I2SClass* p_speaker = nullptr;
    // Used by setupSpeaker(). ADC capture on the original ESP32 uses I2S0 internally,
    // so playback stays on I2S1 and both can run at the same time.
    I2SClass speaker { I2S_NUM_1 };
};

// Microphone input. Either the ADC or an I2S microphone, never both.
struct t_microphone {
    I2SClass i2s { I2S_NUM_0 };
    int8_t bclk = -1;
    int8_t ws = -1;
    int8_t din = -1;
    int8_t mclk = -1;
    bool configured = false; // A microphone was selected.
    bool use_i2s = false;    // true: I2S microphone, false: ADC.
    uint8_t adc_pin = 0;
};

// Live information about the phone connection.
struct t_info {
    int battery_level = 0;   // As reported by the phone (0-5).
    int signal_strength = 0; // As reported by the phone (0-5).
    // Speaker volume 0-15. The phone only reports it when it changes, so the last
    // value is saved in flash. VOLUME_MAX (no attenuation) is used until one has been saved.
    int volume = VOLUME_MAX;
    e_state state = IDLE_DISCONNECTED;
    bool network_available = false; // Phone has cellular service.
    bool connected = false;         // Phone is connected.
    char device_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {}; // Name of the paired phone.
};

// User callbacks. nullptr means not set.
struct t_callbacks {
    void (*cb_bt_connected)(void) = nullptr;
    void (*cb_bt_disconnected)(void) = nullptr;

    void (*cb_bt_pin_correct)(void) = nullptr;
    void (*cb_bt_pin_incorrect)(void) = nullptr;

    void (*cb_bt_ssp_auth)(char ssp_pin[7]) = nullptr;
    void (*cb_bt_legacy_pin)(void) = nullptr;

    void (*cb_incoming_call)(char phone_number[15], bool sim_call) = nullptr;
    void (*cb_outgoing_call)(char phone_number[15], bool sim_call) = nullptr;
};

// Everything the library keeps and does internally. Hidden from users behind EspHf::_impl.
struct EspHf::Impl {
    t_bluetooth bt;
    t_speaker speaker;
    t_microphone microphone;
    t_callbacks callbacks;
    t_info _info;

    esp_bd_addr_t peer_addr;      // Address of the last connected phone.
    bool already_run = false;     // Bonded phone connect was already attempted.
    bool initialised = false;     // begin() finished successfully.

    // Set when the phone signals a call from another app (not a SIM call), so the next
    // call event is treated as incoming. The Bluetooth stack does not tell us directly.
    bool incoming_call = false;

    // --- Startup (esp32_hf.cpp, hf_gap)
    bool setup_stacks(void);
    bool validate_config(void);
    static bool isStrPrintable(const char* str);
    static bool init_controller_and_bluetooth(bool ssp_enabled);
    static bool setup_bt_gap(const char* device_name, bool ssp_enabled, const char* legacy_pairing_pin);

    // --- Pairing and discovery events (hf_gap)
    void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    void auth_success_check(esp_bt_gap_cb_param_t* param);
    void ssp_auth(esp_bd_addr_t bda, uint32_t num_val);

    // --- Hands-free events: connection, calls, indicators (hf_app)
    void bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param);
    void connection_change(const esp_hf_client_cb_param_t* param);
    void call_event(char* phone_number, esp_hf_current_call_direction_t dir);
    // Start a task that runs a call callback with its own copy of the arguments.
    void call(const char* phone_number, bool is_simcard_call, void (*fn)(char*, bool));

    // --- Audio (esp32_hf_audio.cpp)
    void audio_change(esp_hf_client_cb_param_t* param);
    void bt_app_hf_client_incoming_cb(const uint8_t* buf, uint32_t sz); // Phone audio -> speaker.
    bool start_microphone(uint32_t sample_rate);
    void load_volume(void);
    void save_volume(int volume);

    // --- C-style functions handed to ESP-IDF, they forward to the members above.
    static void esp_bt_gap_cb_wrapper(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    static void bt_app_hf_client_cb_wrapper(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param);
    static void bt_app_hf_client_incoming_cb_wrapper(const uint8_t* buf, uint32_t sz);

    // --- Task entry points that run user callbacks (esp32_hf_callback_wrapper.cpp).
    static void wrap(void* ptr);          // Callback without arguments.
    static void call_wrap(void* ptr);     // Call callback. Frees the t_call.
    static void ssp_auth_wrap(void* ptr); // Pairing code callback. Frees the t_auth.
};
