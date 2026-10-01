#include "esp32_hf_intern.hpp"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "Arduino.h"

#undef BT_HF_TAG
#define BT_HF_TAG "BT_HF_GAP"

void test();
typedef enum {
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;


typedef struct {
    bool dev_found;
    uint8_t bdname_len;
    uint8_t eir_len;
    uint8_t rssi;
    uint32_t cod;
    uint8_t eir[ESP_BT_GAP_EIR_DATA_LEN];
    uint8_t bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    esp_bd_addr_t bda;
    app_gap_state_t state;
} app_gap_cb_t;

static app_gap_cb_t m_dev_info;

static void update_device_info(esp_bt_gap_cb_param_t *param);
// static bool get_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len) {
//     uint8_t *rmt_bdname = NULL;
//     uint8_t rmt_bdname_len = 0;

//     if (!eir)
//         return false;

//     rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
//     if (rmt_bdname)
//     {
//         if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN)
//             rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;

//         if (bdname)
//         {
//             memcpy(bdname, rmt_bdname, rmt_bdname_len);
//             bdname[rmt_bdname_len] = '\0';
//         }
//         if (bdname_len)
//             *bdname_len = rmt_bdname_len;
//         return true;
//     }
//     return false;
// }


static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
}

// void EspHf::Impl::gap_helper_device_discovered(esp_bt_gap_cb_param_t *param, esp_bd_addr_t peer_addr, char *peer_bdname, uint8_t *peer_bdname_len) {
//     for (int i = 0; i < param->disc_res.num_prop; i++)
//     {
//         if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR &&
//             get_name_from_eir((uint8_t *)param->disc_res.prop[i].val, peer_bdname, peer_bdname_len))
//         {
//             // Copy bluetooth address into peer_addr.
//             memcpy(peer_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);

//             // Connect to new AG host.
//             if (esp_hf_client_connect(peer_addr) != ESP_OK)
//                 ESP_LOGE(BT_HF_TAG, "Could not connect to device: %s", peer_bdname);
//             else
//             {
//                 ESP_LOGI(BT_HF_TAG, "Connected to device: %s", peer_bdname);
//                 esp_bt_gap_cancel_discovery();
//                 break;
//             }
//         }
//     }
// }

bool EspHf::Impl::setup_bt_gap(const char *device_name, bool ssp_enabled, const char* legacy_pairing_pin) {
    // Set up device name.
    ERROR_CHECK(esp_bt_gap_set_device_name(device_name), "Could not set device name.")
    

    // Register GAP callback function. (The function that will handle conenction and discovery).
    ERROR_CHECK(esp_bt_gap_register_callback(Impl::esp_bt_gap_cb_wrapper), "Could not register callback.")

    if (ssp_enabled)
    {
        // Set default parameters for Secure Simple Pairing.
        esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
        esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(uint8_t));
    }
    // Set pincode, and activate callback calls for auth.
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, strlen(legacy_pairing_pin), (uint8_t *)legacy_pairing_pin);
    
    esp_bt_cod_t cod = {0};
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;
    cod.minor = 0x01;
    cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_TELEPHONY;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);
    // Set discoverable and connectable mode, wait to be connected.
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    return true;
}

void EspHf::Impl::auth_success_check(esp_bt_gap_cb_param_t *param) {
    if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGI(BT_HF_TAG, "authentication success: %s",
            param->auth_cmpl.device_name);
        esp_bt_gap_cancel_discovery();
        if (callbacks.cb_bt_pin_correct)
            xTaskCreatePinnedToCore(wrap, nullptr, 4096, (void *)callbacks.cb_bt_pin_correct, 1, nullptr, 1);
    }
    else
    {
        ESP_LOGE(BT_HF_TAG, "authentication failed, status: %d",
            param->auth_cmpl.stat);
        if (callbacks.cb_bt_pin_incorrect)
            xTaskCreatePinnedToCore(wrap, nullptr, 4096, (void *)callbacks.cb_bt_pin_incorrect, 1, nullptr, 1);
    }
}

void EspHf::Impl::ssp_auth(esp_bd_addr_t bda, uint32_t num_val) {
    static t_auth a;
    
    ESP_LOGI(BT_HF_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32, num_val);
    esp_bt_gap_ssp_confirm_reply(bda, true);
    
    if (!callbacks.cb_bt_ssp_auth)
        return;
    snprintf(a.auth_number, sizeof(a.auth_number), "%06" PRIu32, num_val);
    // itoa(num_val, a.auth_number, 6);
    a.fn = callbacks.cb_bt_ssp_auth;
    
    xTaskCreatePinnedToCore(ssp_auth_wrap, nullptr, 4096, (void *)&a, 1, nullptr, 1);
}

