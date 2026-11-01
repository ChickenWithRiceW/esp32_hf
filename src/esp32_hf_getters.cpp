#include "esp32_hf_intern.hpp"

e_state EspHf::getState(void)
{
    return _impl->_info.state;
}

bool EspHf::isConnected(void)
{
    return _impl->_info.connected;
}

int EspHf::getBatteryLevel(void)
{
    return _impl->_info.battery_level;
}

int EspHf::getSignalStrength(void)
{
    return _impl->_info.signal_strength;
}

bool EspHf::isNetworkAvailable(void)
{
    return _impl->_info.network_available;
}

const char* EspHf::getPeerName(void)
{
    return _impl->_info.device_name;
}

int EspHf::getVolume(void)
{
    return _impl->_info.volume;
}
