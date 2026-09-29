/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp32_hf.hpp"
#include "freertos/idf_additions.h"
#include <cstring>
#undef BT_HF_TAG
#define BT_HF_TAG "BT_HF"
// This is the number that gets send by the esp32 internal when you call someone on any other app then over your normal Simcard.
#define NO_SIMCARD_CALL_NUMBER "10000000"

extern "C" {
#include "esp_hf_defs.h"
#include <stdint.h>
#include "esp_hf_client_api.h"
#include "freertos/ringbuf.h"
}



// esp_hf_service_availability_status_t
const char *c_service_availability_status_str[] = {
    "unavailable",
    "available",
};

// esp_hf_roaming_status_t
const char *c_roaming_status_str[] = {
    "inactive",
    "active",
};

// esp_hf_client_call_state_t
const char *c_call_str[] = {
    "NO call in progress",
    "call in progress",
};

// esp_hf_client_callsetup_t
const char *c_call_setup_str[] = {
    "NONE",
    "INCOMING",
    "OUTGOING_DIALING",
    "OUTGOING_ALERTING"
};

// esp_hf_client_callheld_t
const char *c_call_held_str[] = {
    "NONE held",
    "Held and Active",
    "Held",
};

// esp_hf_response_and_hold_status_t
const char *c_resp_and_hold_str[] = {
    "HELD",
    "HELD ACCEPTED",
    "HELD REJECTED",
};

// esp_hf_client_call_direction_t
const char *c_call_dir_str[] = {
    "outgoing",
    "incoming",
};

// esp_hf_client_call_state_t
const char *c_call_state_str[] = {
    "active",
    "held",
    "dialing",
    "alerting",
    "incoming",
    "waiting",
    "held_by_resp_hold",
};

// esp_hf_current_call_mpty_type_t
const char *c_call_mpty_type_str[] = {
    "single",
    "multi",
};

// esp_hf_volume_control_target_t
const char *c_volume_control_target_str[] = {
    "SPEAKER",
    "MICROPHONE"
};

// esp_hf_at_response_code_t
const char *c_at_response_code_str[] = {
    "OK",
    "ERROR"
    "ERR_NO_CARRIER",
    "ERR_BUSY",
    "ERR_NO_ANSWER",
    "ERR_DELAYED",
    "ERR_BLACKLILSTED",
    "ERR_CME",
};

// esp_hf_client_in_band_ring_state_t
const char *c_inband_ring_state_str[] = {
    "NOT provided",
    "Provided",
};

#if CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI

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

void EspHf::bt_app_hf_client_incoming_cb(const uint8_t *buf, uint32_t sz) {
    // if (!this->speaker)
    // {
    //     ESP_LOGE(BT_HF_TAG, "Speaker object is not set.");
    //     return ;
    // }
    EspHf s_self = EspHf::getInstance();
    s_self._speaker.speaker.write(buf, sz);
}

void EspHf::bt_app_hf_client_incoming_cb_wrapper(const uint8_t *buf, uint32_t sz)
{
    if (! m_rb) {
        return;
    }
    getInstance().bt_app_hf_client_incoming_cb(buf, sz);

    // BaseType_t done = xRingbufferSend(m_rb, (uint8_t *)buf, sz, 0);
    // if (! done) {
    //     ESP_LOGE(BT_HF_TAG, "rb send fail");
    // }

    // write_bs(buf, sz);

    // esp_hf_client_outgoing_data_ready();
}

void EspHf::bt_app_hf_client_cb_wrapper(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
    getInstance().bt_app_hf_client_cb(event, param);
}


void EspHf::connection_change(const esp_hf_client_cb_param_t *param) {
    // ESP_LOGI(BT_HF_TAG, "--connection state %s, peer feats 0x%" PRIx32 ", chld_feats 0x%" PRIx32 ,
    //     c_connection_state_str[param->conn_stat.state],
        // param->conn_stat.peer_feat,
        // param->conn_stat.chld_feat);

    if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED && _connected) {
        _connected = false;
        if (_callbacks.cb_bt_disconnected)
            xTaskCreatePinnedToCore(cb_dis_wrap, "dis", 4096, (void *)_callbacks.cb_bt_disconnected, 1, nullptr, 1);
    }
    else if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED && !_connected) {
        _connected = true;
        if (_callbacks.cb_bt_connected)
            xTaskCreatePinnedToCore(cb_con_wrap, "con", 4096, (void *)_callbacks.cb_bt_connected, 1, nullptr, 1);
    }
    memcpy(this->_peer_addr, param->conn_stat.remote_bda, ESP_BD_ADDR_LEN);
}

void EspHf::audio_change(esp_hf_client_cb_param_t *param) {
    // ESP_LOGI(BT_HF_TAG, "--audio state %s",
    //     c_audio_state_str[param->audio_stat.state]);

    if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED ||
        param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) {
        esp_hf_client_register_data_callback(bt_app_hf_client_incoming_cb_wrapper,
                                            bt_app_hf_client_outgoing_cb);
        bt_app_hf_client_audio_open();
    } else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED) {
        bt_app_hf_client_audio_close();
    }
}

void EspHf::call(const char *phone_number, bool is_simcard_call, void (*fn)(char *, bool)) {
    static t_arg arg;

    arg.fn = fn;
    arg.is_simcard_call = is_simcard_call;

    if (!is_simcard_call)
        arg.phone_number[0] = '\0';
    else
        strcpy(arg.phone_number, phone_number);

    xTaskCreatePinnedToCore(call_wrap, nullptr, 4096, &arg, 1, nullptr, 1);
}


