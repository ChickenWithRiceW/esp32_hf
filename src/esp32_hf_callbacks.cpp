#include "esp32_hf.hpp"

void EspHf::cb_con_wrap(void *ptr) {
    reinterpret_cast<void (*)()>(ptr)();
    vTaskDelete(nullptr);
}

void EspHf::cb_dis_wrap(void *ptr) {
    reinterpret_cast<void (*)()>(ptr)();
    vTaskDelete(nullptr);
}

void EspHf::wrap(void *ptr) {
    reinterpret_cast<void (*)()>(ptr)();
    vTaskDelete(nullptr);
}

void EspHf::call_wrap(void *ptr) {
    t_arg *p_arg;

    p_arg = reinterpret_cast<t_arg (*)>(ptr);
    p_arg->fn(p_arg->phone_number, p_arg->is_simcard_call);
    free(p_arg);
    vTaskDelete(nullptr);
}
