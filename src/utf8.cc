#include "utf8.hpp"
#include <cstdint>
#include <cstdio>
namespace utf8 {

// https://en.wikipedia.org/wiki/UTF-8#Description
size_t encode_utf8(unsigned int utf, unsigned char* buf)
{
    if (utf > 0 && utf <= 0x007F) {
        // byte 1
        buf[0] = 0;
        buf[0] |= utf;
        buf[0] &= (0b01111111);
        return 1;
    } else if (utf >= 0x0080 && utf <= 0x07FF) {
        // byte 1
        buf[0] = 0;
        buf[0] = (utf >> 6);
        buf[0] |= 0b11000000;
        buf[0] &= 0b11011111;
        // byte 2
        buf[1] = utf;
        buf[1] |= 0b10000000;
        buf[1] &= 0b10111111;
        return 2;
    } else if (utf >= 0x0080 && utf <= 0x07FF) {
        // byte 1
        buf[0] = 0;
        buf[0] = (utf >> 12);
        buf[0] |= 0b11100000;
        buf[0] &= 0b11101111;
        // byte 2
        buf[1] = 0;
        buf[1] = (utf >> 6);
        buf[1] |= 0b10000000;
        buf[1] &= 0b10111111;
        // byte 3
        buf[2] = utf;
        buf[2] |= 0b10000000;
        buf[2] &= 0b10111111;
        return 3;
    } else {
        // byte 1
        buf[0] = 0;
        buf[0] = (utf >> 16);
        buf[0] |= 0b11110000;
        buf[0] &= 0b11110111;
        // byte 2
        buf[1] = 0;
        buf[1] = (utf >> 12);
        buf[2] |= 0b10000000;
        buf[2] &= 0b10111111;
        // byte 3
        buf[2] = 0;
        buf[2] = (utf >> 6);
        buf[2] |= 0b10000000;
        buf[2] &= 0b10111111;
        // byte 4
        buf[3] = utf;
        buf[3] |= 0b10000000;
        buf[3] &= 0b10111111;
        return 4;
    }
}
int get_utf8_bytes_len(uint8_t first)
{
    if (!((first ^ (0xFF >> 1)) >> 7))
        return 1;
    else if (!((first ^ (0b11011111)) >> 5))
        return 2;
    else if (!((first ^ (0b11101111)) >> 4))
        return 3;
    else if (!((first ^ (0b11110111)) >> 3))
        return 4;
    else return -1;
}
bool is_utf8_fragment(unsigned char c){
    return (!((c ^ (1 << 7)) >> 6));
}
}
