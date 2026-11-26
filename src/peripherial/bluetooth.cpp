/*
 * bluetooth.cpp
 *
 *  Created on: 30.10.2026
 *  Updated on: 16.11.2026
 */

#if (defined(CONFIG_IDF_TARGET_ESP32S31) && ESP_IDF_VERSION_MAJOR >= 6)
    #include "../Audio.h"
    #include "bluetooth.h"
    #include "nvs.h"
    #include "nvs_flash.h"
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

BT_Source* BT_Source::s_instance = nullptr;

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  B L U E T O O T H  📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void BT_Source::gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {

    switch (event) {

        case ESP_BT_GAP_DISC_RES_EVT: {
            uint8_t*     name = nullptr;
            uint8_t      name_len = 0;
            uint8_t*     eir = nullptr;
            ps_ptr<char> devName;

            for (int i = 0; i < param->disc_res.num_prop; i++) {
                if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR) {
                    eir = (uint8_t*)param->disc_res.prop[i].val;
                    break;
                }
            }

            if (eir) {
                name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &name_len);
                if (!name) { name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &name_len); }
            }
            if (!name || name_len == 0) break;

            // Clean up device names from EIR data
            size_t len = name_len;
            while (len > 0 && (name[len - 1] == '\n' || name[len - 1] == '\r' || name[len - 1] == '\0')) { len--; }
            devName.copy_from((char*)name, len);
            s_instance->info(Audio::evt_bt, "BT device found: '{}'", devName);

            // compare mames
            if (devName == s_instance->m_devName) {
                // if (s_instance && len == strlen(s_instance->m_devName.c_get()) && memcmp(name, s_instance->m_devName.c_get(), len) == 0) {
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
                if (!s_instance || s_instance->m_stopping.load(std::memory_order_acquire)) { break; }
                if (s_instance->m_deviceFound && s_instance->m_a2dpInitialized.load(std::memory_order_acquire)) {
                    s_instance->connect();
                } else if (s_instance->m_isInit) {
                    // No device found: Start the next scan
                    s_instance->startScan();
                }
            }
            break;
        default: break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
