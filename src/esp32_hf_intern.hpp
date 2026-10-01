#pragma once

#include "esp32_hf.hpp"

#if ARDUINO
    #include <Arduino.h>
#endif

extern "C" {
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#include "esp32-hal-alloc-bt-classic-mem.h"

#include "ESP_I2S.h"
}

#define MSBC_BITRATE 16000

#define ASSERT_INIT     if (!this->_impl->initialised) {ESP_LOGE(BT_HF_TAG, "Can't call function without begin being called first."); return false;}
#define ASSERT_NOT_INIT if (this->_impl->initialised)  {ESP_LOGE(BT_HF_TAG, "Can't call function if begin was called earlier."); return false;}

#define ERROR_CHECK(fn, msg) if (fn != ESP_OK) {ESP_LOGE(BT_HF_TAG, msg); return false;}

struct t_call {
    void (*fn)(char phone_number[15], bool sim_call) = nullptr;
    char phone_number[15];
    bool is_simcard_call = false;
};

struct t_auth {
    void (*fn)(char[7]) = nullptr;
    char auth_number[7];
};

struct t_bluetooth
{
    // Bluetooth device name of esp. This will be the name you see in the bluetooth menu.
    const char *device_name = nullptr;
    // Bluetooth pin in cases of old phone or secure pairing (ssp) disabled.
    const char *legacy_pairing_pin = nullptr;
    // Secured pairing, if disabled fallback to legacy pin pairing.
    bool ssp_enabled = true;
};

struct t_speaker
{
    I2SClass *p_speaker = nullptr;
    I2SClass speaker;
    uint8_t pcm_clk_pin   = 5;
    uint8_t pcm_fsync_pin = 25;
    uint8_t pcm_dout_pin  = 26;
    uint8_t pcm_din_pin   = 35;
};


struct t_info {
    int     battery_level = 0;
    int     signal_strength = 0;
    e_state state;
    bool    network_available = false;
    bool    connected = false;
};

struct t_callbacks
{
    void (*cb_bt_connected)(void)    = nullptr;
    void (*cb_bt_disconnected)(void) = nullptr;

    void (*cb_bt_pin_correct)(void)   = nullptr;
    void (*cb_bt_pin_incorrect)(void) = nullptr;

    void (*cb_bt_ssp_auth)(char ssp_pin[6]) = nullptr;
    void (*cb_bt_legacy_pin)(void) = nullptr;


    void (*cb_incoming_call)(char phone_number[15], bool sim_call) = nullptr;
    void (*cb_outgoing_call)(char phone_number[15], bool sim_call) = nullptr;
};

struct EspHf::Impl
{
    t_bluetooth  bt;
    t_speaker    speaker;
    t_callbacks  callbacks;

    esp_bd_addr_t peer_addr;
    char          peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    uint8_t       peer_bdname_len = 0;
    bool          already_run  = false;
    bool          initialised  = false;
    t_info _info;

    // This is because of a bug in the system.
    bool          incoming_call = false;

    bool        setup_stacks(void);
    bool        validate_config(void);
    static bool isStrPrintable(const char *str);

    static bool  init_controller_and_bluetooth(bool ssp_enabled);
    static bool  setup_bt_gap(const char *device_name, bool ssp_enabled, const char *legacy_pairing_pin);

    void        esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
    void        bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
    void        call_event(char *phone_number, esp_hf_current_call_direction_t dir);
    void        bt_app_hf_client_incoming_cb(const uint8_t *buf, uint32_t sz);
    void        gap_helper_device_discovered(esp_bt_gap_cb_param_t *param, esp_bd_addr_t peer_addr,
                                                char *peer_bdname, uint8_t *peer_bdname_len);
    void        auth_success_check(esp_bt_gap_cb_param_t *param);
    void        ssp_auth(esp_bd_addr_t bda, uint32_t num_val);

    void        audio_change(esp_hf_client_cb_param_t *param);
    void        connection_change(const esp_hf_client_cb_param_t *param);
    void        call(const char *phone_number, bool is_simcard_call, void (*fn)(char *, bool));

    // ---- C-style trampolines handed to ESP-IDF ----
    static void esp_bt_gap_cb_wrapper(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
    static void bt_app_hf_client_cb_wrapper(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
    static void bt_app_hf_client_incoming_cb_wrapper(const uint8_t *buf, uint32_t sz);

    static void wrap(void *ptr);
    static void call_wrap(void *ptr);
    static void ssp_auth_wrap(void *ptr);
};
