#include "esp32_hf_intern.hpp"
#include "esp_adc/adc_continuous.h"
#include "esp_hf_client_legacy_api.h"

#include "freertos/queue.h"

#define BT_HF_TAG "BT_HF_AUDIO"

static constexpr size_t HFP_CVSD_FRAME_BYTES = 60;
static constexpr size_t HFP_MSBC_FRAME_BYTES = 240;
static constexpr size_t HFP_MIC_QUEUE_DEPTH = 2;
static constexpr size_t MIC_ADC_FRAME_BYTES = 256;
static constexpr size_t MIC_ADC_SAMPLES_PER_READ = MIC_ADC_FRAME_BYTES / SOC_ADC_DIGI_RESULT_BYTES;

static QueueHandle_t mic_frames = nullptr;
static TaskHandle_t mic_task = nullptr;
static volatile bool mic_task_running = false;
static adc_continuous_handle_t mic_adc = nullptr;
static I2SClass* active_mic_i2s = nullptr;
static bool active_mic_uses_i2s = false;
static size_t active_mic_frame_bytes = HFP_MSBC_FRAME_BYTES;
static adc_continuous_data_t mic_samples[MIC_ADC_SAMPLES_PER_READ];

static void bt_app_hf_client_audio_close(void)
{
    mic_task_running = false;
    if (mic_adc)
        adc_continuous_stop(mic_adc);
    if (active_mic_i2s)
        active_mic_i2s->end();
    while (mic_task) {
        vTaskDelay(1);
    }
    if (mic_frames) {
        vQueueDelete(mic_frames);
        mic_frames = nullptr;
    }
    if (mic_adc) {
        adc_continuous_deinit(mic_adc);
        mic_adc = nullptr;
    }
    active_mic_i2s = nullptr;
    active_mic_uses_i2s = false;
}

static uint32_t bt_app_hf_client_outgoing_cb(uint8_t* p_buf, uint32_t sz)
{
    if (!p_buf || !mic_frames || sz != active_mic_frame_bytes)
        return 0;
    return xQueueReceive(mic_frames, p_buf, 0) == pdTRUE ? sz : 0;
}

static void microphone_capture_task(void* arg)
{
    (void)arg;
    int16_t frame[HFP_MSBC_FRAME_BYTES / sizeof(int16_t)];
    size_t frame_samples = active_mic_frame_bytes / sizeof(int16_t);
    size_t frame_position = 0;
    const size_t decimation = active_mic_frame_bytes == HFP_CVSD_FRAME_BYTES ? 4 : 2;
    size_t phase = 0;
    int32_t decimation_sum = 0;
    int32_t i2s_samples[HFP_MSBC_FRAME_BYTES / sizeof(int16_t)];

    while (mic_task_running) {
        if (active_mic_uses_i2s) {
            const size_t sample_count = frame_samples;
            const size_t bytes = active_mic_i2s->readBytes(
                reinterpret_cast<char*>(i2s_samples), sample_count * sizeof(int32_t));
            if (mic_task_running && bytes == sample_count * sizeof(int32_t)) {
                for (size_t i = 0; i < sample_count; ++i)
                    frame[i] = static_cast<int16_t>(i2s_samples[i] >> 16);
                if (xQueueSend(mic_frames, frame, 0) == pdTRUE)
                    esp_hf_client_outgoing_data_ready();
            }
            continue;
        }

        uint32_t sample_count = 0;
        const esp_err_t err = adc_continuous_read_parse(mic_adc, mic_samples,
            MIC_ADC_SAMPLES_PER_READ, &sample_count, 100);
        if (!mic_task_running)
            break;
        if (err != ESP_OK)
            continue;

        for (uint32_t i = 0; i < sample_count; ++i) {
            if (!mic_samples[i].valid)
                continue;
            // Remove the nominal 12-bit ADC midpoint and scale to PCM16.
            const int32_t scaled = (static_cast<int32_t>(mic_samples[i].raw_data) - 2048) * 16;
            const int16_t pcm = static_cast<int16_t>(scaled > 32767 ? 32767 : (scaled < -32768 ? -32768 : scaled));
            decimation_sum += pcm;
            if (++phase < decimation)
                continue;
            frame[frame_position++] = static_cast<int16_t>(decimation_sum / static_cast<int32_t>(decimation));
            phase = 0;
            decimation_sum = 0;
            if (frame_position == frame_samples) {
                if (xQueueSend(mic_frames, frame, 0) == pdTRUE)
                    esp_hf_client_outgoing_data_ready();
                frame_position = 0;
            }
        }
    }
    mic_task = nullptr;
    vTaskDelete(nullptr);
}

