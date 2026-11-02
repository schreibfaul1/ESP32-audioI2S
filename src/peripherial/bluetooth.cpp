#include "Arduino.h"
#ifdef CONFIG_IDF_TARGET_ESP32S31
#include "bluetooth.h"
#include"../Audio.h"



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
                s_instance->info(Audio::evt_bt, "Target device found: '{}'", s_instance->m_devName);
                esp_bt_gap_cancel_discovery();
            }
            break;
        }

        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                s_instance->info(Audio::evt_bt, "GAP: Discovery started");
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                s_instance->info(Audio::evt_bt, "GAP: Discovery stopped");
                if (s_instance && s_instance->m_deviceFound) {
                    s_instance->info(Audio::evt_bt, "Target device MAC: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", s_instance->m_targetBda[0], s_instance->m_targetBda[1],
                                     s_instance->m_targetBda[2], s_instance->m_targetBda[3], s_instance->m_targetBda[4], s_instance->m_targetBda[5]);
                    s_instance->connect();
                }
            }
            break;
        default: break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
static void log_codec_info(const char* source, const esp_a2d_mcc_t& mcc) {

    log_d("%s: codec type %u", source, mcc.type);

    switch (mcc.type) {

        case ESP_A2D_MCT_SBC: {

            const auto& sbc = mcc.cie.sbc_info;
            (void)sbc;

            log_d("SBC sampling frequency mask: 0x%X", sbc.samp_freq);
            log_d("SBC channel mode mask:       0x%X", sbc.ch_mode);
            log_d("SBC block length mask:       0x%X", sbc.block_len);
            log_d("SBC subbands mask:           0x%X", sbc.num_subbands);
            log_d("SBC allocation method:       0x%X", sbc.alloc_mthd);
            log_d("SBC bitpool min: %u, max: %u", sbc.min_bitpool, sbc.max_bitpool);

            break;
        }

        case ESP_A2D_MCT_M24: log_d("AAC codec"); break;

        case ESP_A2D_MCT_M12: log_d("MPEG Audio codec"); break;

        default: log_d("Other codec"); break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void BT_Source::a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {

    switch (event) {

        case ESP_A2D_CONNECTION_STATE_EVT:

            switch (param->conn_stat.state) {

                case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
                    if (s_instance) s_instance->m_connected.store(false, std::memory_order_release);
                    s_instance->info(Audio::evt_bt, "A2DP: Disconnected");
                    break;

                case ESP_A2D_CONNECTION_STATE_CONNECTING: s_instance->info(Audio::evt_bt, "A2DP: Connecting"); break;

                case ESP_A2D_CONNECTION_STATE_CONNECTED: {
                    if (s_instance) s_instance->m_connected.store(true, std::memory_order_release);
                    s_instance->info(Audio::evt_bt, "A2DP: Connected");
                    esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);

                    if (err != ESP_OK) { log_e("A2DP CHECK_SRC_RDY failed: %s", esp_err_to_name(err)); }
                    break;
                }
                case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
                    if (s_instance) s_instance->m_connected.store(false, std::memory_order_release);
                    s_instance->info(Audio::evt_bt, "A2DP: Disconnecting");
                    break;

                default: break;
            }
            break;

        case ESP_A2D_AUDIO_CFG_EVT: {

            const auto& sbc = param->audio_cfg.mcc.cie.sbc_info;

            s_instance->m_sbcConfig.sampleRate = sbc.samp_freq;
            s_instance->m_sbcConfig.channelMode = sbc.ch_mode;
            s_instance->m_sbcConfig.blockLength = sbc.block_len;
            s_instance->m_sbcConfig.subbands = sbc.num_subbands;
            s_instance->m_sbcConfig.allocation = sbc.alloc_mthd;
            s_instance->m_sbcConfig.minBitpool = sbc.min_bitpool;
            s_instance->m_sbcConfig.maxBitpool = sbc.max_bitpool;

            log_codec_info("AUDIO_CFG", param->audio_cfg.mcc);

            break;
        }
        case ESP_A2D_PROF_STATE_EVT:

            if (param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS) {
                if (s_instance) s_instance->m_a2dpInitialized = true;
            } else if (param->a2d_prof_stat.init_state == ESP_A2D_DEINIT_SUCCESS) {
                if (s_instance) s_instance->m_a2dpInitialized = false;
                s_instance->info(Audio::evt_bt, "A2DP: Source deinitialized");
            }
            break;

        case ESP_A2D_REPORT_SNK_CODEC_CAPS_EVT: {

            s_instance->info(Audio::evt_bt, "A2DP: REPORT_SNK_CODEC_CAPS_EVT");
            log_codec_info("Selected sink codec capabilities", param->a2d_report_snk_codec_caps_stat.mcc);
            break;
        }

        case ESP_A2D_REPORT_SNK_ALL_CODEC_CAPS_EVT: {

            auto& caps = param->a2d_report_snk_all_codec_caps_stat;
            s_instance->info(Audio::evt_bt, "A2DP: REPORT_SNK_ALL_CODEC_CAPS_EVT");
            s_instance->info(Audio::evt_bt, "Number of sink endpoints: {}", caps.sep_num);

            if (caps.sep_mcc) {
                for (int i = 0; i < caps.sep_num; i++) {
                    s_instance->info(Audio::evt_bt, "SEP {}:", caps.sep_mcc[i].seid);
                    log_codec_info("SEP codec capabilities", caps.sep_mcc[i].mcc);
                }
            }
            break;
        }

        case ESP_A2D_MEDIA_CTRL_ACK_EVT: {

            auto& stat = param->media_ctrl_stat;

            s_instance->info(Audio::evt_bt, "A2DP MEDIA_CTRL_ACK: cmd={} status={}", stat.cmd, stat.status);

            if (stat.status != ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                log_e("A2DP media control failed");
                break;
            }

            if (stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {

                s_instance->info(Audio::evt_bt, "A2DP source ready, starting media");

                esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);

                if (err != ESP_OK) { log_e("A2DP START failed: %s", esp_err_to_name(err)); }
            }

            if (stat.cmd == ESP_A2D_MEDIA_CTRL_START) { s_instance->info(Audio::evt_bt, "A2DP media start acknowledged"); }

            break;
        }

        case ESP_A2D_AUDIO_STATE_EVT: {

            auto state = param->audio_stat.state;
            s_instance->info(Audio::evt_bt, "A2DP AUDIO_STATE: {}", state);
            if (state == ESP_A2D_AUDIO_STATE_STARTED) { s_instance->info(Audio::evt_bt, "A2DP audio transmission started!"); }

            break;
        }

        default: s_instance->info(Audio::evt_bt, "A2DP: Event {}", event); break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

bool BT_Source::init(ps_ptr<char> devName) {
    m_devName = devName;
    bt_samplesBuffer.setBufsize(m_bt_buffSize);
    size_t buffSize = bt_samplesBuffer.init();
    if (buffSize == m_bt_buffSize) {
        m_workBuff.alloc_array(1024);
        s_instance = this;
    }
    // Configure the Bluetooth Controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    // Initialize the controller
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        log_e("BT Controller init failed: %s", esp_err_to_name(err));
        return false;
    }
    s_instance->info(Audio::evt_bt, "BT Controller initialized");
    m_isInit = true;

    // Enable Bluetooth Classic
    err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK) {
        log_e("BT Controller enable failed: %s", esp_err_to_name(err));
        esp_bt_controller_deinit();
        return false;
    }
    info(Audio::evt_bt, "BT Controller enabled");

    // Initialize Bluedroid
    err = esp_bluedroid_init();
    if (err != ESP_OK) {
        log_e("Bluedroid init failed: %s", esp_err_to_name(err));
        return false;
    }
    s_instance->info(Audio::evt_bt, "Bluedroid initialized");
    info(Audio::evt_bt, "Bluedroid initialized {}", 4.6);

    // Activate Bluedroid
    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        log_e("Bluedroid enable failed: %s", esp_err_to_name(err));
        return false;
    }
    info(Audio::evt_bt, "Bluedroid enabled");

    // tegister GAP Callback
    err = esp_bt_gap_register_callback(gap_callback);
    if (err != ESP_OK) {
        log_e("GAP callback registration failed: %s", esp_err_to_name(err));
        return false;
    }
    info(Audio::evt_bt, "GAP callback registered");
    startScan();

    // register A2DP Callback
    err = esp_a2d_register_callback(a2dp_callback);
    if (err != ESP_OK) {
        log_e("A2DP callback registration failed: %s", esp_err_to_name(err));
        return false;
    }
    info(Audio::evt_bt, "A2DP callback registered");

    // initialisize A2DP Source
    err = esp_a2d_source_init();
    if (err != ESP_OK) {
        log_e("A2DP source init failed: %s", esp_err_to_name(err));
        return false;
    }
    info(Audio::evt_bt, "A2DP source initialization requested");
    m_controllerInitialized = true;

    err = esp_a2d_source_register_data_callback(BT_Source::get_data);
    info(Audio::evt_bt, "err {}", err);
    if (err != ESP_OK) {
        log_e("A2DP data callback registration failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::startScan() {

    esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);

    if (err != ESP_OK) {
        log_e("GAP discovery failed: %s", esp_err_to_name(err));
        return false;
    }

    info(Audio::evt_bt, "GAP discovery started");
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void BT_Source::reset() {

    if (!m_controllerInitialized) return;

    m_connected = false;
    m_controllerInitialized = false;

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) { esp_bluedroid_disable(); }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) { esp_bluedroid_deinit(); }

    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    info(Audio::evt_bt, "Bluetooth stopped");
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::getChunk(int32_t* chunk, size_t size) {
    size_t written = 0;
    if (size <= bt_samplesBuffer.freeSpace()) {
        written = bt_samplesBuffer.write(chunk, size);
        if (written != size) {
            log_e("buffer write error size: %lu, written: %lu", size, written);
            return false;
        }
    } else {
        log_d("not enough space, size %lu, free %lu", size, bt_samplesBuffer.freeSpace());
        return false;
    }
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t BT_Source::get_data(uint8_t* data, int32_t bytes) {

    // Flush request
    if (bytes == -1) return 0;

    if (!data || bytes <= 0) return 0;

    // Default: silence
    memset(data, 0, bytes);

    if (!s_instance) return bytes;

    // statistics
    static uint32_t cbCount = 0;
    size_t          filled = s_instance->bt_samplesBuffer.bufferFilled();
    (void)filled;
    size_t words = s_instance->bt_samplesBuffer.peek_words(s_instance->m_workBuff.get(), bytes / 2);
    if (++cbCount % 100 == 0) {
        log_d("BT callback: requested=%d, words=%u, filled=%u", bytes / 2, words, filled);
        if (words > 1) { log_d("PCM samples: 0x%08lX, 0x%08lX", (uint32_t)s_instance->m_workBuff[0], (uint32_t)s_instance->m_workBuff[1]); }
    }

    size_t requestedWords = bytes / 2;
    words = s_instance->bt_samplesBuffer.peek_words(s_instance->m_workBuff.get(), requestedWords);

    for (size_t i = 0; i < words; i++) {
        int32_t sample = s_instance->m_workBuff[i];
        // Convert int32_t to signed 16-bit PCM (little endian)
        data[i * 2] = (sample >> 16) & 0xFF;
        data[i * 2 + 1] = (sample >> 24) & 0xFF;
    }

    s_instance->bt_samplesBuffer.wordsConsumed(words);
    return bytes;
}
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

    info(Audio::evt_bt, "A2DP connection requested");

    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#endif