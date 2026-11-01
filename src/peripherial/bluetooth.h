#pragma once

#include "BluetoothA2DPCommon.h"
#include "BluetoothA2DPSource.h"
#include "buffers.hpp"
#include "psram_unique_ptr.hpp"
#include <Arduino.h>

class BT_Source {

    RingBuffer          bt_samplesBuffer;
    BluetoothA2DPSource a2dp_source;

  public:
    BT_Source() { ; };
    ~BT_Source() { s_instance = nullptr; };

  private:
    bool              m_isInit = false;
    size_t            m_bt_buffSize = 8192 * 4;
    ps_ptr<char>      m_devName;
    static BT_Source* s_instance;
    ps_ptr<int32_t>   m_workBuff;

  public:
    bool init(ps_ptr<char> devName);
    void reset();
    bool is_init() { return m_isInit; }
    bool connected() { return a2dp_source.is_connected(); }
    bool getChunk(int32_t* chunk, size_t size);

  private:
    static int32_t bt_data_callback(uint8_t* data, int32_t len) {
        if (!s_instance) return 0;
        return s_instance->get_data(data, len);
    }
    int32_t get_data(uint8_t* data, int32_t bytes);
};