void EspHf::Impl::esp_bt_gap_cb_wrapper(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    getInstance()._impl->esp_bt_gap_cb(event, param);
}

void EspHf::Impl::esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event)
    {
        case ESP_BT_GAP_DISC_RES_EVT:
            // gap_helper_device_discovered(param, peer_addr, peer_bdname, &peer_bdname_len); break;
            // update_device_info(param); break;
            test(); break;

        case ESP_BT_GAP_AUTH_CMPL_EVT:
            auth_success_check(param); break;

        case ESP_BT_GAP_PIN_REQ_EVT:
        {
            esp_bt_gap_pin_reply(param->pin_req.bda, true, strlen(bt.legacy_pairing_pin), (uint8_t *)bt.legacy_pairing_pin);
            if (callbacks.cb_bt_legacy_pin)
                xTaskCreatePinnedToCore(wrap, nullptr, 4096, (void *)callbacks.cb_bt_legacy_pin, 1, nullptr, 1);
            break;
        }

        case ESP_BT_GAP_CFM_REQ_EVT:
            ssp_auth(param->cfm_req.bda, param->cfm_req.num_val); break;
    }
}

static void update_device_info(esp_bt_gap_cb_param_t *param)
{
    char bda_str[18];
    uint32_t cod = 0;
    int32_t rssi = -129; /* invalid value */
    uint8_t *bdname = NULL;
    uint8_t bdname_len = 0;
    uint8_t *eir = NULL;
    int eir_len = 0;
    esp_bt_gap_dev_prop_t *p;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            cod = *(uint32_t *)(p->val);
            ESP_LOGI(BT_HF_TAG, "--Class of Device: 0x%"PRIx32, cod);
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t *)(p->val);
            ESP_LOGI(BT_HF_TAG, "--RSSI: %"PRId32, rssi);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME:
            bdname_len = (p->len > ESP_BT_GAP_MAX_BDNAME_LEN) ? ESP_BT_GAP_MAX_BDNAME_LEN :
                          (uint8_t)p->len;
            bdname = (uint8_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_EIR: {
            eir_len = p->len;
            eir = (uint8_t *)(p->val);
            break;
        }
        default:
            break;
        }
    }

    /* search for device with Major device type "PHONE" or "Audio/Video" in COD */
    app_gap_cb_t *p_dev = &m_dev_info;
    if (p_dev->dev_found) {
        ESP_LOGI(BT_HF_TAG, "Found device on dev found");
        return;
    }

    if (!esp_bt_gap_is_valid_cod(cod) ||
	    (!(esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_PHONE) &&
             !(esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_AV))) {
        ESP_LOGI(BT_HF_TAG, "Found device By identifiying cod major dev phone");

        return;
    }

    memcpy(p_dev->bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
    p_dev->dev_found = true;

    p_dev->cod = cod;
    p_dev->rssi = rssi;
    if (bdname_len > 0) {
        memcpy(p_dev->bdname, bdname, bdname_len);
        p_dev->bdname[bdname_len] = '\0';
        p_dev->bdname_len = bdname_len;
    }
    if (eir_len > 0) {
        uint8_t copy_len = (eir_len > ESP_BT_GAP_EIR_DATA_LEN) ? ESP_BT_GAP_EIR_DATA_LEN : eir_len;
        memcpy(p_dev->eir, eir, copy_len);
        p_dev->eir_len = copy_len;
    }

    if (p_dev->bdname_len == 0) {
        get_name_from_eir(p_dev->eir, p_dev->bdname, &p_dev->bdname_len);
    }

    ESP_LOGI(BT_HF_TAG, "Found a target device, address %s, name %s", bda_str, p_dev->bdname);
    p_dev->state = APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE;
    ESP_LOGI(BT_HF_TAG, "Cancel device discovery ...");
    // esp_bt_gap_cancel_discovery();
}

void test() {
    int i = esp_bt_gap_get_bond_device_num();
    esp_bd_addr_t peer_addrs;
    if (i > 0)
    {
        esp_bt_gap_get_bond_device_list(&i, &peer_addrs);
        ESP_LOGI("SOMETHING SOMETHING", "dev_num: %d", i);

        for (int x = 0; x < 6; x++)
        {
            ESP_LOGI("t", "%d", peer_addrs[x]);
        }
        esp_hf_client_connect(peer_addrs);
    }

}