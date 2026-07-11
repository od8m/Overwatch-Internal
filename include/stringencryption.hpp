#pragma once
#include <cstdint>
#include <cstddef>

namespace crypt {
    template<size_t N>
    struct enc {
        char d[N];
        constexpr enc(const char(&p)[N]) : d{} {
            for (size_t i = 0; i < N; i++)
                d[i] = p[i] ^ (char)(0xAB + i * 0x37);
        }
        void dec(char* o) const {
            for (size_t i = 0; i < N; i++)
                o[i] = d[i] ^ (char)(0xAB + i * 0x37);
        }
    };
}

#define S(str) ([]() -> const char* { \
    static constexpr crypt::enc<sizeof(str)> _c(str); \
    static char _b[sizeof(str)]; \
    static bool _r = false; \
    if (!_r) { _c.dec(_b); _r = true; } \
    return _b; \
}())
