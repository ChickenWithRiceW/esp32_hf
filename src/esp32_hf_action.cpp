#include "esp32_hf.hpp"
#include "esp32_hf_intern.hpp"
#include "esp_hf_client_api.h"

#define BT_HF_TAG "BT_HF_GAP_SETTER"

bool EspHf::acceptIncomingCall(void)
{
    ASSERT_INIT
    return (esp_hf_client_answer_call() == 0);
}

bool EspHf::rejectIncomingCall(void)
{
    ASSERT_INIT
    return (esp_hf_client_reject_call() == 0);
}

bool EspHf::callNumber(const char* phone_number)
{
    ASSERT_INIT
    if (!phone_number || !phone_number[0])
        return false;
    return (esp_hf_client_dial(phone_number) == 0);
}
bool EspHf::endCall(void)
{
    return rejectIncomingCall();
}

bool EspHf::redial(void)
{
    ASSERT_INIT
    // A null number makes the stack send the "call last number" command.
    return (esp_hf_client_dial(nullptr) == ESP_OK);
}

bool EspHf::connectAudio(void)
{
    ASSERT_INIT
    if (!_impl->_info.connected)
        return false;
    return (esp_hf_client_connect_audio(_impl->peer_addr) == ESP_OK);
}

bool EspHf::disconnectAudio(void)
{
    ASSERT_INIT
    if (!_impl->_info.connected)
        return false;
    return (esp_hf_client_disconnect_audio(_impl->peer_addr) == ESP_OK);
}

bool EspHf::setVolume(int volume)
{
    ASSERT_INIT
    if (volume < 0 || volume > VOLUME_MAX) {
        ESP_LOGE(BT_HF_TAG, "Volume must be between 0 and %d.", VOLUME_MAX);
        return false;
    }
    _impl->save_volume(volume);
    if (!_impl->_info.connected)
        return true;
    return (esp_hf_client_volume_update(ESP_HF_VOLUME_CONTROL_TARGET_SPK, volume) == ESP_OK);
}

bool EspHf::setMicMute(bool mute)
{
    _impl->set_mic_mute(mute);
    return true;
}
