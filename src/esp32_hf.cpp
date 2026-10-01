#include "esp32_hf_intern.hpp"

#include "esp_hf_client_api.h"

#define BT_HF_TAG "BT_HF_HF"

// EspHf::EspHf() {}

EspHf::EspHf() : _impl(new Impl) {}
EspHf::~EspHf() = default;

bool EspHf::Impl::init_controller_and_bluetooth(bool ssp_enabled) {
    // Initialize NVS — it is used to store PHY calibration data
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    
    // Change bluetooth mode to match desired bluetooth classic.
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    bluedroid_cfg.ssp_en = ssp_enabled;
    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool EspHf::begin(void)
{
    if (this->_impl->initialised)
    {
        ESP_LOGE(BT_HF_TAG, "You already begin the hf, you cannot init it twice");
        return false;
    }

    if (!_impl->validate_config())
    {
        ESP_LOGE(BT_HF_TAG, "Validate fail");
        return false;
    }
    
    if (!_impl->init_controller_and_bluetooth(_impl->bt.ssp_enabled))
    {
        ESP_LOGE(BT_HF_TAG, "Init did not work");
        return false;
    }
    
    if (!_impl->setup_stacks())
    {
        ESP_LOGE(BT_HF_TAG, "Stack didnt work");
        return false;
    }
    
    #if ACOUSTIC_ECHO_CANCELLATION_ENABLE
        app_gpio_aec_io_cfg();
    #endif

    _impl->initialised = true;
    return true;
}

bool EspHf::Impl::setup_stacks(void)
{
    if (!setup_bt_gap(bt.device_name, bt.ssp_enabled, bt.legacy_pairing_pin))
        return false;

    ERROR_CHECK(esp_hf_client_register_callback(Impl::bt_app_hf_client_cb_wrapper), "Could not register hf client callback.")
    ERROR_CHECK(esp_hf_client_init(), "Could not initialize hf client.")

    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
    return true;
}

bool EspHf::Impl::validate_config(void) {
    if (!bt.device_name)
    {
        ESP_LOGE(BT_HF_TAG, "No device name set.");
        return false;
    }

    if (!bt.legacy_pairing_pin)
    {
        ESP_LOGE(BT_HF_TAG, "No legacy pin set.");
        return false;
    }
    return true;
}