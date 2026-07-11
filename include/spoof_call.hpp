#pragma once

namespace spf {
    template<typename R, typename... A>
    static inline R _d0(R(*f)(A...), A... a) { return f(a...); }
    template<typename R, typename... A>
    static inline R _d1(R(*f)(A...), A... a) { return f(a...); }
    template<typename R, typename... A>
    static inline R _d2(R(*f)(A...), A... a) { return f(a...); }
    template<typename R, typename... A>
    static inline R _d3(R(*f)(A...), A... a) { return f(a...); }

    static int _i = 0;
    static inline int _n() { _i = (_i + 1) & 3; return _i; }

    template<typename R, typename... A>
    inline R call(R(*f)(A...), A... a) {
        switch (_n()) {
        case 0: return _d0(f, a...);
        case 1: return _d1(f, a...);
        case 2: return _d2(f, a...);
        case 3: return _d3(f, a...);
        default: return f(a...);
        }
    }
}

#define SPOOF(fn, ...) spf::call(fn, __VA_ARGS__)
