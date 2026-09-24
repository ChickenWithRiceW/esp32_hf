#include "esp32_hf.hpp"

void EspHf::cb_con_wrap(void *ptr) {
    reinterpret_cast<void (*)()>(ptr)();
    vTaskDelete(nullptr);
}

void EspHf::cb_dis_wrap(void *ptr) {
    reinterpret_cast<void (*)()>(ptr)();
    vTaskDelete(nullptr);
}