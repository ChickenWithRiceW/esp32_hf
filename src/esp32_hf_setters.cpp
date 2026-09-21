#include "esp32_hf.h"
#define BT_HF_TAG "BT_HF_GAP_SETTER"

bool EspHf::setDeviceName(const char *str) {
    if (!str || !str[0])
    {
        ESP_LOGE(BT_HF_TAG, "Device name can't be nullpointer or empty.");
        return false;
    }
    if (!EspHf::isStrPrintable(str))
    {
        ESP_LOGE(BT_HF_TAG, "Device name must only contain printable characters.");
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
    if (!EspHf::isStrPrintable(str))
    {
        ESP_LOGE(BT_HF_TAG, "Legacy pin must only contain printable characters.");
        return false;
    }
    cfg.bluetooth.legacy_pairing_pin = str;
    return true;
}

bool EspHf::setSpeakerI2s(I2SClass *speaker) {
    ASSERT_NOT_INIT
    if (!speaker)
    {
        ESP_LOGE(BT_HF_TAG, "Cannot pass in nullpointer.");
        return false;
    }
    this->p_speaker = speaker;
    return true;
}

I2SClass *EspHf::setSpeaker(int8_t bclk, int8_t ws, int8_t dout, int8_t din, int8_t mclk) {
    this->speaker.setPins(bclk, ws, dout, din, mclk);
    if (!this->speaker.begin(I2S_MODE_STD, MSBC_BITRATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        ESP_LOGE(BT_HF_TAG, "I2s speaker init failed");
        return NULL;
    }
    return &this->speaker;
}

bool EspHf::disableSecurePairing(void) {
    ASSERT_NOT_INIT
    this->cfg.bluetooth.ssp_enabled = false;
    return true;
}






// Callbacks
bool EspHf::setCbIncomingCall(void (*ptr)(const char *phone_number, bool sim_call)) {
    this->cb_incoming_call = ptr;
    return true;
}
