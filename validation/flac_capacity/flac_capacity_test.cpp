// Host regression for FlacDecoder::decode output capacity.
// Frames are native FLAC (verbatim subframes, CRC-8). No flac(1) or ffmpeg
// binary is required. This is not an on-device Audio::sendBytes run.

#include "flac_decoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int32_t kSentinel = 0x11111111;
constexpr int32_t kCanary = static_cast<int32_t>(0xC0FFEE11u);
constexpr size_t kGuardWords = 32;

uint8_t crc8(const uint8_t* data, size_t length) {
    static const uint8_t table[256] = {
        0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
        0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD, 0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
        0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
        0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
        0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4, 0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
        0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
        0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
        0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83, 0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3};
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) { crc = table[crc ^ data[i]]; }
    return crc;
}

void appendSample(std::vector<uint8_t>& frame, int32_t sample, int bits) {
    if (bits == 16) {
        const uint16_t word = static_cast<uint16_t>(sample);
        frame.push_back(static_cast<uint8_t>(word >> 8));
        frame.push_back(static_cast<uint8_t>(word));
    } else {
        const uint32_t word = static_cast<uint32_t>(sample) & 0xFFFFFFu;
        frame.push_back(static_cast<uint8_t>(word >> 16));
        frame.push_back(static_cast<uint8_t>(word >> 8));
        frame.push_back(static_cast<uint8_t>(word));
    }
}

std::vector<uint8_t> makeFrame(int channels, int bits, int blockSize) {
    uint8_t blockCode = 0;
    if (blockSize == 4608) {
        blockCode = 5;
    } else if (blockSize == 4609) {
        blockCode = 7;
    } else if (blockSize == 8192) {
        blockCode = 13;
    }
    const uint8_t rateCode = 9;
    const uint8_t chan = static_cast<uint8_t>(channels - 1);
    const uint8_t sizeCode = bits == 16 ? 4 : 6;

    std::vector<uint8_t> frame;
    frame.push_back(0xFF);
    frame.push_back(0xF8);
    frame.push_back(static_cast<uint8_t>((blockCode << 4) | rateCode));
    frame.push_back(static_cast<uint8_t>((chan << 4) | (sizeCode << 1)));
    frame.push_back(0x00);
    if (blockCode == 7) {
        const uint16_t stored = static_cast<uint16_t>(blockSize - 1);
        frame.push_back(static_cast<uint8_t>(stored >> 8));
        frame.push_back(static_cast<uint8_t>(stored));
    }
    frame.push_back(crc8(frame.data(), frame.size()));

    for (int ch = 0; ch < channels; ch++) {
        frame.push_back(0x02);
        for (int i = 0; i < blockSize; i++) {
            const int32_t sample = ch == 0 ? i : (1000 + i);
            appendSample(frame, sample, bits);
        }
    }
    frame.push_back(0x00);
    frame.push_back(0x00);
    return frame;
}

int32_t expectedWord(int32_t sample, int bits) {
    return bits == 16 ? (sample << 16) : (sample << 8);
}