static void log_codec_info(const char* source, const esp_a2d_mcc_t& mcc) {

    // printf("[%s:%i]" ANSI_ESC_MAGENTA " %s: codec type %u" ANSI_ESC_RESET "\n", __FILE__, __LINE__, source, mcc.type);

    switch (mcc.type) {

        case ESP_A2D_MCT_SBC: {

            const auto& sbc = mcc.cie.sbc_info;
            (void)sbc;

            // printf("[%s:%i]" ANSI_ESC_MAGENTA " SBC sampling frequency mask: 0x%X" ANSI_ESC_RESET "\n", __FILE__, __LINE__, sbc.samp_freq);
            // printf("[%s:%i]" ANSI_ESC_MAGENTA " SBC channel mode mask:       0x%X" ANSI_ESC_RESET "\n", __FILE__, __LINE__, sbc.ch_mode);
            // printf("[%s:%i]" ANSI_ESC_MAGENTA " SBC block length mask:       0x%X" ANSI_ESC_RESET "\n", __FILE__, __LINE__, sbc.block_len);
            // printf("[%s:%i]" ANSI_ESC_MAGENTA " SBC subbands mask:           0x%X" ANSI_ESC_RESET "\n", __FILE__, __LINE__, sbc.num_subbands);
            // printf("[%s:%i]" ANSI_ESC_MAGENTA " SBC allocation method:       0x%X" ANSI_ESC_RESET "\n", __FILE__, __LINE__, sbc.alloc_mthd);
            // printf("[%s:%i]" ANSI_ESC_MAGENTA " SBC bitpool min: %u, max: %u" ANSI_ESC_RESET "\n", __FILE__, __LINE__, sbc.min_bitpool, sbc.max_bitpool);

            break;
        }

        case ESP_A2D_MCT_M24: /* printf("[%s:%i]" ANSI_ESC_MAGENTA " AAC codec" ANSI_ESC_RESET "\n", __FILE__, __LINE__); */ break;

        case ESP_A2D_MCT_M12: /* printf("[%s:%i]" ANSI_ESC_MAGENTA " MPEG Audio codec" ANSI_ESC_RESET "\n", __FILE__, __LINE__); */ break;

        default: /* printf("[%s:%i]" ANSI_ESC_MAGENTA " Other codec" ANSI_ESC_RESET "\n", __FILE__, __LINE__); */ break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void BT_Source::a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {

    switch (event) {

        case ESP_A2D_CONNECTION_STATE_EVT:

            switch (param->conn_stat.state) {

                case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
                    s_instance->m_connected.store(false, std::memory_order_release);
                    xEventGroupSetBits(s_instance->m_btEvents, BT_EVT_DISCONNECTED);
                    s_instance->info(Audio::evt_bt, "A2DP: Disconnected");
                    s_instance->restartDiscoveryIfDisconnected();
                    break;

                case ESP_A2D_CONNECTION_STATE_CONNECTING: s_instance->info(Audio::evt_bt, "A2DP: Connecting"); break;

                case ESP_A2D_CONNECTION_STATE_CONNECTED: {
                    if (s_instance) s_instance->m_connected.store(true, std::memory_order_release);
                    s_instance->info(Audio::evt_bt, "A2DP: Connected");
                    esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);

                    if (err != ESP_OK) { printf("[%s:%i]" ANSI_ESC_RED " A2DP CHECK_SRC_RDY failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err)); }
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
                printf("[%s:%i]" ANSI_ESC_RED " A2DP media control failed" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
                break;
            }

            if (stat.cmd == ESP_A2D_MEDIA_CTRL_SUSPEND && stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) { //
                xEventGroupSetBits(s_instance->m_btEvents, BT_EVT_MEDIA_SUSPEND);
            }

            if (stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
                s_instance->info(Audio::evt_bt, "A2DP source ready, starting media");
                esp_err_t err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                if (err != ESP_OK) { printf("[%s:%i]" ANSI_ESC_RED " A2DP START failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err)); }
            }

            if (stat.cmd == ESP_A2D_MEDIA_CTRL_START) { //
                s_instance->info(Audio::evt_bt, "A2DP media start acknowledged");
            }

            break;
        }

        case ESP_A2D_AUDIO_STATE_EVT: {

            auto state = param->audio_stat.state;
            s_instance->info(Audio::evt_bt, "A2DP AUDIO_STATE: {}", state);
            if (state == ESP_A2D_AUDIO_STATE_STARTED) { s_instance->info(Audio::evt_bt, "A2DP audio transmission started!"); }

            break;
        }

        case ESP_A2D_PROF_STATE_EVT:

            if (param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS) {

                s_instance->m_a2dpInitialized = true;

                xEventGroupSetBits(s_instance->m_btEvents, BT_EVT_A2DP_INIT);
            } else if (param->a2d_prof_stat.init_state == ESP_A2D_DEINIT_SUCCESS) {

                s_instance->m_a2dpInitialized = false;

                s_instance->info(Audio::evt_bt, "A2DP deinitialized");

                xEventGroupSetBits(s_instance->m_btEvents, BT_EVT_A2DP_DEINIT);
            }
            break;

        default: s_instance->info(Audio::evt_bt, "A2DP: Event {}", event); break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void BT_Source::avrc_tg_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param) {
    if (!s_instance || !param) return;

    switch (event) {

        case ESP_AVRC_TG_PROF_STATE_EVT:

            if (param->avrc_tg_init_stat.state == ESP_AVRC_INIT_SUCCESS) {
                s_instance->info(Audio::evt_bt, "AVRCP Target initialized");
                xEventGroupSetBits(s_instance->m_btEvents, BT_EVT_AVRC_INIT);
            } else if (param->avrc_tg_init_stat.state == ESP_AVRC_DEINIT_SUCCESS) {
                s_instance->info(Audio::evt_bt, "AVRCP Target deinitialized");
                xEventGroupSetBits(s_instance->m_btEvents, BT_EVT_AVRC_DEINIT);
            }
            break;

        case ESP_AVRC_TG_CONNECTION_STATE_EVT: {
            const bool connected = param->conn_stat.connected;
            s_instance->m_avrcConnected.store(connected, std::memory_order_release);
            if (connected) {
                xEventGroupClearBits(s_instance->m_btEvents, BT_EVT_AVRC_DISCONNECTED);
            } else {
                xEventGroupSetBits(s_instance->m_btEvents, BT_EVT_AVRC_DISCONNECTED);
            }
            s_instance->info(Audio::evt_bt, "AVRCP connection: {}", connected ? "connected" : "disconnected");
            if (!connected) { s_instance->restartDiscoveryIfDisconnected(); }
            break;
        }

        case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT: {
            const auto& cmd = param->psth_cmd;
            printf("[%s:%i]" ANSI_ESC_YELLOW "cmd.key_code %u" ANSI_ESC_RESET "\n", __FILE__, __LINE__, cmd.key_code);
            // Nur Tastendruck verarbeiten, nicht das Loslassen
            if (cmd.key_state != ESP_AVRC_PT_CMD_STATE_PRESSED) { break; }

            switch (cmd.key_code) {

                case ESP_AVRC_PT_CMD_PLAY: s_instance->info(Audio::evt_bt, "play"); break;

                case ESP_AVRC_PT_CMD_PAUSE: s_instance->info(Audio::evt_bt, "pause"); break;

                case ESP_AVRC_PT_CMD_STOP: s_instance->info(Audio::evt_bt, "stop"); break;

                case ESP_AVRC_PT_CMD_FORWARD: s_instance->info(Audio::evt_bt, "next"); break;

                case ESP_AVRC_PT_CMD_BACKWARD: s_instance->info(Audio::evt_bt, "previous"); break;

                case ESP_AVRC_PT_CMD_FAST_FORWARD: s_instance->info(Audio::evt_bt, "fast_forward"); break;

                case ESP_AVRC_PT_CMD_REWIND: s_instance->info(Audio::evt_bt, "rewind"); break;

                default: s_instance->info(Audio::evt_bt, "unknown AVRCP command: {}", cmd.key_code); break;
            }

            break;
        }

        case ESP_AVRC_TG_REMOTE_FEATURES_EVT: s_instance->info(Audio::evt_bt, "AVRCP remote features received"); break;

        default: break;
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::init(ps_ptr<char> devName) {

    EventBits_t bits = {};

    if (!m_btEvents) {
        printf("[%s:%i]" ANSI_ESC_RED " Bluetooth event group unavailable" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        return false;
    }

    if (m_isInit) {
        printf("[%s:%i]" ANSI_ESC_MAGENTA " Bluetooth already initialized" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        return true;
    }

    s_instance = this;

    m_stopping = false;
    m_connected = false;
    m_deviceFound = false;
    m_a2dpInitialized = false;

    m_devName = devName;

    xEventGroupClearBits(m_btEvents, 0x00FFFFFF);

    info(Audio::evt_bt, "starting Bluetooth, device name: {}", devName);
    vTaskDelay(10);
    // Initialize sample buffer
    bt_samplesBuffer.setBufsize(m_bt_buffSize);

    if (bt_samplesBuffer.init() != m_bt_buffSize) {
        printf("[%s:%i]" ANSI_ESC_RED " Bluetooth sample buffer initialization failed" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        return false;
    }

    m_workBuff.alloc_array(1024);

    esp_err_t err;

    // --------------------------------------------------
    // Bluetooth Controller
    // --------------------------------------------------

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    err = esp_bt_controller_init(&bt_cfg);

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " bt_controller_init error: %i,  %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, err, esp_err_to_name(err));
        goto error;
    }

    m_btControllerInitialized = true;

    err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " bt_controller_enable error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    m_btControllerEnabled = true;

    info(Audio::evt_bt, "BT Controller enabled");

    // --------------------------------------------------
    // Bluedroid
    // --------------------------------------------------

    err = esp_bluedroid_init();

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " bluedroid_init error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    m_bluedroidInitialized = true;

    err = esp_bluedroid_enable();

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " bluedroid_enable error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    m_bluedroidEnabled = true;

    info(Audio::evt_bt, "Bluedroid enabled");

    // --------------------------------------------------
    // GAP
    // --------------------------------------------------

    err = esp_bt_gap_register_callback(gap_callback);

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " bt_gap_register error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    info(Audio::evt_bt, "GAP callback registered");

    // --------------------------------------------------
    // AVRCP Target
    // --------------------------------------------------

    err = esp_avrc_tg_register_callback(avrc_tg_callback);

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " avrc_tg_register error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    err = esp_avrc_tg_init();

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " esp_avrc_tg_init error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    m_avrcTgInitRequested = true;

    info(Audio::evt_bt, "AVRCP Target initialization requested" ANSI_ESC_RESET "\n", __FILE__, __LINE__);

    // --------------------------------------------------
    // A2DP Source
    // --------------------------------------------------

    err = esp_a2d_register_callback(a2dp_callback);

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " a2d_register error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    err = esp_a2d_source_init();

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " a2d_source_init error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    m_a2dpInitRequested = true;

    err = esp_a2d_source_register_data_callback(get_data);

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " a2d_source_register error" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    info(Audio::evt_bt, "A2DP Source initialization requested");

    // --------------------------------------------------
    // Wait for both profiles
    // --------------------------------------------------

    bits = xEventGroupWaitBits(m_btEvents, BT_EVT_AVRC_INIT | BT_EVT_A2DP_INIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));

    {
        esp_avrc_psth_bit_mask_t cmdSet{};

        const esp_avrc_pt_cmd_t commands[] = {ESP_AVRC_PT_CMD_PLAY,         //
                                              ESP_AVRC_PT_CMD_PAUSE,        //
                                              ESP_AVRC_PT_CMD_STOP,         //
                                              ESP_AVRC_PT_CMD_FORWARD,      //
                                              ESP_AVRC_PT_CMD_BACKWARD,     //
                                              ESP_AVRC_PT_CMD_FAST_FORWARD, //
                                              ESP_AVRC_PT_CMD_REWIND,       //
                                              ESP_AVRC_PT_CMD_VOL_UP,       //
                                              ESP_AVRC_PT_CMD_VOL_DOWN};

        for (auto cmd : commands) {
            if (!esp_avrc_psth_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &cmdSet, cmd)) {
                printf("[%s:%i]" ANSI_ESC_RED " AVRCP passthrough mask error: %u" ANSI_ESC_RESET "\n", __FILE__, __LINE__, cmd);
                err = ESP_FAIL;
                goto error;
            }
        }

        err = esp_avrc_tg_set_psth_cmd_filter(ESP_AVRC_PSTH_FILTER_SUPPORTED_CMD, &cmdSet);

        if (err != ESP_OK) {
            printf("[%s:%i]" ANSI_ESC_RED " AVRCP passthrough filter failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
            goto error;
        }

        info(Audio::evt_bt, "AVRCP passthrough commands configured");
    }

    if ((bits & (BT_EVT_AVRC_INIT | BT_EVT_A2DP_INIT)) != (BT_EVT_AVRC_INIT | BT_EVT_A2DP_INIT)) {

        printf("[%s:%i]" ANSI_ESC_RED " Bluetooth profile initialization timeout" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        goto error;
    }

    m_isInit = true;

    info(Audio::evt_bt, "Bluetooth profiles initialized");

    // Start discovery only now!
    if (!startScan()) goto error;

    return true;

error:

    printf("[%s:%i]" ANSI_ESC_RED " Bluetooth initialization failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));

    reset();

    return false;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

void BT_Source::reset() {

    info(Audio::evt_bt, "stopping Bluetooth");

    if (!m_btControllerInitialized) {
        m_isInit = false;
        return;
    }

    m_stopping.store(true, std::memory_order_release);

    // --------------------------------------------------
    // Stop GAP discovery
    // --------------------------------------------------

    esp_bt_gap_cancel_discovery();

    // --------------------------------------------------
    // Disconnect A2DP and AVRCP
    // --------------------------------------------------

    if (m_connected.load(std::memory_order_acquire) || m_avrcConnected.load(std::memory_order_acquire)) {
        const bool waitAvrc = m_avrcConnected.load(std::memory_order_acquire);

        // Clear events before requesting disconnect
        EventBits_t clearBits = BT_EVT_DISCONNECTED;
        if (waitAvrc) { clearBits |= BT_EVT_AVRC_DISCONNECTED; }
        xEventGroupClearBits(m_btEvents, clearBits);
        esp_err_t err = esp_a2d_source_disconnect(m_targetBda);

        if (err == ESP_OK) {
            EventBits_t waitBits = BT_EVT_DISCONNECTED;
            if (waitAvrc) { waitBits |= BT_EVT_AVRC_DISCONNECTED; }
            EventBits_t bits = xEventGroupWaitBits(m_btEvents, waitBits, pdTRUE,
                                                   pdTRUE, // Beide Events müssen eintreffen!
                                                   pdMS_TO_TICKS(5000));

            if ((bits & waitBits) != waitBits) {
                printf("[%s:%i]" ANSI_ESC_RED " Bluetooth disconnect timeout: A2DP=%d AVRCP=%d" ANSI_ESC_RESET "\n", __FILE__, __LINE__, (bits & BT_EVT_DISCONNECTED) != 0, !waitAvrc || (bits & BT_EVT_AVRC_DISCONNECTED) != 0);

                // Nicht mit der Deinitialisierung fortfahren!
                return;
            }
        } else {
            printf("[%s:%i]" ANSI_ESC_RED " A2DP disconnect failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
            return;
        }
    }
    m_connected = false;

    // --------------------------------------------------
    // Deinitialize AVRCP Target
    // --------------------------------------------------

    if (m_avrcTgInitRequested) {

        xEventGroupClearBits(m_btEvents, BT_EVT_AVRC_DEINIT);

        esp_err_t err = esp_avrc_tg_deinit();

        if (err == ESP_OK) {

            EventBits_t bits = xEventGroupWaitBits(m_btEvents, BT_EVT_AVRC_DEINIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));

            if (!(bits & BT_EVT_AVRC_DEINIT)) {
                printf("[%s:%i]" ANSI_ESC_RED " AVRCP deinit timeout - keeping Bluetooth stack alive" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
                return;
            }
        } else {
            printf("[%s:%i]" ANSI_ESC_RED " AVRCP deinit failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
            return;
        }

        m_avrcTgInitRequested = false;
    }

    // --------------------------------------------------
    // Deinitialize A2DP Source
    // --------------------------------------------------

    if (m_a2dpInitRequested) {

        xEventGroupClearBits(m_btEvents, BT_EVT_A2DP_DEINIT);

        esp_err_t err = esp_a2d_source_deinit();

        if (err == ESP_OK) {

            EventBits_t bits = xEventGroupWaitBits(m_btEvents, BT_EVT_A2DP_DEINIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));

            if (!(bits & BT_EVT_A2DP_DEINIT)) {
                printf("[%s:%i]" ANSI_ESC_RED " A2DP deinit timeout - keeping Bluetooth stack alive" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
                return;
            }
        } else {
            printf("[%s:%i]" ANSI_ESC_RED " A2DP deinit failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
            return;
        }

        m_a2dpInitRequested = false;
    }

    // --------------------------------------------------
    // Bluedroid
    // --------------------------------------------------

    if (m_bluedroidEnabled) {

        esp_err_t err = esp_bluedroid_disable();

        if (err != ESP_OK) {
            printf("[%s:%i]" ANSI_ESC_RED " Bluedroid disable failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
            return;
        }

        m_bluedroidEnabled = false;
    }

    if (m_bluedroidInitialized) {

        esp_err_t err = esp_bluedroid_deinit();

        if (err != ESP_OK) {
            printf("[%s:%i]" ANSI_ESC_RED " Bluedroid deinit failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
            return;
        }

        m_bluedroidInitialized = false;
    }

    // --------------------------------------------------
    // Bluetooth Controller
    // --------------------------------------------------

    if (m_btControllerEnabled) {

        esp_err_t err = esp_bt_controller_disable();

        if (err != ESP_OK) {
            printf("[%s:%i]" ANSI_ESC_RED " BT Controller disable failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
            return;
        }

        m_btControllerEnabled = false;
    }

    if (m_btControllerInitialized) {

        esp_err_t err = esp_bt_controller_deinit();

        if (err != ESP_OK) {
            printf("[%s:%i]" ANSI_ESC_RED " BT Controller deinit failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
            return;
        }

        m_btControllerInitialized = false;
    }

    // --------------------------------------------------
    // Reset states
    // --------------------------------------------------

    m_a2dpInitialized = false;
    m_connected = false;
    m_deviceFound = false;

    m_isInit = false;
    m_stopping = false;

    memset(m_targetBda, 0, sizeof(m_targetBda));

    info(Audio::evt_bt, "Bluetooth stopped");
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::startScan() {

    esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " GAP discovery failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
        return false;
    }

    info(Audio::evt_bt, "GAP discovery started");
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::getChunk(int32_t* chunk, size_t size) {
    size_t written = 0;
    if (size <= bt_samplesBuffer.freeSpace()) {
        written = bt_samplesBuffer.write(chunk, size);
        if (written != size) {
            printf("[%s:%i]" ANSI_ESC_RED " buffer write error size: %lu, written: %lu" ANSI_ESC_RESET "\n", __FILE__, __LINE__, size, written);
            return false;
        }
    } else {
        // printf("[%s:%i]" ANSI_ESC_MAGENTA " not enough space, size %lu, free %lu" ANSI_ESC_RESET "\n", __FILE__, __LINE__, size, bt_samplesBuffer.freeSpace());
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
        printf("[%s:%i]" ANSI_ESC_MAGENTA " BT callback: requested=%d, words=%u, filled=%u", bytes / 2, words, filled);
        if (words > 1) { printf("[%s:%i]" ANSI_ESC_MAGENTA " PCM samples: 0x%08lX, 0x%08lX", (uint32_t)s_instance->m_workBuff[0], (uint32_t)s_instance->m_workBuff[1]); }
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
    if (m_stopping || !m_isInit) return false;
    if (!m_a2dpInitialized) {
        printf("[%s:%i]" ANSI_ESC_RED " A2DP not initialized" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        return false;
    }

    if (!m_deviceFound) {
        printf("[%s:%i]" ANSI_ESC_RED " Target device not found" ANSI_ESC_RESET "\n", __FILE__, __LINE__);
        return false;
    }

    esp_err_t err = esp_a2d_source_connect(m_targetBda);

    if (err != ESP_OK) {
        printf("[%s:%i]" ANSI_ESC_RED " A2DP connect failed: %s" ANSI_ESC_RESET "\n", __FILE__, __LINE__, esp_err_to_name(err));
        return false;
    }

    info(Audio::evt_bt, "A2DP connection requested");

    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void BT_Source::restartDiscoveryIfDisconnected() {

    if (!m_isInit || m_stopping.load(std::memory_order_acquire)) return;

    if (m_connected.load(std::memory_order_acquire) || m_avrcConnected.load(std::memory_order_acquire)) return;

    info(Audio::evt_bt, "Bluetooth connection lost, restarting discovery");

    m_deviceFound = false;
    memset(m_targetBda, 0, sizeof(m_targetBda));

    if (!startScan()) { printf("[%s:%i]" ANSI_ESC_RED " Restarting Bluetooth discovery failed" ANSI_ESC_RESET "\n", __FILE__, __LINE__); }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#endif