#include "esp32_hf.h"

static const char *TAG = "HFP";


EspHf::EspHf() {}

int EspHf::init_controller_and_bluetooth(bool ssp_enabled) {
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    
    // Change bluetooth mode to match desired bluetooth classic.
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    if (!ssp_enabled)
        bluedroid_cfg.ssp_en = false;
    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(BT_HF_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    return SUCCESS;
}

bool EspHf::begin(void)
{
    if (this->initialised)
    {
        ESP_LOGE(TAG, "You already begin the hf, you cannot init it twice");
        return false;
    }
    this->initialised = true;

    this->init_controller_and_bluetooth(this->cfg.bluetooth.ssp_enabled);

    this->setup_stacks();

    #if ACOUSTIC_ECHO_CANCELLATION_ENABLE
        app_gpio_aec_io_cfg();
    #endif
    return true;
}

void EspHf::setup_stacks(void)
{
    this->setup_bt_gap(this->cfg.bluetooth.device_name, this->cfg.bluetooth.ssp_enabled, this->cfg.bluetooth.legacy_pairing_pin);

    esp_hf_client_register_callback(EspHf::bt_app_hf_client_cb_wrapper);

    // ? I guess the black magic of what it does to enable it?
    esp_hf_client_init();

    // Removed because pbac is not exposed in arduino core.
#ifdef ESP_PLATFORM
    // esp_pbac_register_callback(bt_app_pbac_cb);
    // esp_pbac_init();
#endif

    // Start device discovery Gap
    ESP_LOGI("BT_HF_TAG", "Starting device discovery...");
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}

bool EspHf::validate_config(void) {
    if (!this->cfg.bluetooth.device_name)
        return false;

    // if (!this->cfg.bluetooth.legacy_pairing_pin)
    //     return false;
    return true;
}