#include "esp32_hf.hpp"
#include "esp_gap_bt_api.h"

#undef BT_HF_TAG
#define BT_HF_TAG "BT_HF_GAP_STACK"

// extern esp_bd_addr_t peer_addr;

// extern char peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
// extern uint8_t peer_bdname_len;


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


void EspHf::esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
        // If nearby Bluetooth devices are discovered.
        case ESP_BT_GAP_DISC_RES_EVT:
        {
            gap_helper_device_discovered(param, this->_peer_addr, this->_peer_bdname, &this->_peer_bdname_len);
            break;
        }
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_DISC_STATE_CHANGED_EVT");

        case ESP_BT_GAP_RMT_SRVCS_EVT:
        case ESP_BT_GAP_RMT_SRVC_REC_EVT:
            break;

        case ESP_BT_GAP_AUTH_CMPL_EVT:
        {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
            {
                ESP_LOGI(BT_HF_TAG, "authentication success: %s",
                        param->auth_cmpl.device_name);
                if (_callbacks.cb_pin_correct)
                    _callbacks.cb_pin_correct();
            }
            else
            {
                ESP_LOGE(BT_HF_TAG, "authentication failed, status:%d",
                        param->auth_cmpl.stat);
                if (_callbacks.cb_pin_incorrect)
                    _callbacks.cb_pin_incorrect();
            }
            break;
        }
        case ESP_BT_GAP_PIN_REQ_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "Input pin code: %s", this->_bt.legacy_pairing_pin);
            esp_bt_gap_pin_reply(param->pin_req.bda, true, strlen(this->_bt.legacy_pairing_pin), (uint8_t *)this->_bt.legacy_pairing_pin);
            break;
        }

    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(
            BT_HF_TAG,
            "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32,
            param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        ESP_LOGI(
            BT_HF_TAG,
            "GOT CALLED AFTER BLOCKING FUNCTION");
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%06" PRIu32,
                 param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;

    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d",
                 param->mode_chg.mode);
        break;

    default:
    {
        ESP_LOGI(BT_HF_TAG, "event: %d", event);
        break;
    }
    }
    return;
}