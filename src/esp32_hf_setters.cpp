#include "esp32_hf.h"
#define BT_HF_TAG "BT_HF_GAP_SETTER"

bool EspHf::setDeviceName(const char *str) {
    if (!str || !str[0])
    {
        ESP_LOGE(BT_HF_TAG, "Device name cannot be nullpointer or empty.");
        return false;
    }

    this->cfg.bluetooth.device_name = str;
    return true;
}

bool EspHf::setLegacyPin(const char *str) {
    if (!str || !str[0])
    {
        ESP_LOGE(BT_HF_TAG, "Legacy pin cannot be nullpointer or empty.");
        return false;
    }
    if (strlen(str) > ESP_BT_PIN_CODE_LEN) {
        ESP_LOGE(BT_HF_TAG, "Legacy Pin cannot be bigger then %d", ESP_BT_PIN_CODE_LEN);
        return false;
    }
    cfg.bluetooth.legacy_pairing_pin = str;
    return true;
}


void EspHf::setIncomingCallCallback(void (*ptr)(esp_hf_client_cb_param_t *param)) {
    this->ptr = ptr;
}
void EspHf::disableSecurePairing(void) {
    this->cfg.bluetooth.ssp_enabled = false;
}