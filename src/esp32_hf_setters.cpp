#include "esp32_hf.h"
#define BT_HF_TAG "BT_HF_GAP_SETTER"

bool EspHf::setDeviceName(const char *str) {
    ASSERT_NOT_INIT
    if (!str || !str[0])
    {
        ESP_LOGE(BT_HF_TAG, "Device name can't be nullpointer or empty.");
        return false;
    }

    this->cfg.bluetooth.device_name = str;
    return true;
}

bool EspHf::setLegacyPin(const char *str) {
    ASSERT_NOT_INIT
    if (!str || !str[0])
    {
        ESP_LOGE(BT_HF_TAG, "Legacy pin can't be nullpointer or empty.");
        return false;
    }
    if (strlen(str) > ESP_BT_PIN_CODE_LEN) {
        ESP_LOGE(BT_HF_TAG, "Legacy Pin cannot be bigger then %d", ESP_BT_PIN_CODE_LEN);
        return false;
    }
    // ! Check if input is alphanumeric
    cfg.bluetooth.legacy_pairing_pin = str;
    return true;
}

bool EspHf::disableSecurePairing(void) {
    ASSERT_NOT_INIT
    this->cfg.bluetooth.ssp_enabled = false;
    return true;
}






// Callbacks
bool EspHf::setCallbackIncomingCall(void (*ptr)(void)) {
    this->ptr = ptr;
    return true;
}
