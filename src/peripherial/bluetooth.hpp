#pragma once

#include <Arduino.h>
#include "buffers.hpp"
#include "psram_unique_ptr.hpp"
#include "BluetoothA2DPCommon.h"
#include "BluetoothA2DPSource.h"

class BT_Source {

    RingBuffer bt_samplesBuffer;
    BluetoothA2DPSource a2dp_source;

  public:
    BT_Source() { ; };
    ~BT_Source() { ; };

  private:
    bool            m_isInit = false;
    size_t          m_bt_buffSize = 8192;
    ps_ptr<char>    m_devName;

  public:
    bool init(ps_ptr<char> devName) {
        if (bt_samplesBuffer.isInitialized()) { bt_samplesBuffer.reset(); }
        m_devName = devName;
        bt_samplesBuffer.setBufsize(m_bt_buffSize);
        size_t buffSize = bt_samplesBuffer.init();
        if (buffSize == m_bt_buffSize) {
            a2dp_source.set_data_callback(get_data);
            a2dp_source.start(m_devName.c_get());
            m_isInit = true;
            return m_isInit;
        }
        return false;
    }

    void reset() {
        if (bt_samplesBuffer.isInitialized()) bt_samplesBuffer.reset();
        m_isInit = false;
    }

    bool is_init() { return m_isInit; }

    bool connected() {
        // return false;
        return m_isInit;
    }

    bool getChunk(int32_t* chunk, size_t size) {
        size_t written = 0;
        if (size <= bt_samplesBuffer.freeSpace()) {
            written = bt_samplesBuffer.write(chunk, size);
            if(written != size) {
                log_e("buffer write error size: %lu, written: %lu", size, written);
                return false;
            }
        } else {
            log_e("not enough space, size %lu, free %lu", size, bt_samplesBuffer.freeSpace());
            return false;
        }
        return true;
    }

    //---------------------------------------------CallBacks--------------------------------------------------------------------

    int32_t get_data(uint8_t* data, int32_t bytes) {
        size_t was_read =0;
        if(bytes < bt_samplesBuffer.bufferFilled() * 2){
            log_e("not enough data in bt_samplesBuffer, buffer filled: %lu, bytes %li",  bt_samplesBuffer.bufferFilled(), bytes);
        }
        else{
            was_read = bt_samplesBuffer.peek8(data, (size_t)bytes);
            if(was_read == bytes){
                bt_samplesBuffer.bytesRead(was_read / 2);
                return was_read;
            }
        }
        return 0;
    }
};