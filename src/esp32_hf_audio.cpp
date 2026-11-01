#include "esp32_hf_intern.hpp"
#include "esp_adc/adc_continuous.h"
#include "esp_hf_client_legacy_api.h"

#include "freertos/queue.h"

#define BT_HF_TAG "BT_HF_AUDIO"

// Call audio is 16-bit mono and arrives in fixed size frames:
// 8 kHz (CVSD, narrow-band) or 16 kHz (mSBC, wide-band).
static constexpr uint32_t CVSD_RATE = 8000;
static constexpr size_t CVSD_FRAME_BYTES = 60;
static constexpr size_t MSBC_FRAME_BYTES = 240;

// Microphone frames waiting to be picked up by the Bluetooth stack.
static constexpr size_t MIC_QUEUE_FRAMES = 2;
static constexpr uint32_t MIC_TASK_STACK = 6144;
static constexpr UBaseType_t MIC_TASK_PRIORITY = 3;

// The ADC samples at 32 kHz, which divides evenly into both call rates
// (and is above the ESP32 ADC minimum of 20 kHz).
static constexpr uint32_t ADC_SAMPLE_RATE = 32000;
static constexpr size_t ADC_READ_BYTES = 256;
static constexpr size_t ADC_READ_SAMPLES = ADC_READ_BYTES / SOC_ADC_DIGI_RESULT_BYTES;
static constexpr int32_t ADC_MIDPOINT = 2048; // Middle of the 12-bit range = silence.
static constexpr int32_t ADC_TO_PCM16 = 16; // 12-bit to 16-bit scaling.

// Largest speaker packet handled at once, in samples.
static constexpr size_t SPEAKER_MAX_SAMPLES = 256;

static struct {
    QueueHandle_t frames = nullptr; // Finished frames, filled by the task.
    TaskHandle_t task = nullptr; // Capture task, null when not running.
    volatile bool task_running = false;
    size_t frame_bytes = MSBC_FRAME_BYTES; // Frame size of the current call.
    uint32_t rate = MSBC_RATE; // Sample rate of the current call.
    bool use_i2s = false; // true: I2S microphone, false: ADC.
    volatile bool muted = false; // Send silence instead of the recording.
    I2SClass* i2s = nullptr;
    adc_continuous_handle_t adc = nullptr;
    adc_continuous_data_t adc_samples[ADC_READ_SAMPLES];
} mic;

// Hand a finished frame to the Bluetooth stack. A full queue drops the frame.
static void queue_mic_frame(int16_t* frame)
{
    if (mic.muted)
        memset(frame, 0, mic.frame_bytes);
    if (xQueueSend(mic.frames, frame, 0) == pdTRUE)
        esp_hf_client_outgoing_data_ready();
}

// Called by the Bluetooth stack when it wants microphone audio. Returning 0 means no data.
static uint32_t bt_app_hf_client_outgoing_cb(uint8_t* p_buf, uint32_t sz)
{
    if (!p_buf || !mic.frames || sz != mic.frame_bytes)
        return 0;
    return xQueueReceive(mic.frames, p_buf, 0) == pdTRUE ? sz : 0;
}

