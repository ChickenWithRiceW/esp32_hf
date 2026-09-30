#include "esp32_hf_intern.hpp"


e_state EspHf::getState(void)
{
	return _impl->_info.state;
}