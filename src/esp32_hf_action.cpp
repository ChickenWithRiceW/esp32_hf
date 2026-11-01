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

bool EspHf::callNumber(char *phone_number) {
    ASSERT_INIT
    if (!phone_number || !phone_number[0])
        return false;
    return (esp_hf_client_dial(phone_number) == 0);
}