#pragma once
// Host stand-in for src/Audio.h. The FLAC capacity test compiles the real
// flac_decoder.cpp against this file. It is not part of the firmware build.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>

using boolean = bool;

template <typename T> T min(T a, T b) {
    return a < b ? a : b;
}

template <typename T> class ps_ptr {
  public:
    ps_ptr() = default;
    ps_ptr(const ps_ptr&) = delete;
    ps_ptr& operator=(const ps_ptr&) = delete;
    ps_ptr(ps_ptr&& other) noexcept : ptr_(other.ptr_), count_(other.count_) {
        other.ptr_ = nullptr;
        other.count_ = 0;
    }
    ps_ptr& operator=(ps_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            count_ = other.count_;
            other.ptr_ = nullptr;
            other.count_ = 0;
        }
        return *this;
    }
    ~ps_ptr() { reset(); }

    bool alloc_array(size_t count, const char*) {
        reset();
        ptr_ = new T[count]();
        count_ = count;
        return ptr_ != nullptr;
    }
    bool calloc_array(size_t count, const char* name) { return alloc_array(count, name); }
    bool valid() const { return ptr_ != nullptr; }
    explicit operator bool() const { return ptr_ != nullptr; }
    bool alloc(size_t count, const char* name = nullptr, bool = true) { return alloc_array(count, name); }
    void copy_from(const char* src, size_t count, bool = true) { assign(src, static_cast<uint32_t>(count)); }
    void reset() {
        delete[] ptr_;
        ptr_ = nullptr;
        count_ = 0;
    }
    void clear() {
        if (ptr_ != nullptr) { std::memset(ptr_, 0, count_ * sizeof(T)); }
    }
    void zero_mem() { clear(); }
    T* get() { return ptr_; }
    const T* get() const { return ptr_; }
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    T& operator[](size_t index) { return ptr_[index]; }
    const T& operator[](size_t index) const { return ptr_[index]; }
    size_t size() const { return count_; }
    const char* c_get() const { return ptr_ == nullptr ? "" : reinterpret_cast<const char*>(ptr_); }
    void assign(const char* text) { assign(text, text == nullptr ? 0 : static_cast<uint32_t>(std::strlen(text))); }
    void assign(const char* text, uint32_t length) {
        reset();
        ptr_ = new T[length + 1]();
        count_ = length + 1;
        if (text != nullptr && length != 0) { std::memcpy(ptr_, text, length); }
    }
    void append(const char* text) {
        if (text == nullptr) { return; }
        std::string joined = c_get();
        joined += text;
        assign(joined.c_str());
    }

  private:
    T* ptr_ = nullptr;
    size_t count_ = 0;
};

class Audio {
  public:
    enum { evt_id3data = 1 };

    template <typename... Args>
    static void AUDIO_LOG_IMPL(uint8_t level, const char* path, int line, const char* func, const char* fmt, Args&&...) {
        if (level == 1) { std::fprintf(stderr, "FLAC %s:%d %s %s\n", path, line, func, fmt); }
    }

    template <typename... Args> void info(Audio&, int, const char*, Args&&...) {}
};

class Decoder {
  public:
    virtual ~Decoder() = default;
    virtual bool init() = 0;
    virtual void clear() = 0;
    virtual void reset() = 0;
    virtual bool isValid() = 0;
    virtual int32_t findSyncWord(uint8_t* buf, int32_t nBytes) = 0;
    virtual uint8_t getChannels() = 0;
    virtual uint32_t getSampleRate() = 0;
    virtual uint8_t getBitsPerSample() = 0;
    virtual uint32_t getBitRate() = 0;
    virtual uint32_t getAudioDataStart() = 0;
    virtual uint32_t getAudioFileDuration() = 0;
    virtual uint32_t getOutputSamples() = 0;
    virtual int32_t decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf1) = 0;
    virtual void setRawBlockParams(uint8_t param1, uint32_t param2, uint8_t param3, uint32_t param4, uint32_t param5) = 0;
    virtual const char* getStreamTitle() = 0;
    virtual const char* whoIsIt() = 0;
    virtual std::vector<uint32_t> getMetadataBlockPicture() = 0;
    virtual const char* arg1() = 0;
    virtual const char* arg2() = 0;
    virtual int32_t val1() = 0;
    virtual int32_t val2() = 0;

  protected:
    explicit Decoder(Audio& audioRef) : audio(audioRef) {}
    Audio& audio;

  private:
    Decoder() = delete;
};
