#include "esp32_hf.hpp"

bool EspHf::isStrPrintable(const char *str) {
	size_t	i;

	i = 0;
	if (!str || !str[0])
		return false;

	while (str[i]) {
		if (!isprint(str[i++]))
			return false;
	}
	return true;
}
