#ifndef UTF8_HPP
#define UTF8_HPP

#include <cstdint>
#include <cstddef>

namespace utf8 {
    size_t encode_utf8(unsigned int utf, unsigned char* buf);
    int get_utf8_bytes_len(uint8_t first);
    bool is_utf8_fragment(unsigned char);
}

#endif
