// Something something explaination

#pragma once

// Arduino specific.
#if ARDUINO
    #include <Arduino.h>


#endif

extern "C" {
// #include "esp_gap_bt_api.h"    
// #include "esp_hf_client_api.h"
// #include "esp_bt_main.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bt_app_hf.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "gpio_pcm_config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp32-hal-alloc-bt-classic-mem.h"
}

enum esp32_hfp {
    SUCCESS,
    BLUETOOTH_INIT_FAILED
};



// Standard setup.
struct HfpCarKitConfig
{
    // Pins
    struct Pins
    {
        uint8_t pcm_clk_pin = 5;
        uint8_t pcm_fsync_pin = 25;
        uint8_t pcm_dout_pin = 26;
        uint8_t pcm_din_pin = 35;
    } pins;

    struct Bluetooth
    {
        const char *device_name = nullptr;
        const char* legacy_pairing_pin = nullptr;

        // Secured pairing, if disabled fallback to legacy pin paring.
        bool ssp_enabled = true;

        // ESP will automatically connect to any host if not defined automatically.
        bool auto_connect_to_host = false;
        // const char *host_device_name = "AG-Host";
    } bluetooth;
};

class EspHf
{
    public:
        static EspHf& getInstance() {
            static EspHf obj;
        return obj;
    }
    // Setters
    bool setDeviceName(const char *str);
    bool setLegacyPin(const char *str);
    void disableSecurePairing(void);
    
    int begin();
    
    private:
        EspHf(void);
        void setup_stacks(void);
        static void bt_app_hf_client_cb_wrapper(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
        static int init_controller_and_bluetooth(bool ssp_enabled);
        
        static int setup_bt_gap(const char *device_name, bool ssp_enabled, const char* legacy_pairing_pin);
        void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
        static void esp_bt_gap_cb_wrapper(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
        void bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
        void gap_helper_device_discovered(esp_bt_gap_cb_param_t *param, esp_bd_addr_t *peer_addr, char *peer_bdname, uint8_t *peer_bdname_len);
        
        bool validate_config(void);
        
        HfpCarKitConfig cfg;
        esp_bd_addr_t peer_addr;
        char peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
        uint8_t peer_bdname_len;
};

