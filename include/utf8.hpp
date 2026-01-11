#ifndef UTF8_HPP
#define UTF8_HPP

#define is_utf8_fragment(C) (!((C ^ (1 << 7)) >> 6))
#include <cstddef>

namespace utf8 {
    size_t encode_utf8(int utf, char* buf);
    int get_utf8_bytes_len(char first);
}

#endif
