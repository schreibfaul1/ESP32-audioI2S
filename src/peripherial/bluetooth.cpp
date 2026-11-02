
#include "bluetooth.h"

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

BT_Source* BT_Source::s_instance = nullptr;

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  B L U E T O O T H  📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void BT_Source::gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {

    switch (event) {

        case ESP_BT_GAP_DISC_RES_EVT: {
            uint8_t* dev_name = nullptr;
            uint8_t  name_len = 0;
            uint8_t* eir = nullptr;

            for (int i = 0; i < param->disc_res.num_prop; i++) {
                if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR) {
                    eir = (uint8_t*)param->disc_res.prop[i].val;
                    break;
                }
            }

            if (eir) {
                dev_name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &name_len);
                if (!dev_name) { dev_name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &name_len); }
            }
            if (!dev_name || name_len == 0) break;

            // Clean up device names from EIR data
            size_t len = name_len;
            while (len > 0 && (dev_name[len - 1] == '\n' || dev_name[len - 1] == '\r' || dev_name[len - 1] == '\0')) { len--; }

            // compare mames
            if (s_instance && len == strlen(s_instance->m_devName.c_get()) && memcmp(dev_name, s_instance->m_devName.c_get(), len) == 0) {
                memcpy(s_instance->m_targetBda, param->disc_res.bda, sizeof(esp_bd_addr_t));
                s_instance->m_deviceFound = true;
                log_i("Target device found: '%s'", s_instance->m_devName.c_get());
                esp_bt_gap_cancel_discovery();
            }
            break;
        }

        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                log_i("GAP: Discovery started");
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                log_i("GAP: Discovery stopped");

                if (s_instance && s_instance->m_deviceFound) {
                    log_i("Target device MAC: %02X:%02X:%02X:%02X:%02X:%02X", s_instance->m_targetBda[0], s_instance->m_targetBda[1], s_instance->m_targetBda[2], s_instance->m_targetBda[3],
                        s_instance->m_targetBda[4], s_instance->m_targetBda[5]);
                    s_instance->connect();
                }
            }
            break;
        default: break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void BT_Source::a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {

    switch (event) {

        case ESP_A2D_PROF_STATE_EVT:

            if (param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS) {
                if(s_instance) s_instance->m_a2dpInitialized = true;
            } else if (param->a2d_prof_stat.init_state == ESP_A2D_DEINIT_SUCCESS) {
                if(s_instance) s_instance->m_a2dpInitialized = false;
                log_i("A2DP: Source deinitialized");
            }
            break;

        case ESP_A2D_CONNECTION_STATE_EVT:

            switch (param->conn_stat.state) {

                case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
                    if(s_instance) s_instance->m_connected = false;
                    log_i("A2DP: Disconnected");
                    break;

                case ESP_A2D_CONNECTION_STATE_CONNECTING: log_i("A2DP: Connecting"); break;

                case ESP_A2D_CONNECTION_STATE_CONNECTED:
                    if(s_instance) s_instance->m_connected = true;
                    log_i("A2DP: Connected");
                    break;

                case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
                    if(s_instance) s_instance->m_connected = false;
                    log_i("A2DP: Disconnecting");
                    break;

                default: break;
            }
            break;

        default: log_i("A2DP: Event %d", event); break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

bool BT_Source::init(ps_ptr<char> devName) {
    m_devName = devName;
    s_instance = this;
    // Configure the Bluetooth Controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    // Initialize the controller
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        log_e("BT Controller init failed: %s", esp_err_to_name(err));
        return false;
    }
    log_i("BT Controller initialized");
    m_isInit = true;

    // Enable Bluetooth Classic
    err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK) {
        log_e("BT Controller enable failed: %s", esp_err_to_name(err));
        esp_bt_controller_deinit();
        return false;
    }
    log_i("BT Controller enabled");

    // Initialize Bluedroid
    err = esp_bluedroid_init();
    if (err != ESP_OK) {
        log_e("Bluedroid init failed: %s", esp_err_to_name(err));
        return false;
    }
    log_i("Bluedroid initialized");

    // Activate Bluedroid
    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        log_e("Bluedroid enable failed: %s", esp_err_to_name(err));
        return false;
    }
    log_i("Bluedroid enabled");

    // tegister GAP Callback
    err = esp_bt_gap_register_callback(gap_callback);
    if (err != ESP_OK) {
        log_e("GAP callback registration failed: %s", esp_err_to_name(err));
        return false;
    }
    log_i("GAP callback registered");
    startScan();

    // register A2DP Callback
    err = esp_a2d_register_callback(a2dp_callback);
    if (err != ESP_OK) {
        log_e("A2DP callback registration failed: %s", esp_err_to_name(err));
        return false;
    }
    log_i("A2DP callback registered");

    // initialisize A2DP Source
    err = esp_a2d_source_init();
    if (err != ESP_OK) {
        log_e("A2DP source init failed: %s", esp_err_to_name(err));
        return false;
    }
    log_i("A2DP source initialization requested");
    m_controllerInitialized = true;
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::startScan() {

    esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);

    if (err != ESP_OK) {
        log_e("GAP discovery failed: %s", esp_err_to_name(err));
        return false;
    }

    log_i("GAP discovery started");
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void BT_Source::reset() {

    if (!m_controllerInitialized) return;

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) { esp_bluedroid_disable(); }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) { esp_bluedroid_deinit(); }

    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    m_controllerInitialized = false;

    log_i("Bluetooth stopped");
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::getChunk(int32_t* chunk, size_t size) {
    // size_t written = 0;
    // if (size <= bt_samplesBuffer.freeSpace()) {
    //     written = bt_samplesBuffer.write(chunk, size);
    //     if (written != size) {
    //         log_e("buffer write error size: %lu, written: %lu", size, written);
    //         return false;
    //     }
    // } else {
    //     // log_e("not enough space, size %lu, free %lu", size, bt_samplesBuffer.freeSpace());
    //     return false;
    // }
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// int32_t BT_Source::get_data(uint8_t* data, int32_t bytes) { // CallBack

//     // Special case: Flush request
//     if (bytes == -1) { return 0; }

//     if (!data || bytes <= 0) return 0;

//     size_t filled = bt_samplesBuffer.bufferFilled();
//     size_t free = bt_samplesBuffer.freeSpace();
//     size_t ws = bt_samplesBuffer.writeSpace();
//     (void)filled;
//     (void)free;
//     (void)ws;

//     size_t words = bt_samplesBuffer.peek_words(m_workBuff.get(), bytes / 2);
//     //   log_e("words %lu, filled %lu, free %lu, ws %lu, bytes %lu", words, filled, free, ws, bytes);
//     for (int i = 0; i < words; i++) {

//         data[i * 2 + 1] = (m_workBuff[i] & 0xFF000000) >> 24;
//         data[i * 2] = (m_workBuff[i] & 0x00FF0000) >> 16;
//     }
//     bt_samplesBuffer.wordsConsumed(words);
//     return bytes;
// }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::connect() {

    if (!m_a2dpInitialized) {
        log_e("A2DP not initialized");
        return false;
    }

    if (!m_deviceFound) {
        log_e("Target device not found");
        return false;
    }

    esp_err_t err = esp_a2d_source_connect(m_targetBda);

    if (err != ESP_OK) {
        log_e("A2DP connect failed: %s", esp_err_to_name(err));
        return false;
    }

    log_i("A2DP connection requested");

    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
