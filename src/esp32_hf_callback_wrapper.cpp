#include "esp32_hf_intern.hpp"

void EspHf::Impl::wrap(void* ptr)
{
    reinterpret_cast<void (*)()>(ptr)();
    vTaskDelete(nullptr);
}

void EspHf::Impl::ssp_auth_wrap(void* ptr)
{
    t_auth* p_auth;

    p_auth = reinterpret_cast<t_auth(*)>(ptr);
    p_auth->fn(p_auth->auth_number);
    vTaskDelete(nullptr);
}

void EspHf::Impl::call_wrap(void* ptr)
{
    t_call* p_arg;

    p_arg = reinterpret_cast<t_call(*)>(ptr);
    p_arg->fn(p_arg->phone_number, p_arg->is_simcard_call);
    vTaskDelete(nullptr);
}
