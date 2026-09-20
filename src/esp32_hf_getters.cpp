#include "esp32_hf.h"


const char *EspHf::getBatteryLevel(void) {
    if (!this->cb_client_param)
        return "Error null pointer";
    return (this->cb_client_param->cnum.number == NULL) ? "NULL" : (this->cb_client_param->cnum.number);
}