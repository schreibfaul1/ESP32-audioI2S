#pragma once

// AF-2.6A. Decisions for destroying a decoder that the audio task may be using.
// Audio.cpp applies these results to the real FreeRTOS mutex. This header does
// not create tasks and does not take a semaphore.

#include <atomic>
#include <cstdint>

namespace audiolib {

enum class DecoderCtrl : uint8_t {
    None = 0,
    Reconnect = 1, // MP3 NEED_RESTART: stop, then one connecttohost outside the decode mutex
    Stop = 2,      // fatal decode or rejected format; outranks a reconnect
};

inline DecoderCtrl raiseDecoderCtrl(DecoderCtrl current, DecoderCtrl request) {
    return static_cast<uint8_t>(request) > static_cast<uint8_t>(current) ? request : current;
}

// Monotonic raise. Stop can never be replaced by Reconnect or None through this path.
inline void requestDecoderCtrl(std::atomic<uint8_t>& slot, DecoderCtrl request) {
    uint8_t current = slot.load(std::memory_order_acquire);
    for (;;) {
        const DecoderCtrl next = raiseDecoderCtrl(static_cast<DecoderCtrl>(current), request);
        if (next == static_cast<DecoderCtrl>(current)) return;
        if (slot.compare_exchange_weak(current, static_cast<uint8_t>(next), std::memory_order_acq_rel, std::memory_order_acquire)) return;
    }
}

inline bool decoderCtrlPending(uint8_t raw) {
    return raw != static_cast<uint8_t>(DecoderCtrl::None);
}

// Claim one pending command for service. Returns false if none or lost the CAS.
inline bool claimDecoderCtrl(std::atomic<uint8_t>& slot, uint8_t* claimed_out) {
    uint8_t claimed = slot.load(std::memory_order_acquire);
    if (!decoderCtrlPending(claimed)) return false;
    if (!slot.compare_exchange_strong(claimed, static_cast<uint8_t>(DecoderCtrl::None), std::memory_order_acq_rel, std::memory_order_acquire)) return false;
    *claimed_out = claimed;
    return true;
}

// After a failed owned stop: put the claimed command back without demoting a
// Stop that another task published while the take timed out.
inline void restoreDecoderCtrl(std::atomic<uint8_t>& slot, uint8_t claimed) {
    requestDecoderCtrl(slot, static_cast<DecoderCtrl>(claimed));
}

// After a successful destroy for a claimed command: a late Stop is already
// satisfied by that destroy and is consumed. Reconnect runs only when the
// claim was Reconnect and no Stop is pending.
inline bool reconnectAfterOwnedStop(std::atomic<uint8_t>& slot, uint8_t claimed) {
    uint8_t late = slot.load(std::memory_order_acquire);
    if (late == static_cast<uint8_t>(DecoderCtrl::Stop)) {
        uint8_t expected = static_cast<uint8_t>(DecoderCtrl::Stop);
        slot.compare_exchange_strong(expected, static_cast<uint8_t>(DecoderCtrl::None), std::memory_order_acq_rel, std::memory_order_acquire);
        return false;
    }
    return claimed == static_cast<uint8_t>(DecoderCtrl::Reconnect);
}

// Stale NEED_RESTART after the user started a new stream (setDefaults bumps epoch).
inline bool reconnectEpochCurrent(uint32_t published_epoch, uint32_t stream_epoch) {
    return published_epoch == stream_epoch;
}

// Result of xSemaphoreTake, not of the constant pdTRUE.
struct MutexStop {
    bool        proceed; // take succeeded: caller may change decoder state, destroy, and give
    bool        give;    // give only when this call owns the mutex
    DecoderCtrl publish; // published when the take failed; None when the caller proceeds
};

inline MutexStop mutexStop(bool take_succeeded) {
    if (!take_succeeded) return {false, false, DecoderCtrl::Stop};
    return {true, true, DecoderCtrl::None};
}

// A decode section that already failed the take must not give.
// A take that won, then observed a pending stop, must give and must not decode.
struct DecodeGate {
    bool enter;
    bool give;
};

inline DecodeGate decodeGate(bool take_succeeded, bool stop_pending) {
    if (!take_succeeded) return {false, false};
    if (stop_pending) return {false, true};
    return {true, false}; // caller gives when the section ends
}

struct FaultAction {
    DecoderCtrl publish;
    bool        call_stop_now;
    bool        call_connect_now;
    bool        keep_using_decoder;
};

// fatal is decode result -100. need_restart is MP3_NEED_RESTART.
// Neither may call stopSong or connecttohost while the decode mutex is held.
inline FaultAction deferredFault(bool fatal, bool need_restart) {
    if (fatal) return {DecoderCtrl::Stop, false, false, false};
    if (need_restart) return {DecoderCtrl::Reconnect, false, false, false};
    return {DecoderCtrl::None, false, false, true};
}

inline bool sampleFormatUsable(int channels, int bits_per_sample) {
    const bool bits_ok = bits_per_sample == 8 || bits_per_sample == 16 || bits_per_sample == 24 || bits_per_sample == 32;
    const bool channels_ok = channels == 1 || channels == 2;
    return bits_ok && channels_ok;
}

} // namespace audiolib
