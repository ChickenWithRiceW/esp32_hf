#include "esp32_hf_intern.hpp"
#include "freertos/idf_additions.h"
#include <cstring>
#include <new>

#undef BT_HF_TAG
#define BT_HF_TAG "BT_HF"

// This is the number that gets send by the esp32 internal when you call someone on any other app then over your normal simcard.
static constexpr const char* NO_SIMCARD_CALL_NUMBER = "10000000";

extern "C" {
#include "esp_hf_client_api.h"
#include "esp_hf_defs.h"
#include <stdint.h>
}

// esp_hf_client_callsetup_t
const char* c_call_setup_str[] = {
    "NONE",
    "INCOMING",
    "OUTGOING_DIALING",
    "OUTGOING_ALERTING"
};

// esp_hf_client_callheld_t
const char* c_call_held_str[] = {
    "NONE held",
    "Held and Active",
    "Held",
};

// esp_hf_response_and_hold_status_t
const char* c_resp_and_hold_str[] = {
    "HELD",
    "HELD ACCEPTED",
    "HELD REJECTED",
};

// esp_hf_volume_control_target_t
const char* c_volume_control_target_str[] = {
    "SPEAKER",
    "MICROPHONE"
};

// Work out the public state. It follows the phone's call indicators, so it stays
// correct whether or not call audio is connected.
void EspHf::Impl::update_state(void)
{
    if (!_info.connected)
        _info.state = IDLE_DISCONNECTED;
    else if (call_setup == ESP_HF_CALL_SETUP_STATUS_INCOMING)
        _info.state = INCOMING_CALL;
    else if (call_setup == ESP_HF_CALL_SETUP_STATUS_OUTGOING_DIALING || call_setup == ESP_HF_CALL_SETUP_STATUS_OUTGOING_ALERTING)
        _info.state = OUTGOING_CALL;
    else if (call_active)
        _info.state = IN_CALL;
    else
        _info.state = IDLE_CONNECTED;
}

void EspHf::Impl::connection_change(const esp_hf_client_cb_param_t* param)
{
    const esp_hf_client_connection_state_t state = param->conn_stat.state;

    link_state = state;
    memcpy(peer_addr, param->conn_stat.remote_bda, ESP_BD_ADDR_LEN);

    // The phone is ready once the service level connection is up (the earlier
    // CONNECTED state is only the raw link). Commands only work from here on.
    if (state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED && !_info.connected) {
        _info.connected = true;
        update_state();
        esp_bt_gap_read_remote_name(peer_addr);
        run_cb(callbacks.cb_bt_connected);
    } else if (state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
        // Also happens when a connection attempt fails. Forget everything about the phone.
        const bool was_connected = _info.connected;
        _info.connected = false;
        _info.battery_level = -1;
        _info.signal_strength = -1;
        _info.network_available = false;
        _info.device_name[0] = '\0';
        call_active = false;
        call_setup = ESP_HF_CALL_SETUP_STATUS_IDLE;
        incoming_call = false;
        audio_stopped();
        update_state();
        // The reconnect timer tries again by itself.
        if (was_connected)
            run_cb(callbacks.cb_bt_disconnected);
    }
}

void EspHf::Impl::call(const char* phone_number, bool is_simcard_call, void (*fn)(char*, bool))
{
    if (!fn)
        return;

    // Own copy per event, so quick back-to-back events can't overwrite each other.
    // The callback task frees it; if the task could not start, free it here.
    t_call* arg = new (std::nothrow) t_call;
    if (!arg)
        return;

    arg->fn = fn;
    arg->is_simcard_call = is_simcard_call;
    if (is_simcard_call && phone_number)
        snprintf(arg->phone_number, sizeof(arg->phone_number), "%s", phone_number);

    if (xTaskCreatePinnedToCore(call_wrap, nullptr, TASK_SIZE, arg, 1, nullptr, 1) != pdPASS)
        delete arg;
}

