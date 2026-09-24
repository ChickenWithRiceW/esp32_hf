#include "esp32_hf.hpp"
#define BT_HF_TAG "BT_HF_GAP_HELPER"

extern "C"
{
    // Writes bluetooth devices name into bdname and bluetooth device name len into bdname len pointer. Returns bool showing success or failure.
    static bool get_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len)
    {
        uint8_t *rmt_bdname = NULL;
        uint8_t rmt_bdname_len = 0;

        if (!eir)
            return false;

        // ? I guess it gets tried two times?
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);

        if (!rmt_bdname)
            rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);

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