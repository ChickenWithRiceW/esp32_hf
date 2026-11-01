#include "esp32_hf_intern.hpp"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include <new>

#undef BT_HF_TAG
#define BT_HF_TAG "BT_HF_GAP"

// Try the next bonded phone. Called by a timer while no phone is connected, so a phone
// that is out of range at boot is found later, and several bonded phones take turns.
void EspHf::Impl::try_reconnect(void)
{
    // Do nothing if a phone is connected, or a connection is being set up.
    if (_info.connected || link_state != ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED)
        return;

    int count = esp_bt_gap_get_bond_device_num();
    if (count <= 0)
        return;

    // The list holds one address per bonded device, so it must be sized for all of them.
    esp_bd_addr_t* bonded = new (std::nothrow) esp_bd_addr_t[count];
    if (!bonded)
        return;
    if (esp_bt_gap_get_bond_device_list(&count, bonded) == ESP_OK && count > 0) {
        next_bonded %= count;
        esp_hf_client_connect(bonded[next_bonded]);
        next_bonded++;
    }
    delete[] bonded;
}

void EspHf::Impl::reconnect_timer_cb(TimerHandle_t timer)
{
    (void)timer;
    getInstance()._impl->try_reconnect();
}

bool EspHf::Impl::setup_bt_gap(const char* device_name, bool ssp_enabled, const char* legacy_pairing_pin)
{
    // Set up device name.
    ERROR_CHECK(esp_bt_gap_set_device_name(device_name), "Could not set device name.")

    // Register GAP callback function. (The function that will handle conenction and discovery).
    ERROR_CHECK(esp_bt_gap_register_callback(Impl::esp_bt_gap_cb_wrapper), "Could not register callback.")

    if (ssp_enabled) {
        // Set default parameters for Secure Simple Pairing.
        esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
        esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(uint8_t));
    }
    // Set pincode, and activate callback calls for auth.
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, strlen(legacy_pairing_pin), (uint8_t*)legacy_pairing_pin);

    // Setup how the device will be presented.
    esp_bt_cod_t cod = { 0 };
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;
    cod.minor = 0x01;
    cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_TELEPHONY;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

    // Set discoverable and connectable mode, wait to be connected.
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    return true;
}

void EspHf::Impl::auth_success_check(esp_bt_gap_cb_param_t* param)
{
    if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        ESP_LOGI(BT_HF_TAG, "authentication success: %s",
            param->auth_cmpl.device_name);
        snprintf(_info.device_name, sizeof(_info.device_name), "%s", (char*)param->auth_cmpl.device_name);
        if (callbacks.cb_bt_pin_correct)
            run_cb(callbacks.cb_bt_pin_correct);
    } else {
        ESP_LOGE(BT_HF_TAG, "authentication failed, status: %d",
            param->auth_cmpl.stat);
        _info.device_name[0] = '\0';
        if (callbacks.cb_bt_pin_incorrect)
            run_cb(callbacks.cb_bt_pin_incorrect);
    }
}

void EspHf::Impl::ssp_auth(esp_bd_addr_t bda, uint32_t num_val)
{
    ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32, num_val);
    esp_bt_gap_ssp_confirm_reply(bda, true);

    if (!callbacks.cb_bt_ssp_auth)
        return;
    t_auth* a = new (std::nothrow) t_auth;
    if (!a)
        return;
    snprintf(a->auth_number, sizeof(a->auth_number), "%06" PRIu32, num_val);
    a->fn = callbacks.cb_bt_ssp_auth;

    // The task frees 'a'; if it could not start, free it here.
    if (xTaskCreatePinnedToCore(ssp_auth_wrap, nullptr, TASK_SIZE, (void*)a, 1, nullptr, 1) != pdPASS)
        delete a;
}

void EspHf::Impl::esp_bt_gap_cb_wrapper(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param)
{
    getInstance()._impl->esp_bt_gap_cb(event, param);
}

void EspHf::Impl::esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param)
{
    switch (event) {
    // Name of the connected phone, requested when the phone connects.
    case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
        if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS && memcmp(param->read_rmt_name.bda, peer_addr, ESP_BD_ADDR_LEN) == 0)
            snprintf(_info.device_name, sizeof(_info.device_name), "%s", (char*)param->read_rmt_name.rmt_name);
        break;

    case ESP_BT_GAP_AUTH_CMPL_EVT:
        auth_success_check(param);
        break;

    case ESP_BT_GAP_PIN_REQ_EVT: {
        esp_bt_gap_pin_reply(param->pin_req.bda, true, strlen(bt.legacy_pairing_pin), (uint8_t*)bt.legacy_pairing_pin);
        if (callbacks.cb_bt_legacy_pin)
            run_cb(callbacks.cb_bt_legacy_pin);
        break;
    }

    case ESP_BT_GAP_CFM_REQ_EVT:
        ssp_auth(param->cfm_req.bda, param->cfm_req.num_val);
        break;
    }
}
