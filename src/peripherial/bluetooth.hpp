#pragma once

#include "buffers.hpp"
#include "psram_unique_ptr.hpp"
#include <Arduino.h>

class BT_Source {

    RingBuffer bt_samplesBuffer;

  public:
    BT_Source() { ; };
    ~BT_Source() { ; };

  private:
    bool   m_isInit = false;
    size_t m_bt_buffSize = 8192;
    ps_ptr<char> m_devName;
    size_t m_workBuffSize = 1024;
    ps_ptr<int32_t> m_workBuff;

  public:
    bool init(ps_ptr<char> devName) {
        if (bt_samplesBuffer.isInitialized()) { bt_samplesBuffer.reset(); }
        m_devName = devName;
        bt_samplesBuffer.setBufsize(m_bt_buffSize);
        size_t buffSize = bt_samplesBuffer.init();
        m_workBuff.alloc_array(m_workBuffSize, "m_workBuff");
        if (buffSize == m_bt_buffSize && m_workBuffSize == m_workBuff.size()) {
            m_isInit = true;
            return m_isInit;
        }
        return false;
    }

    void reset() {
        if(bt_samplesBuffer.isInitialized()) bt_samplesBuffer.reset();
        m_isInit = false;
    }

    bool is_init(){
      return m_isInit;
    }

    bool connected(){
        // return false;
        return m_isInit;
    }

    bool playChunk(int32_t* chunk, size_t size){
        size_t written = 0, was_read = 0;
        if(size <= bt_samplesBuffer.freeSpace()){
            written = bt_samplesBuffer.write(chunk, size);
        }
        else{
            log_e("not enough space, size %lu, free %lu", size, bt_samplesBuffer.freeSpace());
        }
        was_read = bt_samplesBuffer.peek(m_workBuff.get(), (size_t)1024);
        log_i("written %lu, read %li", written, was_read);
        bt_samplesBuffer.bytesRead(was_read);
        return true;
    }
};