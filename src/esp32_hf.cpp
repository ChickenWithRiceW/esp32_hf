#include "esp32_hf_intern.hpp"

#include "esp_hf_client_api.h"

#define BT_HF_TAG "BT_HF_HF"

EspHf::EspHf()
    : _impl(new Impl)
{
}
EspHf::~EspHf() = default;

bool EspHf::begin(void)
{
    if (_impl->initialised) {
        ESP_LOGE(BT_HF_TAG, "You already begin the hf, you cannot init it twice");
        return false;
    }

    if (!_impl->validate_config()) {
        ESP_LOGE(BT_HF_TAG, "Config validation failed");
        return false;
    }

    if (!_impl->init_controller_and_bluetooth(_impl->bt.ssp_enabled)) {
        ESP_LOGE(BT_HF_TAG, "Bluetooth initialisation was not successful");
        return false;
    }

    _impl->load_volume();

    // Bluetooth events can arrive as soon as the stacks are set up, so mark
    // the library as started first and undo it if the setup fails.
    _impl->initialised = true;
    if (!_impl->setup_stacks()) {
        ESP_LOGE(BT_HF_TAG, "Stack setup was not successful");
        _impl->initialised = false;
        _impl->shutdown_bluetooth();
        return false;
    }
    return true;
}

// Start flash storage, the Bluetooth controller and the Bluedroid stack.
// On failure everything that was started is stopped again, so begin() can be retried.
bool EspHf::Impl::init_controller_and_bluetooth(bool ssp_enabled)
{
    // NVS stores the Bluetooth pairing keys and calibration data.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() != ESP_OK) {
            ESP_LOGE(BT_HF_TAG, "Could not erase flash storage.");
            return false;
        }
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "Could not start flash storage: %s", esp_err_to_name(ret));
        return false;
    }

    // Release the memory of Bluetooth Low Energy, as we do not use it.
    // This fails on a retry because it was already released, which is fine.
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    if (ret != ESP_OK)
        ESP_LOGW(BT_HF_TAG, "Could not release BLE memory: %s", esp_err_to_name(ret));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        shutdown_bluetooth();
        return false;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    bluedroid_cfg.ssp_en = ssp_enabled;

    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        shutdown_bluetooth();
        return false;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        shutdown_bluetooth();
        return false;
    }
    return true;
}

// Stop everything in reverse order. Each step is allowed to fail, because this is
// also used when only part of the startup had worked.
void EspHf::Impl::shutdown_bluetooth(void)
{
    Impl& impl = *getInstance()._impl;

    if (impl.reconnect_timer) {
        xTimerDelete(impl.reconnect_timer, portMAX_DELAY);
        impl.reconnect_timer = nullptr;
    }
    esp_hf_client_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
}

bool EspHf::Impl::setup_stacks(void)
{
    if (!setup_bt_gap(bt.device_name, bt.ssp_enabled, bt.legacy_pairing_pin))
        return false;

    ERROR_CHECK(esp_hf_client_register_callback(Impl::bt_app_hf_client_cb_wrapper), "Could not register hf client callback.")
    ERROR_CHECK(esp_hf_client_init(), "Could not initialize hf client.")

    // Connect to a bonded phone now, and keep trying while none is connected.
    reconnect_timer = xTimerCreate("hf_reconnect", pdMS_TO_TICKS(RECONNECT_INTERVAL_MS), pdTRUE, nullptr, reconnect_timer_cb);
    if (!reconnect_timer || xTimerStart(reconnect_timer, 0) != pdPASS) {
        ESP_LOGE(BT_HF_TAG, "Could not start the reconnect timer.");
        return false;
    }
    try_reconnect();
    return true;
}

bool EspHf::Impl::validate_config(void)
{
    if (!bt.device_name[0]) {
        ESP_LOGE(BT_HF_TAG, "No device name set.");
        return false;
    }

    if (!bt.legacy_pairing_pin[0]) {
        ESP_LOGE(BT_HF_TAG, "No legacy pin set.");
        return false;
    }
    return true;
}
