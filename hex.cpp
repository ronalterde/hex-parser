#include "hex.h"

namespace hex {

uint8_t hexCharToNumber(char c) {
	if (c >= 0x61)
		return c - 0x61 + 0x0a;
	if (c >= 0x41)
		return c - 0x41 + 0x0a;
	return c - 0x30;
}

uint8_t hexCharToNumber(char high, char low) {
	return (hexCharToNumber(high) << 4) | hexCharToNumber(low);
}

}
