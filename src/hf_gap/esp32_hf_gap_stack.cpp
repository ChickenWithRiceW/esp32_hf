#include "esp32_hf.h"
#undef BT_HF_TAG
#define BT_HF_TAG "BT_HF_GAP_STACK"


#include "esp_gap_bt_api.h"
// extern esp_bd_addr_t peer_addr;

// extern char peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
// extern uint8_t peer_bdname_len;


int EspHf::setup_bt_gap(const char *device_name, bool ssp_enabled, const char* legacy_pairing_pin)
{
    // Set up device name.
    esp_bt_gap_set_device_name(device_name);

    // Register GAP callback function. (The function that will handle conenction and discovery)
    esp_bt_gap_register_callback(EspHf::esp_bt_gap_cb);

    if (ssp_enabled)
    {
        /* Set default parameters for Secure Simple Pairing */
        esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
        esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
        esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
    }

    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, strlen(legacy_pairing_pin), (uint8_t *)legacy_pairing_pin);

    /* set discoverable and connectable mode, wait to be connected */
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    return true;
}


void EspHf::esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    EspHf s_self = EspHf::getInstance();
    s_self.cb_gap_param = param;

    switch (event)
    {
        // If nearby Bluetooth devices are discovered.
        case ESP_BT_GAP_DISC_RES_EVT:
        {
            s_self.gap_helper_device_discovered(param, &s_self.peer_addr, s_self.peer_bdname, &s_self.peer_bdname_len);
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
                ESP_LOG_BUFFER_HEX(BT_HF_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
            }
            else
            {
                ESP_LOGE(BT_HF_TAG, "authentication failed, status:%d",
                        param->auth_cmpl.stat);
            }
            break;
        }
        case ESP_BT_GAP_PIN_REQ_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d",
                    param->pin_req.min_16_digit);
            if (param->pin_req.min_16_digit)
            {
                ESP_LOGI(BT_HF_TAG, "Input pin code: 0000 0000 0000 0000");
                // esp_bt_pin_code_t pin_code = {0};
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, nullptr);
            }
            else
            {
                ESP_LOGI(BT_HF_TAG, "Input pin code: 1234");
                // esp_bt_pin_code_t pin_code;
                // pin_code[0] = '9';
                // pin_code[1] = '9';
                // pin_code[2] = '9';
                // pin_code[3] = '9';
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, nullptr);
            }
            break;
        }

    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(
            BT_HF_TAG,
            "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32,
            param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
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