static void microphone_capture_task(void* arg)
{
    (void)arg;
    int16_t frame[MSBC_FRAME_BYTES / sizeof(int16_t)];
    const size_t frame_samples = mic.frame_bytes / sizeof(int16_t);

    // ADC only: the ADC runs faster than the call, so average groups of samples.
    const size_t decimation = ADC_SAMPLE_RATE / mic.rate;
    size_t frame_position = 0;
    size_t phase = 0;
    int32_t decimation_sum = 0;

    // I2S only: raw 32-bit samples before they are reduced to 16 bit.
    int32_t i2s_samples[MSBC_FRAME_BYTES / sizeof(int16_t)];

    while (mic.task_running) {
        if (mic.use_i2s) {
            // Read one whole frame, then keep the upper 16 bits of each sample.
            const size_t bytes_wanted = frame_samples * sizeof(int32_t);
            const size_t bytes = mic.i2s->readBytes(reinterpret_cast<char*>(i2s_samples), bytes_wanted);
            if (mic.task_running && bytes == bytes_wanted) {
                for (size_t i = 0; i < frame_samples; ++i)
                    frame[i] = static_cast<int16_t>(i2s_samples[i] >> 16);
                queue_mic_frame(frame);
            }
            continue;
        }

        uint32_t sample_count = 0;
        const esp_err_t err = adc_continuous_read_parse(mic.adc, mic.adc_samples,
            ADC_READ_SAMPLES, &sample_count, 100);
        if (!mic.task_running)
            break;
        if (err != ESP_OK)
            continue;

        for (uint32_t i = 0; i < sample_count; ++i) {
            if (!mic.adc_samples[i].valid)
                continue;
            // Center around zero and scale to 16 bit.
            const int32_t scaled = (static_cast<int32_t>(mic.adc_samples[i].raw_data) - ADC_MIDPOINT) * ADC_TO_PCM16;
            const int16_t pcm = static_cast<int16_t>(scaled > INT16_MAX ? INT16_MAX : (scaled < INT16_MIN ? INT16_MIN : scaled));
            decimation_sum += pcm;
            if (++phase < decimation)
                continue;
            frame[frame_position++] = static_cast<int16_t>(decimation_sum / static_cast<int32_t>(decimation));
            phase = 0;
            decimation_sum = 0;
            if (frame_position == frame_samples) {
                queue_mic_frame(frame);
                frame_position = 0;
            }
        }
    }
    mic.task = nullptr;
    vTaskDelete(nullptr);
}

// Free everything the microphone uses. Safe to call at any point, also when it
// was only partly started.
static void release_microphone(void)
{
    if (mic.adc) {
        adc_continuous_stop(mic.adc);
        adc_continuous_deinit(mic.adc);
        mic.adc = nullptr;
    }
    if (mic.i2s) {
        mic.i2s->end();
        mic.i2s = nullptr;
    }
    if (mic.frames) {
        vQueueDelete(mic.frames);
        mic.frames = nullptr;
    }
    mic.use_i2s = false;
}

// Stop recording when call audio ends.
static void stop_microphone(void)
{
    mic.task_running = false;
    // Stop the sources first so a read that is waiting in the task returns.
    if (mic.adc)
        adc_continuous_stop(mic.adc);
    if (mic.i2s)
        mic.i2s->end();
    while (mic.task)
        vTaskDelay(1);
    release_microphone();
}

static esp_err_t start_adc_microphone(uint8_t pin)
{
    adc_unit_t unit;
    adc_channel_t channel;
    if (adc_continuous_io_to_channel(pin, &unit, &channel) != ESP_OK || unit != ADC_UNIT_1)
        return ESP_ERR_INVALID_ARG;

    adc_continuous_handle_cfg_t handle_config = { };
    handle_config.max_store_buf_size = 1024;
    handle_config.conv_frame_size = ADC_READ_BYTES;
    esp_err_t err = adc_continuous_new_handle(&handle_config, &mic.adc);
    if (err != ESP_OK)
        return err;

    adc_digi_pattern_config_t pattern = { };
    pattern.atten = ADC_ATTEN_DB_12;
    pattern.channel = channel;
    pattern.unit = unit;
    pattern.bit_width = ADC_BITWIDTH_12;

    adc_continuous_config_t config = { };
    config.sample_freq_hz = ADC_SAMPLE_RATE;
    config.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    config.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
    config.pattern_num = 1;
    config.adc_pattern = &pattern;
    err = adc_continuous_config(mic.adc, &config);
    if (err != ESP_OK)
        return err;
    return adc_continuous_start(mic.adc);
}