bool runCase(int channels, int bits, int blockSize, std::string* detail) {
    Audio audio;
    FlacDecoder decoder(audio);
    if (!decoder.init()) {
        *detail = "init failed";
        return false;
    }
    decoder.setRawBlockParams(static_cast<uint8_t>(channels), 44100, static_cast<uint8_t>(bits), static_cast<uint32_t>(blockSize), 0);

    std::vector<uint8_t> frame = makeFrame(channels, bits, blockSize);
    const int32_t frameBytes = static_cast<int32_t>(frame.size());
    // decodeNative refuses to parse subframes until bytesLeft is at least
    // FLAC_MAX_BLOCKSIZE. The production input buffer is far larger than one frame.
    frame.resize(static_cast<size_t>(frameBytes) + 24576, 0);
    if (decoder.findSyncWord(frame.data(), static_cast<int32_t>(frame.size())) != 0) {
        *detail = "findSyncWord rejected the frame";
        return false;
    }

    std::vector<int32_t> out(FLAC_MAX_OUT_WORDS + kGuardWords, kCanary);
    int32_t bytesLeft = static_cast<int32_t>(frame.size());
    int frameIndex = 0;
    int calls = 0;
    int firstChunk = -1;
    int secondChunk = -1;

    while (true) {
        std::fill(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(FLAC_MAX_OUT_WORDS), kSentinel);
        const int rc = decoder.decode(frame.data(), &bytesLeft, out.data());
        const int chunk = static_cast<int>(decoder.getOutputSamples());
        calls++;
        if (calls == 1) { firstChunk = chunk; }
        if (calls == 2) { secondChunk = chunk; }
        if (chunk <= 0 || static_cast<uint32_t>(chunk) > FLAC_MAX_OUT_FRAMES) {
            *detail = "chunk size out of range";
            return false;
        }
        if (frameIndex + chunk > blockSize) {
            *detail = "chunk ran past the block";
            return false;
        }
        for (int i = 0; i < chunk; i++) {
            const int index = frameIndex + i;
            const int32_t left = expectedWord(index, bits);
            const int32_t right = expectedWord(channels == 1 ? index : (1000 + index), bits);
            if (out[static_cast<size_t>(i) * 2] != left || out[static_cast<size_t>(i) * 2 + 1] != right) {
                *detail = "sample mismatch at frame " + std::to_string(index);
                return false;
            }
        }
        for (size_t word = static_cast<size_t>(chunk) * 2; word < FLAC_MAX_OUT_WORDS; word++) {
            if (out[word] != kSentinel) {
                *detail = "write inside the buffer past the chunk";
                return false;
            }
        }
        for (size_t word = FLAC_MAX_OUT_WORDS; word < out.size(); word++) {
            if (out[word] != kCanary) {
                *detail = "canary overwritten past 9216 words";
                return false;
            }
        }
        frameIndex += chunk;
        if (rc == FlacDecoder::FLAC_NONE) { break; }
        if (rc != FlacDecoder::GIVE_NEXT_LOOP) {
            *detail = "unexpected decode status " + std::to_string(rc);
            return false;
        }
        if (calls > 3) {
            *detail = "too many continuation calls";
            return false;
        }
    }

    if (frameIndex != blockSize) {
        *detail = "emitted " + std::to_string(frameIndex) + " frames, expected " + std::to_string(blockSize);
        return false;
    }
    if (bytesLeft != static_cast<int32_t>(frame.size()) - frameBytes) {
        *detail = "consumed " + std::to_string(static_cast<int32_t>(frame.size()) - bytesLeft) + " bytes, frame is " + std::to_string(frameBytes);
        return false;
    }
    if (blockSize <= static_cast<int>(FLAC_MAX_OUT_FRAMES)) {
        if (calls != 1 || firstChunk != blockSize) {
            *detail = "exact-fit block was split";
            return false;
        }
    } else {
        const int rest = blockSize - static_cast<int>(FLAC_MAX_OUT_FRAMES);
        if (calls != 2 || firstChunk != static_cast<int>(FLAC_MAX_OUT_FRAMES) || secondChunk != rest) {
            *detail = "continuation boundary mismatch";
            return false;
        }
    }
    *detail = "calls=" + std::to_string(calls) + " first=" + std::to_string(firstChunk) + " second=" + std::to_string(secondChunk) + " frames=" + std::to_string(frameIndex);
    return true;
}

} // namespace

int main() {
    if (FLAC_MAX_OUT_FRAMES != 4608 || FLAC_MAX_OUT_WORDS != 9216) {
        std::fprintf(stderr, "capacity constant is not 4608 frames / 9216 words\n");
        return 1;
    }

    const int channels[] = {1, 2};
    const int bits[] = {16, 24};
    const int blocks[] = {4608, 4609, 8192};
    int failed = 0;
    for (int ch : channels) {
        for (int bps : bits) {
            for (int block : blocks) {
                std::string detail;
                const bool ok = runCase(ch, bps, block, &detail);
                std::printf("%s ch=%d bps=%d block=%d %s\n", ok ? "PASS" : "FAIL", ch, bps, block, detail.c_str());
                if (!ok) { failed++; }
            }
        }
    }
    std::printf("result %s (%d failed)\n", failed == 0 ? "PASS" : "FAIL", failed);
    return failed == 0 ? 0 : 1;
}