bool EspHf::Impl::start_microphone(uint32_t sample_rate)
{
    if (!microphone.configured)
        return true;
    if (mic_task || mic_frames)
        return true;

    const size_t frame_bytes = sample_rate == 8000 ? HFP_CVSD_FRAME_BYTES : HFP_MSBC_FRAME_BYTES;
    active_mic_frame_bytes = frame_bytes;
    mic_frames = xQueueCreate(HFP_MIC_QUEUE_DEPTH, frame_bytes);
    if (!mic_frames) {
        ESP_LOGE(BT_HF_TAG, "Could not allocate microphone frame queue");
        return false;
    }

    esp_err_t err = ESP_OK;
    if (microphone.use_i2s) {
        active_mic_i2s = &microphone.i2s;
        active_mic_uses_i2s = true;
        active_mic_i2s->setPins(microphone.bclk, microphone.ws, -1,
            microphone.din, microphone.mclk);
        if (!active_mic_i2s->begin(I2S_MODE_STD, sample_rate,
                I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO))
            err = ESP_FAIL;
    } else {
        adc_unit_t unit;
        adc_channel_t channel;
        if (adc_continuous_io_to_channel(microphone.adc_pin, &unit, &channel) != ESP_OK || unit != ADC_UNIT_1) {
            err = ESP_ERR_INVALID_ARG;
        } else {
            adc_continuous_handle_cfg_t handle_config = { };
            handle_config.max_store_buf_size = 1024;
            handle_config.conv_frame_size = MIC_ADC_FRAME_BYTES;
            err = adc_continuous_new_handle(&handle_config, &mic_adc);
            if (err == ESP_OK) {
                adc_digi_pattern_config_t pattern = { };
                pattern.atten = ADC_ATTEN_DB_12;
                pattern.channel = channel;
                pattern.unit = unit;
                pattern.bit_width = ADC_BITWIDTH_12;

                adc_continuous_config_t config = { };
                config.sample_freq_hz = 32000;
                config.conv_mode = ADC_CONV_SINGLE_UNIT_1;
                config.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
                config.pattern_num = 1;
                config.adc_pattern = &pattern;
                err = adc_continuous_config(mic_adc, &config);
            }
            if (err == ESP_OK)
                err = adc_continuous_start(mic_adc);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "Could not start microphone capture: %s", esp_err_to_name(err));
        if (mic_adc) {
            adc_continuous_deinit(mic_adc);
            mic_adc = nullptr;
        }
        if (active_mic_i2s) {
            active_mic_i2s->end();
            active_mic_i2s = nullptr;
        }
        active_mic_uses_i2s = false;
        vQueueDelete(mic_frames);
        mic_frames = nullptr;
        return false;
    }

    mic_task_running = true;
    if (xTaskCreate(microphone_capture_task, "hfp_mic", 6144, nullptr, 3, &mic_task) != pdPASS) {
        mic_task_running = false;
        if (mic_adc) {
            adc_continuous_stop(mic_adc);
            adc_continuous_deinit(mic_adc);
            mic_adc = nullptr;
        }
        if (active_mic_i2s) {
            active_mic_i2s->end();
            active_mic_i2s = nullptr;
        }
        active_mic_uses_i2s = false;
        vQueueDelete(mic_frames);
        mic_frames = nullptr;
        ESP_LOGE(BT_HF_TAG, "Could not create microphone capture task");
        return false;
    }
    return true;
}

void EspHf::Impl::bt_app_hf_client_incoming_cb(const uint8_t* buf, uint32_t sz)
{
    const int16_t* in = reinterpret_cast<const int16_t*>(buf);
    const size_t count = sz / sizeof(int16_t);

    // volume 0..15 -> gain 0.0..1.0 (squared so it feels natural)
    const float v = _info.volume / 15.0f;
    const float gain = v * v;

    int16_t out[256]; // HFP packets are small (60 or 240 bytes)
    const size_t n = count < 256 ? count : 256;

    for (size_t i = 0; i < n; i++) {
        out[i] = static_cast<int16_t>(in[i] * gain);
    }

    speaker.p_speaker->write(reinterpret_cast<uint8_t*>(out), n * sizeof(int16_t));
}

void EspHf::Impl::bt_app_hf_client_incoming_cb_wrapper(const uint8_t* buf, uint32_t sz)
{
    getInstance()._impl->bt_app_hf_client_incoming_cb(buf, sz);
}

void EspHf::Impl::audio_change(esp_hf_client_cb_param_t* param)
{
    if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED || param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) {
        esp_hf_client_register_data_callback(speaker.p_speaker ? bt_app_hf_client_incoming_cb_wrapper : nullptr, bt_app_hf_client_outgoing_cb);

        const uint32_t rate = param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC ? 16000 : 8000;
        if (!start_microphone(rate))
            ESP_LOGE(BT_HF_TAG, "HFP audio connected without microphone capture");
        _info.state = IN_CALL;
    } else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED) {
        _info.state = IDLE_CONNECTED;
        bt_app_hf_client_audio_close();
    }
}

// The volume is stored in flash (NVS) so it is known again after a reboot.
#define VOLUME_NVS_NAMESPACE "esp32_hf"
#define VOLUME_NVS_KEY "volume"

void EspHf::Impl::load_volume(void)
{
    nvs_handle_t handle;
    uint8_t saved;

    if (nvs_open(VOLUME_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return;
    if (nvs_get_u8(handle, VOLUME_NVS_KEY, &saved) == ESP_OK && saved <= 15)
        _info.volume = saved;
    nvs_close(handle);
}

void EspHf::Impl::save_volume(int new_volume)
{
    if (new_volume < 0 || new_volume > 15 || new_volume == _info.volume)
        return;
    _info.volume = new_volume;

    // Only written when the volume actually changed, to limit flash wear.
    nvs_handle_t handle;
    if (nvs_open(VOLUME_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return;
    nvs_set_u8(handle, VOLUME_NVS_KEY, static_cast<uint8_t>(new_volume));
    nvs_commit(handle);
    nvs_close(handle);
}