// Start recording for a call with the given sample rate (8000 or 16000).
// Returns true if no microphone is configured, since there is nothing to start.
bool EspHf::Impl::start_microphone(uint32_t sample_rate)
{
    if (!microphone.configured)
        return true;
    if (mic.task || mic.frames)
        return true; // Already running.

    mic.rate = sample_rate;
    mic.frame_bytes = (sample_rate == CVSD_RATE) ? CVSD_FRAME_BYTES : MSBC_FRAME_BYTES;
    mic.frames = xQueueCreate(MIC_QUEUE_FRAMES, mic.frame_bytes);
    if (!mic.frames) {
        ESP_LOGE(BT_HF_TAG, "Could not allocate microphone frame queue");
        return false;
    }

    esp_err_t err;
    if (microphone.use_i2s) {
        mic.use_i2s = true;
        mic.i2s = &microphone.i2s;
        mic.i2s->setPins(microphone.bclk, microphone.ws, -1, microphone.din, microphone.mclk);
        err = mic.i2s->begin(I2S_MODE_STD, sample_rate, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO) ? ESP_OK : ESP_FAIL;
    } else {
        err = start_adc_microphone(microphone.adc_pin);
    }
    if (err != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "Could not start microphone capture: %s", esp_err_to_name(err));
        release_microphone();
        return false;
    }

    mic.task_running = true;
    if (xTaskCreate(microphone_capture_task, "hfp_mic", MIC_TASK_STACK, nullptr, MIC_TASK_PRIORITY, &mic.task) != pdPASS) {
        ESP_LOGE(BT_HF_TAG, "Could not create microphone capture task");
        mic.task_running = false;
        release_microphone();
        return false;
    }
    return true;
}


// Called by the Bluetooth stack with audio from the phone. Applies the volume and plays it.
void EspHf::Impl::bt_app_hf_client_incoming_cb(const uint8_t* buf, uint32_t sz)
{
    const int16_t* in = reinterpret_cast<const int16_t*>(buf);
    const size_t count = sz / sizeof(int16_t);

    // volume 0..VOLUME_MAX -> gain 0.0..1.0 (squared so it feels natural)
    const float v = static_cast<float>(_info.volume) / VOLUME_MAX;
    const float gain = v * v;

    int16_t out[SPEAKER_MAX_SAMPLES];
    const size_t n = count < SPEAKER_MAX_SAMPLES ? count : SPEAKER_MAX_SAMPLES;

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
    const auto audio_state = param->audio_stat.state;

    if (audio_state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED || audio_state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) {
        // The call decides the sample rate: 16 kHz for mSBC, 8 kHz for CVSD.
        const uint32_t rate = (audio_state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) ? MSBC_RATE : CVSD_RATE;

        // Set the speaker to the same rate, otherwise 8 kHz audio would play at double speed.
        // Does nothing if the speaker already runs at this rate.
        if (speaker.p_speaker && !speaker.p_speaker->configureTX(rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO))
            ESP_LOGW(BT_HF_TAG, "Could not set the speaker to %u Hz", (unsigned)rate);

        esp_hf_client_register_data_callback(speaker.p_speaker ? bt_app_hf_client_incoming_cb_wrapper : nullptr, bt_app_hf_client_outgoing_cb);
        if (!start_microphone(rate))
            ESP_LOGE(BT_HF_TAG, "HFP audio connected without microphone capture");
        if (!audio_active) {
            audio_active = true;
            run_cb(callbacks.cb_audio_connected);
        }
    } else if (audio_state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED) {
        audio_stopped();
    }
}

// Call audio ended. Also called when the phone disconnects, in case the stack
// did not report the audio ending first.
void EspHf::Impl::audio_stopped(void)
{
    stop_microphone();
    if (audio_active) {
        audio_active = false;
        run_cb(callbacks.cb_audio_disconnected);
    }
}

void EspHf::Impl::set_mic_mute(bool mute)
{
    mic.muted = mute;
}


static constexpr const char* VOLUME_NVS_NAMESPACE = "esp32_hf";
static constexpr const char* VOLUME_NVS_KEY = "volume";

void EspHf::Impl::load_volume(void)
{
    nvs_handle_t handle;
    uint8_t saved;

    if (nvs_open(VOLUME_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return;
    if (nvs_get_u8(handle, VOLUME_NVS_KEY, &saved) == ESP_OK && saved <= VOLUME_MAX)
        _info.volume = saved;
    nvs_close(handle);
}

void EspHf::Impl::save_volume(int new_volume)
{
    if (new_volume < 0 || new_volume > VOLUME_MAX || new_volume == _info.volume)
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