void EspHf::Impl::call_event(char* phone_number, esp_hf_current_call_direction_t dir)
{
    // The number can be missing, treat that like an app call.
    bool has_phone_number = (phone_number && strcmp(phone_number, NO_SIMCARD_CALL_NUMBER) != 0);

    if (dir == ESP_HF_CURRENT_CALL_DIRECTION_INCOMING || incoming_call) {
        call(phone_number, has_phone_number, callbacks.cb_incoming_call);
    } else {
        call(phone_number, has_phone_number, callbacks.cb_outgoing_call);
    }
    incoming_call = false;
}

void EspHf::Impl::bt_app_hf_client_cb_wrapper(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param)
{
    getInstance()._impl->bt_app_hf_client_cb(event, param);
}

void EspHf::Impl::bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param)
{
    switch (event) {
    case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
        connection_change(param);
        break;

    case ESP_HF_CLIENT_AUDIO_STATE_EVT:
        audio_change(param);
        break;

    case ESP_HF_CLIENT_CLCC_EVT:
        call_event(param->clcc.number, param->clcc.dir);
        break;

    case ESP_HF_CLIENT_CIND_SERVICE_AVAILABILITY_EVT: {
        ESP_LOGI(BT_HF_TAG, "--NETWORK STATE %d", param->service_availability.status);
        _info.network_available = param->service_availability.status;
        break;
    }

    case ESP_HF_CLIENT_CIND_SIGNAL_STRENGTH_EVT: {
        ESP_LOGI(BT_HF_TAG, "--signal strength: %d", param->signal_strength.value);
        _info.signal_strength = param->signal_strength.value;
        break;
    }

    case ESP_HF_CLIENT_CIND_BATTERY_LEVEL_EVT: {
        ESP_LOGI(BT_HF_TAG, "--battery level %d", param->battery_level.value);
        _info.battery_level = param->battery_level.value;
        break;
    }

    // Will be called in ingoing and outgoing for other apps.
    case ESP_HF_CLIENT_CIND_CALL_EVT: {
        ESP_LOGI(BT_HF_TAG, "incoming call from other apps: %d", param->call.status);
        const bool active = (param->call.status == ESP_HF_CALL_STATUS_CALL_IN_PROGRESS);
        if (active)
            incoming_call = true;
        if (active != call_active) {
            call_active = active;
            run_cb(active ? callbacks.cb_call_active : callbacks.cb_call_ended);
        }
        update_state();
        break;
    }

    // Will be called for outgoing for otehr apps and simcard calls.
    case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT: {
        ESP_LOGI(BT_HF_TAG, "--Call setup indicator %s",
            c_call_setup_str[param->call_setup.status]);

        if (param->call_setup.status != ESP_HF_CALL_SETUP_STATUS_IDLE && param->call_setup.status != ESP_HF_CALL_SETUP_STATUS_OUTGOING_DIALING)
            esp_hf_client_query_current_calls();
        else
            incoming_call = false;
        call_setup = param->call_setup.status;
        update_state();
        break;
    }

    // TODO: Call waiting is having some unexpected behaviour and should be looked at in the future.
    case ESP_HF_CLIENT_CIND_CALL_HELD_EVT: {
        ESP_LOGI(BT_HF_TAG, "--Call held indicator %s",
            c_call_held_str[param->call_held.status]);
        break;
    }

    case ESP_HF_CLIENT_BTRH_EVT: {
        ESP_LOGI(BT_HF_TAG, "--response and hold %s",
            c_resp_and_hold_str[param->btrh.status]);
        break;
    }

    case ESP_HF_CLIENT_CCWA_EVT: {
        ESP_LOGI(BT_HF_TAG, "--call_waiting %s",
            (param->ccwa.number == NULL) ? "NULL" : (param->ccwa.number));
        break;
    }
        // TODO: End of not tested cases.

    case ESP_HF_CLIENT_VOLUME_CONTROL_EVT:
        // Only the speaker volume matters here, ignore microphone volume changes.
        if (param->volume_control.type == ESP_HF_VOLUME_CONTROL_TARGET_SPK)
            save_volume(param->volume_control.volume);
        break;
    }
}
