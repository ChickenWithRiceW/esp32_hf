/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_APP_HF_H__
#define __BT_APP_HF_H__

#include <stdint.h>
#include "esp_hf_client_api.h"


#define BT_HF_TAG               "BT_HF"

/**
 * @brief     callback function for HF client
 */
extern "C" {
void bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
}

// Just for now
// void write_bs(const uint8_t *buf, uint32_t sz);

#endif /* __BT_APP_HF_H__*/
