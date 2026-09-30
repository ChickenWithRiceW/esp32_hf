#include "esp32_hf_intern.hpp"

extern "C" {
#include "freertos/ringbuf.h"
}

#define ESP_HFP_RINGBUF_SIZE 3600
static RingbufHandle_t m_rb = NULL;

static void bt_app_hf_client_audio_open(void)
{
    m_rb = xRingbufferCreate(ESP_HFP_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
}

static void bt_app_hf_client_audio_close(void)
{
    if (!m_rb) {
        return ;
    }

    vRingbufferDelete(m_rb);
    m_rb = NULL;
}

static uint32_t bt_app_hf_client_outgoing_cb(uint8_t *p_buf, uint32_t sz)
{
    // if (!m_rb) {
    //     return 0;
    // }

    // size_t item_size = 0;
    // uint8_t *data = xRingbufferReceiveUpTo(m_rb, &item_size, 0, sz);
    // if (item_size == sz) {
    //     memcpy(p_buf, data, item_size);
    //     vRingbufferReturnItem(m_rb, data);
    //     return sz;
    // } else if (0 < item_size) {
    //     vRingbufferReturnItem(m_rb, data);
    //     return 0;
    // } else {
    //     // data not enough, do not read
    //     return 0;
    // }
    return 0;
}

void EspHf::Impl::bt_app_hf_client_incoming_cb(const uint8_t *buf, uint32_t sz) {
    // if (!this->speaker)
    // {
    //     ESP_LOGE(BT_HF_TAG, "Speaker object is not set.");
    //     return ;
    // }
    // EspHf s_self = EspHf::getInstance();
    // s_self.speaker.speaker.write(buf, sz);
    speaker.speaker.write(buf, sz);
}

void EspHf::Impl::bt_app_hf_client_incoming_cb_wrapper(const uint8_t *buf, uint32_t sz)
{
    if (! m_rb) {
        return;
    }
    getInstance()._impl->bt_app_hf_client_incoming_cb(buf, sz);

    // esp_hf_client_outgoing_data_ready();
}

void EspHf::Impl::audio_change(esp_hf_client_cb_param_t *param) {
    // if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED ||
    //     param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) 
    // {
    //     esp_hf_client_register_data_callback(bt_app_hf_client_incoming_cb_wrapper, bt_app_hf_client_outgoing_cb);
    //     bt_app_hf_client_audio_open();
    // }
    // else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED)
    //     bt_app_hf_client_audio_close();
    ESP_LOGI("DONT NEED", "STILL NO NEED");
}