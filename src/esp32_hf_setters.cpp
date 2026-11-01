#include "esp32_hf_intern.hpp"
#include "esp_gap_bt_api.h"
#include <cstring>

#define BT_HF_TAG "BT_HF_GAP_SETTER"

bool EspHf::setDeviceName(const char* device_name)
{
    ASSERT_NOT_INIT
    if (!device_name || !device_name[0]) {
        ESP_LOGE(BT_HF_TAG, "Device name can't be nullpointer or empty.");
        return false;
    }
    if (!Impl::isStrPrintable(device_name)) {
        ESP_LOGE(BT_HF_TAG, "Device name must only contain printable characters.");
        return false;
    }
    if (strlen(device_name) > ESP_BT_GAP_MAX_BDNAME_LEN) {
        ESP_LOGE(BT_HF_TAG, "Device name [%d] is to long. Max len is [%d]", strlen(device_name), ESP_BT_GAP_MAX_BDNAME_LEN);
        return false;
    }

    // Copy, so the string can go out of scope after this call.
    strcpy(_impl->bt.device_name, device_name);
    return true;
}

bool EspHf::setLegacyPin(const char* str)
{
    ASSERT_NOT_INIT
    if (!str || !str[0]) {
        ESP_LOGE(BT_HF_TAG, "Legacy pin can't be nullpointer or empty.");
        return false;
    }
    if (strlen(str) > ESP_BT_PIN_CODE_LEN) {
        ESP_LOGE(BT_HF_TAG, "Legacy Pin cannot be bigger then %d", ESP_BT_PIN_CODE_LEN);
        return false;
    }
    if (!EspHf::Impl::isStrPrintable(str)) {
        ESP_LOGE(BT_HF_TAG, "Legacy pin must only contain printable characters.");
        return false;
    }
    strcpy(_impl->bt.legacy_pairing_pin, str);
    return true;
}

bool EspHf::setSpeakerI2s(I2SClass* speaker)
{
    ASSERT_NOT_INIT
    if (!speaker) {
        ESP_LOGE(BT_HF_TAG, "Cannot pass in nullpointer.");
        return false;
    }
    if (_impl->microphone.configured && speaker->getPort() == I2S_NUM_0) {
        ESP_LOGE(BT_HF_TAG, "Microphone capture uses I2S0; configure speaker playback on I2S1.");
        return false;
    }
    _impl->speaker.p_speaker = speaker;
    return true;
}

I2SClass* EspHf::setupSpeaker(int8_t bclk, int8_t ws, int8_t dout, int8_t din, int8_t mclk)
{
    if (_impl->initialised) {
        ESP_LOGE(BT_HF_TAG, "Can't call function if begin was called earlier.");
        return NULL;
    }
    _impl->speaker.speaker.setPins(bclk, ws, dout, din, mclk);
    if (!_impl->speaker.speaker.begin(I2S_MODE_STD, MSBC_BITRATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        ESP_LOGE(BT_HF_TAG, "I2s speaker init failed");
        return NULL;
    }
    // Internally we just use to pointer so its more handy.
    _impl->speaker.p_speaker = &_impl->speaker.speaker;
    return &_impl->speaker.speaker;
}

bool EspHf::setMicrophoneAdcPin(uint8_t pin)
{
    ASSERT_NOT_INIT
    if (pin < 32 || pin > 39) {
        ESP_LOGE(BT_HF_TAG, "On the original ESP32, microphone ADC must use an ADC1 pin (GPIO 32-39).");
        return false;
    }
    if (_impl->speaker.p_speaker && _impl->speaker.p_speaker->getPort() == I2S_NUM_0) {
        ESP_LOGE(BT_HF_TAG, "Microphone capture uses I2S0; configure speaker playback on I2S1.");
        return false;
    }
    _impl->microphone.adc_pin = pin;
    _impl->microphone.configured = true;
    _impl->microphone.use_i2s = false;
    return true;
}

bool EspHf::setMicrophoneI2s(int8_t bclk, int8_t ws, int8_t din, int8_t mclk)
{
    ASSERT_NOT_INIT
    if (bclk < 0 || ws < 0 || din < 0) {
        ESP_LOGE(BT_HF_TAG, "I2S microphone requires BCLK, WS, and DIN pins.");
        return false;
    }
    if (_impl->speaker.p_speaker && _impl->speaker.p_speaker->getPort() == I2S_NUM_0) {
        ESP_LOGE(BT_HF_TAG, "Microphone capture uses I2S0; configure speaker playback on I2S1.");
        return false;
    }
    _impl->microphone.bclk = bclk;
    _impl->microphone.ws = ws;
    _impl->microphone.din = din;
    _impl->microphone.mclk = mclk;
    _impl->microphone.configured = true;
    _impl->microphone.use_i2s = true;
    return true;
}

bool EspHf::setSecurePairing(bool enable_ssp)
{
    ASSERT_NOT_INIT
    _impl->bt.ssp_enabled = enable_ssp;
    return true;
}

// --- Callbacks

bool EspHf::setCbIncomingCall(void (*ptr)(char phone_number[15], bool sim_call))
{
    if (!ptr) {
        ESP_LOGE(BT_HF_TAG, "Incoming call callback can't be nullptr.");
        return false;
    }
    _impl->callbacks.cb_incoming_call = ptr;
    return true;
}

bool EspHf::setCbOutgoingCall(void (*ptr)(char phone_number[15], bool sim_call))
{
    if (!ptr) {
        ESP_LOGE(BT_HF_TAG, "Outgoing call callback can't be nullptr.");
        return false;
    }
    _impl->callbacks.cb_outgoing_call = ptr;
    return true;
}

bool EspHf::setCbPinCorrect(void (*ptr)(void))
{
    if (!ptr) {
        ESP_LOGE(BT_HF_TAG, "Correct pin callback can't be nullptr.");
        return false;
    }
    _impl->callbacks.cb_bt_pin_correct = ptr;
    return true;
}

bool EspHf::setCbPinIncorrect(void (*ptr)(void))
{
    if (!ptr) {
        ESP_LOGE(BT_HF_TAG, "Incorrect pin callback can't be nullptr.");
        return false;
    }
    _impl->callbacks.cb_bt_pin_incorrect = ptr;
    return true;
}

bool EspHf::setCbSspAuth(void (*ptr)(char[6]))
{
    if (!ptr) {
        ESP_LOGE(BT_HF_TAG, "SSP auth callback can't be nullptr.");
        return false;
    }
    _impl->callbacks.cb_bt_ssp_auth = ptr;
    return true;
}

bool EspHf::setCbLegacyAuth(void (*ptr)(void))
{
    if (!ptr) {
        ESP_LOGE(BT_HF_TAG, "Legacy auth callback can't be nullptr.");
        return false;
    }
    _impl->callbacks.cb_bt_legacy_pin = ptr;
    return true;
}

bool EspHf::setCbBtConnected(void (*ptr)(void))
{
    if (!ptr) {
        ESP_LOGE(BT_HF_TAG, "Bluetooth connected callback can't be nullptr.");
        return false;
    }
    _impl->callbacks.cb_bt_connected = ptr;
    return true;
}

bool EspHf::setCbBtDisconnected(void (*ptr)(void))
{
    if (!ptr) {
        ESP_LOGE(BT_HF_TAG, "Bluetooth disconnected callback can't be nullptr.");
        return false;
    }
    _impl->callbacks.cb_bt_disconnected = ptr;
    return true;
}
