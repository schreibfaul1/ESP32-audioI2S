#pragma once

#include "buffers.hpp"
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_bt_api.h"
#include "psram_unique_ptr.hpp"
#include <Arduino.h>

class BT_Source {

    RingBuffer bt_samplesBuffer;

  public:
    BT_Source() = default;
    ~BT_Source() { s_instance = nullptr; };

  private:
    bool   m_isInit = false;
    size_t m_bt_buffSize = 8192 * 4;

    static BT_Source* s_instance;
    ps_ptr<int32_t>   m_workBuff;
    ps_ptr<char>      m_devName;
    bool              m_controllerInitialized = false;
    esp_bd_addr_t     m_targetBda = {0};
    bool              m_deviceFound = false;
    bool              m_connected = false;
    bool              m_a2dpInitialized = false;
    bool test = false;

  public:
    bool init(ps_ptr<char> devName);
    void reset();
    bool is_init() { return m_isInit; }
    bool connect();
    bool connected() { return m_connected;; }
    bool getChunk(int32_t* chunk, size_t size);

  private:
    bool startScan();
    static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    static void a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);
};
