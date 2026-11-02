#pragma once
#include <Arduino.h>

#ifdef CONFIG_IDF_TARGET_ESP32S31
#include "../buffers.hpp"
#include "../audiolib_structs.hpp"
#include "../psram_unique_ptr.hpp"
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_bt_api.h"
#include <atomic>

using bt_info_callback_t = bool (*)(void* context, uint16_t event, ps_ptr<char>&& message);
class BT_Source {

    RingBuffer bt_samplesBuffer;

  public:
    BT_Source(void* context = nullptr, bt_info_callback_t callback = nullptr) : m_infoContext(context), m_infoCallback(callback) {}
    ~BT_Source() { s_instance = nullptr; };

  private:
    //  using bt_info_callback_t = bool (*)(event_t e, const char* fmt, ...);
    struct SBC_Config {
        uint8_t sampleRate;
        uint8_t channelMode;
        uint8_t blockLength;
        uint8_t subbands;
        uint8_t allocation;
        uint8_t minBitpool;
        uint8_t maxBitpool;
    };

    SBC_Config m_sbcConfig{};

    bool   m_isInit = false;
    size_t m_bt_buffSize = 32768;

    static BT_Source*  s_instance;
    ps_ptr<int32_t>    m_workBuff;
    ps_ptr<char>       m_devName;
    bool               m_controllerInitialized = false;
    esp_bd_addr_t      m_targetBda = {0};
    bool               m_deviceFound = false;
    std::atomic_bool   m_connected{false};
    bool               m_a2dpInitialized = false;
    bool               test = false;
    void*              m_infoContext = nullptr;
    bt_info_callback_t m_infoCallback = nullptr;

  public:
    bool init(ps_ptr<char> devName);
    void reset();
    bool is_init() { return m_isInit; }
    bool connect();
    bool connected() { return m_connected.load(std::memory_order_acquire); }
    bool getChunk(int32_t* chunk, size_t size);



  private:
    bool           startScan();
    static void    gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    static void    a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);
    static int32_t get_data(uint8_t* data, int32_t bytes);

    template <typename... Args> bool info(uint16_t event, const char* fmt, Args&&... args) {
        if (!m_infoCallback) return false;
        ps_ptr<char> result;
        result.assignf(fmt, std::forward<Args>(args)...);
        if (!result.get()) return false;
        return m_infoCallback(m_infoContext, event, std::move(result));
    }
};

#else // dummy

#include "../psram_unique_ptr.hpp"

using bt_info_callback_t = bool (*)(void*, uint16_t, ps_ptr<char>&&);

class BT_Source {
  public:
    BT_Source(void* = nullptr, bt_info_callback_t = nullptr) {}

    bool init(ps_ptr<char>) { return false; }
    void reset() {}
    bool is_init() { return false; }
    bool connect() { return false; }
    bool connected() { return false; }
    bool getChunk(int32_t*, size_t) { return false; }
};

#endif
