#include "esp32_hf.hpp"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"

#undef BT_HF_TAG
#define BT_HF_TAG "BT_HF_GAP_STACK"


static bool get_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir)
        return false;

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);

    // if (!rmt_bdname)
    //     rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);

    if (rmt_bdname)
    {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN)
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;

        if (bdname)
        {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len)
            *bdname_len = rmt_bdname_len;

        return true;
    }
    return false;
}

void EspHf::gap_helper_device_discovered(esp_bt_gap_cb_param_t *param, esp_bd_addr_t peer_addr, char *peer_bdname, uint8_t *peer_bdname_len) {
    for (int i = 0; i < param->disc_res.num_prop; i++)
    {
        if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR &&
            get_name_from_eir((uint8_t *)param->disc_res.prop[i].val, peer_bdname, peer_bdname_len))
            {
                // Copy bluetooth address into peer_addr.
                memcpy(peer_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);

                ESP_LOGI(BT_HF_TAG, "Found a target device address:");
                ESP_LOG_BUFFER_HEX(BT_HF_TAG, peer_addr, ESP_BD_ADDR_LEN);
                ESP_LOGI(BT_HF_TAG, "Found a target device name: %s", peer_bdname);

                // Connect to new AG host.
                esp_hf_client_connect(peer_addr);
                
                // Stop discovery.
                esp_bt_gap_cancel_discovery();
            }
        }
}


int EspHf::setup_bt_gap(const char *device_name, bool ssp_enabled, const char* legacy_pairing_pin)
{
    // Set up device name.
    esp_bt_gap_set_device_name(device_name);

    // Register GAP callback function. (The function that will handle conenction and discovery)
    esp_bt_gap_register_callback(EspHf::esp_bt_gap_cb_wrapper);

    if (ssp_enabled)
    {
        // Set default parameters for Secure Simple Pairing
        esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
        esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
        esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
    }
    // Set 
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, strlen(legacy_pairing_pin), (uint8_t *)legacy_pairing_pin);

    // Set discoverable and connectable mode, wait to be connected
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    return true;
}

void EspHf::esp_bt_gap_cb_wrapper(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    getInstance().esp_bt_gap_cb(event, param);
}

void EspHf::pin_check(esp_bt_gap_cb_param_t *param) {
    if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGI(BT_HF_TAG, "authentication success: %s",
                param->auth_cmpl.device_name);
        if (_callbacks.cb_bt_pin_correct)
            xTaskCreatePinnedToCore(wrap, nullptr, 4096, (void *)_callbacks.cb_bt_pin_correct, 1, nullptr, 1);
    }
    else
    {
        ESP_LOGE(BT_HF_TAG, "authentication failed, status:%d",
                param->auth_cmpl.stat);
        if (_callbacks.cb_bt_pin_incorrect)
            xTaskCreatePinnedToCore(wrap, nullptr, 4096, (void *)_callbacks.cb_bt_pin_incorrect, 1, nullptr, 1);
    }
}

void EspHf::esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
        case ESP_BT_GAP_DISC_RES_EVT:
            gap_helper_device_discovered(param, this->_peer_addr, this->_peer_bdname, &this->_peer_bdname_len); break;

        case ESP_BT_GAP_AUTH_CMPL_EVT:
            pin_check(param); break;

        case ESP_BT_GAP_PIN_REQ_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "Input pin code: %s", this->_bt.legacy_pairing_pin);
            esp_bt_gap_pin_reply(param->pin_req.bda, true, strlen(this->_bt.legacy_pairing_pin), (uint8_t *)this->_bt.legacy_pairing_pin);
            break;
        }

        case ESP_BT_GAP_CFM_REQ_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32, param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        }
        default:
            ESP_LOGI(BT_HF_TAG, "Event: %d", event);
    }
}
