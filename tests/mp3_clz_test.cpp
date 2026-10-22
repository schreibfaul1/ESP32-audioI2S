#include "mp3_decoder/structs.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {
int referenceCount(uint32_t value) {
    int count = 0;
    for (uint32_t bit = UINT32_C(0x80000000); bit && !(value & bit); bit >>= 1) ++count;
    return count;
}

void check(uint32_t value) {
    assert(CLZ(value) == referenceCount(value));
    assert(CLZ(value) >= 0 && CLZ(value) <= 32);
}
}

int main() {
    volatile uint32_t zero = 0;
    assert(CLZ(zero) == 32);
    // Decoder guard-bit callers subtract one, including for silent blocks.
    assert(CLZ(zero) - 1 == 31);
    for (uint32_t bit = 0; bit < 32; ++bit) {
        const uint32_t value = UINT32_C(1) << bit;
        check(value);
        check(value - 1);
        check(value + 1);
    }
    check(std::numeric_limits<uint32_t>::max());
    check(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()));
    check(static_cast<uint32_t>(-1));

    uint32_t state = UINT32_C(0x12345678);
    for (std::size_t i = 0; i < 100000; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        check(state);
    }
    uint32_t evaluated = 0;
    assert(CLZ(evaluated++) == 32 && evaluated == 1);
}
