// Something something explaination

#pragma once
#define ASSERT_INIT if (!this->_initialised) {ESP_LOGE(BT_HF_TAG, "Can't call function without begin being called first."); return false;}
#define ASSERT_NOT_INIT if (this->_initialised) {ESP_LOGE(BT_HF_TAG, "Can't call function if begin was called earlier."); return false;}
#define MSBC_BITRATE 16000


// For log overwriting to append to arduino.
#if ARDUINO
    #include <Arduino.h>
#endif

extern "C" {
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// #include "bt_app_hf.hpp"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
// #include "esp_hf_client_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp32-hal-alloc-bt-classic-mem.h"

#include "ESP_I2S.h"

}

struct esp_hf_client_cb_event_t;
struct esp_hf_client_cb_param_t;

enum esp32_hfp {
    SUCCESS,
    BLUETOOTH_INIT_FAILED
};

struct t_arg {
    void (*fn)(char phone_number[15], bool sim_call) = nullptr;
    char phone_number[15];
    bool is_simcard_call = false;
};



struct t_bluetooth
{
    // Bluetooth device name of esp. This will be the name you see in the bluetooth menu.
    const char *device_name = nullptr;
    // Bluetooth pin in cases of old phone or secure paring (ssp) disabled.
    const char* legacy_pairing_pin = nullptr;
    // Secured pairing, if disabled fallback to legacy pin paring.
    bool ssp_enabled = true;
    
    // ESP will automatically connect to any host if not defined automatically.
    bool auto_connect_to_host = false;
};

struct t_speaker
{
    I2SClass *p_speaker;
    I2SClass speaker;
    uint8_t pcm_clk_pin = 5;
    uint8_t pcm_fsync_pin = 25;
    uint8_t pcm_dout_pin = 26;
    uint8_t pcm_din_pin = 35;
};

struct t_callbacks
{
    // Gap
    void (*cb_bt_connected)(void);
    void (*cb_bt_disconnected)(void);



    void (*cb_bt_pin_correct)(void);
    void (*cb_bt_pin_incorrect)(void);



    void (*cb_incoming_call)(char phone_number[15], bool sim_call);
    void (*cb_outgoing_call)(char phone_number[15], bool sim_call);
};


class EspHf
{
    public:
        static EspHf& getInstance() {
            static EspHf obj;
            return obj;
        }

    /**
     * @brief Setup bluetooth controller and enable classic bluetooth with defiend settings. DO NOT call without having set device name and legacy pin.
     * @return True if initializing was successful, false if unsuccessful.
     */
    bool begin();


    // Setters

    /**
     * @brief Set device name of bluetooth device. Ensure that the string will not be freed at runtime.
     * 
     * This is runtime changable but will only be visable when the AG Host disconnects from client and reconnects.
     * @param device_name String representing device name.
     * @return True if action was successful, false if error accrued.
     */
    bool setDeviceName(const char *device_name);

    // ! NO CHECK FOR IT YET
    /**
     * @brief Set legacy pin. Legacy pin has a max length of 16 and should only contain numbers and/or letters.
     * @brief Pin should be at least one character long.
     * @param legacy_pin String representing of legacy pin.
     * @return True if action was successful, false if error accrued.
     */
    bool setLegacyPin(const char *legacy_pin);

    /**
     * @brief Set I2s object for speaker. Object must be init with setPin and begin.
     * @param speaker Pointer to I2sclass object.
     * @return True if action was successful, false if error accrued.
     */
    bool setSpeakerI2s(I2SClass *speaker);


    /**
     * @brief Set i2s speaker internally.
     * @param
     * @return Poiner to i2s object or NULL if inizilation failed.
     */
    I2SClass *setSpeaker(int8_t bclk, int8_t ws, int8_t dout, int8_t din = (int8_t)(-1), int8_t mclk = (int8_t)(-1));

    /**
     * @brief Disable secure paring (ssp). If disabled legacy pin will be used.
     * @return True if action was successful, false if error accrued.
     */
    bool setSecurePairing(bool enable_ssp);

    // Getters

    // Callback setters.

    /**
     * @brief Set callback for incoming phone calls.
     * 
     * Callback can be changed at runtime.
     * @return True if action was successful, false if error accrued.
     */
     
     
     
    bool setCbIncomingCall(void (*ptr)(char phone_number[15], bool sim_call));
    bool setCbOutgoingCall(void (*ptr)(char phone_number[15], bool sim_call));
    bool setCbPinCorrect(void (*ptr)(void));
    bool setCbPinIncorrect(void (*ptr)(void));

    bool setCbBtConnected(void (*ptr)(void));
    bool setCbBtDisconnected(void (*ptr)(void));

    static void wrap(void *ptr);
    void pin_check(esp_bt_gap_cb_param_t *param);


    // Actions

    /**
     * @brief Accept incoming call.
     * @return Return true regardless of if there is a incoming call present or not. If begin was not called before return false.
     */
    bool acceptIncomingCall(void);

    /**
     * @brief Reject incoming call or outgoing call.
     * @return Return true regardless of if there is a incoming/outgoing call present or not. If begin was not called before return false.
     */
    bool rejectIncomingCall(void);

    
    private:

        EspHf(void);
        void setup_stacks(void);
        static void bt_app_hf_client_cb_wrapper(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
        static int init_controller_and_bluetooth(bool ssp_enabled);
        
        static int setup_bt_gap(const char *device_name, bool ssp_enabled, const char* legacy_pairing_pin);
        void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
        static void esp_bt_gap_cb_wrapper(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
        void bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
        void bt_app_hf_client_incoming_cb(const uint8_t *buf, uint32_t sz);
        static void bt_app_hf_client_incoming_cb_wrapper(const uint8_t *buf, uint32_t sz);
        void gap_helper_device_discovered(esp_bt_gap_cb_param_t *param, esp_bd_addr_t peer_addr, char *peer_bdname, uint8_t *peer_bdname_len);



        void audio_change(esp_hf_client_cb_param_t *param);
        void connection_change(const esp_hf_client_cb_param_t *param);
        void call(const char *phone_number, bool is_simcard_call, void (*fn)(char *, bool));


        // Callback Wrapper
        static void cb_con_wrap(void *ptr);
        static void cb_dis_wrap(void *ptr);

        static void call_wrap(void *ptr);

        
        bool validate_config(void);
        
        t_bluetooth _bt;
        t_speaker _speaker;
        t_callbacks _callbacks;

        esp_bd_addr_t _peer_addr;
        char _peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
        uint8_t _peer_bdname_len;
        bool _already_run = false;
        bool _initialised = false;
        bool _connected = false;

        // This is because of a bug in the system.
        bool _incoming_call = false;



        // Misc

        bool isStrPrintable(const char *str);
};

