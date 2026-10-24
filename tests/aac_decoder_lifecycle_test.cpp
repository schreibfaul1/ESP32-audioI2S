#include "neaacdec.h"

#include <cassert>
#include <cstdio>
#include <cstring>

// The host adapter exposes a ledger for the decoder's real malloc/free calls.
extern size_t host_live_allocation_count();
extern size_t host_live_allocation_bytes();
extern size_t host_live_allocations_of_size(size_t size);
extern void host_set_psram_available(bool available);
extern void host_discard_live_allocations();

namespace {
constexpr size_t kIterations = 100;

void requireEmptyLedger(const char* phase) {
    if (host_live_allocation_count() != 0) {
        std::fprintf(stderr, "%s: leaked %zu bytes in %zu allocations\n", phase,
                     host_live_allocation_bytes(), host_live_allocation_count());
        std::abort();
    }
}

struct Session {
    NeaacDecoder decoder;
    NeAACDecHandle handle = nullptr;

    void open() {
        assert(handle == nullptr);
        handle = decoder.NeAACDecOpen();
        assert(handle != nullptr);
        uint8_t config[] = {0x12, 0x10}; // AAC-LC, 44.1 kHz, stereo.
        uint32_t sampleRate = 0;
        uint8_t channels = 0;
        assert(decoder.NeAACDecInit2(handle, config, sizeof(config), &sampleRate, &channels) == 0);
        assert(sampleRate == 44100 && channels == 2);
    }

    void reset() {
        decoder.NeAACDecClose(handle);
        handle = nullptr;
    }

    ~Session() { reset(); }
};
}

int main(int argc, char** argv) {
    const bool expectLeak = argc == 2 && std::strcmp(argv[1], "--expect-leak") == 0;
    if (expectLeak) {
        for (size_t i = 0; i < kIterations; ++i) {
            Session session;
            session.open();
            session.reset();
            session.reset();
        }
#ifdef FIXED_POINT
        requireEmptyLedger("fixed regression control");
#else
        assert(host_live_allocation_count() == kIterations * 3);
        assert(host_live_allocation_bytes() == kIterations * (64 + 512 + 256) * sizeof(complex_t));
        for (size_t size : {64, 512, 256}) {
            assert(host_live_allocations_of_size(size * sizeof(complex_t)) == kIterations);
        }
        std::printf("REPRODUCED: %zu bytes in %zu allocations after %zu lifecycles\n",
                    host_live_allocation_bytes(), host_live_allocation_count(), kIterations);
        host_discard_live_allocations();
#endif
        return 0;
    }

    for (bool psram : {false, true}) {
        host_set_psram_available(psram);
        requireEmptyLedger("initial");
        for (size_t i = 0; i < kIterations; ++i) {
            {
                Session session;
                session.open();
                if (i % 2 == 0) {
                    session.reset();
                    session.reset();
                }
            }
            requireEmptyLedger("sequential lifecycle");
        }
        for (size_t i = 0; i < kIterations; ++i) {
            {
                Session first;
                Session second;
                first.open();
                second.open();
                const size_t bothLive = host_live_allocation_bytes();
                first.reset();
                first.reset();
                assert(second.decoder.NeAACDecGetCurrentConfiguration(second.handle)->defSampleRate == 44100);
                assert(host_live_allocation_bytes() > 0);
                assert(host_live_allocation_bytes() < bothLive);
                first.open();
                second.reset();
                first.reset();
                second.reset();
            }
            requireEmptyLedger("interleaved lifecycle");
        }
    }
    std::puts("PASS: 100 sequential and 100 interleaved lifecycles per RAM mode; zero live allocations");
}