#endif /* #if CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI */
/* callback for HF_CLIENT */
void EspHf::bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    switch (event) {
        ESP_LOGI(BT_HF_TAG, "--audio state %d", param->audio_stat.state);

        case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
            connection_change(param); break;

        case ESP_HF_CLIENT_AUDIO_STATE_EVT:
            audio_change(param); break;

        case ESP_HF_CLIENT_CIND_SERVICE_AVAILABILITY_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--NETWORK STATE %s",
                    c_service_availability_status_str[param->service_availability.status]);
            break;
        }


        case ESP_HF_CLIENT_CIND_SIGNAL_STRENGTH_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "-- signal strength: %d",
                    param->signal_strength.value);
            break;
        }

        case ESP_HF_CLIENT_CIND_BATTERY_LEVEL_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--battery level %d",
                    param->battery_level.value);
            break;
        }

        // Will be called in ingoing and outgoing for other apps.
        case ESP_HF_CLIENT_CIND_CALL_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "incoming call from other apps: %d", param->call.status);
            if (param->call.status == ESP_HF_CALL_STATUS_CALL_IN_PROGRESS)
                _incoming_call = true;
            break;
        }

        // Will be called for outgoing for otehr apps and simcard calls.
        case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--Call setup indicator %s",
                    c_call_setup_str[param->call_setup.status]);

            if (param->call_setup.status != ESP_HF_CALL_SETUP_STATUS_IDLE && param->call_setup.status != ESP_HF_CALL_SETUP_STATUS_OUTGOING_DIALING)
                esp_hf_client_query_current_calls();
            else
                _incoming_call = false;
            break;
        }

        case ESP_HF_CLIENT_CIND_CALL_HELD_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--Call held indicator %s",
                    c_call_held_str[param->call_held.status]);
            break;
        }

        case ESP_HF_CLIENT_BTRH_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--response and hold %s",
                    c_resp_and_hold_str[param->btrh.status]);
            break;
        }

        case ESP_HF_CLIENT_CCWA_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--call_waiting %s",
                    (param->ccwa.number == NULL) ? "NULL" : (param->ccwa.number));
            break;
        }

        case ESP_HF_CLIENT_CLCC_EVT:
        {
            bool has_phone_number = (strcmp(param->clcc.number, NO_SIMCARD_CALL_NUMBER) != 0);
            // This will only be called on outgoing call.
            ESP_LOGI(BT_HF_TAG, "--Current call: idx %d, dir %s, state %s, mpty %s, number %s",
                    param->clcc.idx,
                    c_call_dir_str[param->clcc.dir],
                    c_call_state_str[param->clcc.status],
                    c_call_mpty_type_str[param->clcc.mpty],
                    (param->clcc.number == NULL) ? "NULL" : (param->clcc.number));

            if (param->clcc.dir == ESP_HF_CURRENT_CALL_DIRECTION_INCOMING || _incoming_call)
                call(param->clcc.number, has_phone_number, _callbacks.cb_incoming_call);
            else
                call(param->clcc.number, has_phone_number, _callbacks.cb_outgoing_call);
            
            _incoming_call = false;
            break;
        }

        case ESP_HF_CLIENT_VOLUME_CONTROL_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--volume_target: %s, volume %d",
                    c_volume_control_target_str[param->volume_control.type],
                    param->volume_control.volume);
            break;
        }

        case ESP_HF_CLIENT_AT_RESPONSE_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--AT response event, code %d, cme %d",
                    param->at_response.code, param->at_response.cme);
            break;
        }

        case ESP_HF_CLIENT_CNUM_EVT:
        {
            // ESP_LOGI(BT_HF_TAG, "--subscriber type %s, number %s",
            //         c_subscriber_service_type_str[param->cnum.type],
            //         (param->cnum.number == NULL) ? "NULL" : param->cnum.number);

            ESP_LOGI(BT_HF_TAG, "--subscriber type: %d, number %s",
                        param->cnum.type,
                        (param->cnum.number == NULL) ? "NULL" : param->cnum.number);
            break;
        }

        case ESP_HF_CLIENT_BSIR_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--inband ring state %s",
                    c_inband_ring_state_str[param->bsir.state]);
            break;
        }

        case ESP_HF_CLIENT_BINP_EVT:
        {
            ESP_LOGI(BT_HF_TAG, "--last voice tag number: %s",
                    (param->binp.number == NULL) ? "NULL" : param->binp.number);
            break;
        }
        case ESP_HF_CLIENT_PKT_STAT_NUMS_GET_EVT:
        {
            ESP_LOGE(BT_HF_TAG, "ESP_HF_CLIENT_PKT_STAT_NUMS_GET_EVT: %d", event);
            break;
        }
        case ESP_HF_CLIENT_PROF_STATE_EVT:
        {
            if (ESP_HF_INIT_SUCCESS == param->prof_stat.state) {
                ESP_LOGI(BT_HF_TAG, "HF PROF STATE: Init Complete");
            } else if (ESP_HF_DEINIT_SUCCESS == param->prof_stat.state) {
                ESP_LOGI(BT_HF_TAG, "HF PROF STATE: Deinit Complete");
            } else {
                ESP_LOGE(BT_HF_TAG, "HF PROF STATE error: %d", param->prof_stat.state);
            }
            break;
        }
        default:
            ESP_LOGE(BT_HF_TAG, "HF_CLIENT EVT: %d", event);
            break;
    }
}