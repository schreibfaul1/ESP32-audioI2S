
#pragma once

#define AUTOCONNECT_TRY_NUM               1000
#define A2DP_SPP_SUPPORT                  1
#define A2DP_I2S_MAX_WRITE_SIZE           1024 * 5
#define A2DP_I2S_MAX_WRITE_DELAY_MS       0
#define A2DP_DISCONNECT_LIMIT             20


#include "Arduino.h"
// #include "esp32-hal-bt.h"
// #include "esp32-hal-log.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
// #include "esp_spp_api.h"
// #include "esp_task_wdt.h"
// #include "esp_timer.h"
#include "map"
// #include "nvs.h"
// #include "nvs_flash.h"
#include <vector>


#define BT_APP_SIG_WORK_DISPATCH (0x01)

/**
 * @brief     handler for the dispatched work
 */
typedef void (*app_callback_t)(uint16_t event, void* param);

/** @brief Internal message to be sent for BluetoothA2DPSink and
 * BluetoothA2DPSource */
typedef struct {
    uint16_t       sig;   /*!< signal to app_task */
    uint16_t       event; /*!< message event id */
    app_callback_t cb;    /*!< context switch callback */
    void*          param; /*!< parameter area needs to be last */
} bt_app_msg_t;

// AVRCP used transaction labels
#define APP_RC_CT_TL_GET_CAPS           (0)
#define APP_RC_CT_TL_GET_META_DATA      (1)
#define APP_RC_CT_TL_RN_TRACK_CHANGE    (2)
#define APP_RC_CT_TL_RN_PLAYBACK_CHANGE (3)
#define APP_RC_CT_TL_RN_PLAY_POS_CHANGE (4)

// common a2dp callbacks
extern "C" void ccall_bt_app_task_handler(void* arg);
extern "C" void ccall_app_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
extern "C" void ccall_app_rc_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param);
extern "C" void ccall_app_a2d_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);
extern "C" void ccall_av_hdl_stack_evt(uint16_t event, void* p_param);
extern "C" void ccall_app_rc_tg_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param);
extern "C" void ccall_av_hdl_avrc_tg_evt(uint16_t event, void* p_param);


/**
 * @brief Buetooth A2DP Reconnect Status
 * @ingroup a2dp
 */
enum ReconnectStatus { NoReconnect, AutoReconnect, IsReconnecting };

/**
 * @brief Common Bluetooth A2DP functions
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */
class BluetoothA2DPCommon {
    /// task handler
    friend void ccall_bt_app_task_handler(void* arg);
    friend void ccall_app_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    friend void ccall_app_rc_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param);
    friend void ccall_app_a2d_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);
    friend void ccall_av_hdl_stack_evt(uint16_t event, void* p_param);

    /// handle esp_avrc_tg_cb_event_t
    friend void ccall_app_rc_tg_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param);
    /* avrc TG event handler */
    friend void ccall_av_hdl_avrc_tg_evt(uint16_t event, void* p_param);


  public:
    /// Default constructor
    BluetoothA2DPCommon();
    /// Destructor
    virtual ~BluetoothA2DPCommon() = default;


    /// Closes the connection
    virtual void disconnect();


    /// Closes the connection and stops A2DP
    virtual void end(bool releaseMemory = false);

    /// Checks if A2DP is connected using the connection state received from the speaker
    virtual bool is_connected() { return connection_state == ESP_A2D_CONNECTION_STATE_CONNECTED; }

    /// Determine the actual audio state
    virtual esp_a2d_audio_state_t get_audio_state();

    /// Determine the connection state
    virtual esp_a2d_connection_state_t get_connection_state();

    /// Set the callback that is called when the connection state is changed
    /// This callback is called before the I2S bus is changed.
    virtual void set_on_connection_state_changed(void (*callBack)(esp_a2d_connection_state_t state, void*), void* obj = nullptr);

    /// Set the callback that is called after the audio state has changed.
    /// This callback is called after the I2S bus has changed.
    virtual void set_on_audio_state_changed_post(void (*callBack)(esp_a2d_audio_state_t state, void*), void* obj = nullptr);

    /// Set the callback that is called when the audio state is changed
    virtual void set_on_audio_state_changed(void (*callBack)(esp_a2d_audio_state_t state, void*), void* obj = nullptr);

    /// Logs the free heap
    void log_free_heap();

    /// converts esp_a2d_connection_state_t to a string
    const char* to_str(esp_a2d_connection_state_t state);

    /// converts a esp_a2d_audio_state_t to a string
    const char* to_str(esp_a2d_audio_state_t state);

    /// converts a esp_bd_addr_t to a string - the string is 18 characters long!
    const char* to_str(esp_bd_addr_t bda);

    /// converts esp_bt_gap_discovery_state_t to a string
    const char* to_str(esp_bt_gap_discovery_state_t state);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    /// converts esp_avrc_playback_stat_t to a string
    const char* to_str(esp_avrc_playback_stat_t state);
#endif

    /// defines the task priority (the default value is configMAX_PRIORITIES - 10)
    void set_task_priority(UBaseType_t priority) { task_priority = priority; }

    /// Defines the core which is used to start the tasks (to process the events
    /// and audio queue): default is 1
    void set_task_core(BaseType_t core) { task_core = core; }

    /// Defines the queue size of the event task
    void set_event_queue_size(int size) { event_queue_size = size; }

    /// Defines the stack size of the event task (in bytes)
    void set_event_stack_size(int size) { event_stack_size = size; }

    /// Provides the address of the last device
    virtual esp_bd_addr_t* get_last_peer_address() { return &last_connection; }

    /// Bluetooth discoverability
    virtual void set_discoverability(esp_bt_discovery_mode_t d);

    /// Bluetooth connectable
    virtual void set_connectable(bool connectable) { set_scan_mode_connectable(connectable); }

    /// Provides the actual SSID name
    virtual const char* get_name() { return bt_name; }

    /// clean last connection (delete)
    virtual void clean_last_connection();

    /// Defines the default bt mode. The default is ESP_BT_MODE_CLASSIC_BT: use
    /// this e.g. to set to ESP_BT_MODE_BTDM
    virtual void set_default_bt_mode(esp_bt_mode_t mode) { bt_mode = mode; }

    /// Defines the esp_bluedroid_config_t: Available from IDF 5.2.1
    void set_bluedroid_config_t(esp_bluedroid_config_t cfg) { bluedroid_config = cfg; }

    /// calls vTaskDelay to pause for the indicated number of milliseconds
    void delay_ms(uint32_t millis);
    /// Provides the time in milliseconds since the last system boot
    unsigned long get_millis();

    virtual void set_avrc_rn_events(std::vector<esp_avrc_rn_event_ids_t> events) { avrc_rn_events = events; }

    ///  Store a reference to an object (e.g. to use in the callbacks)
    void set_reference(void* ref, int type = 0);
    ///  Get the reference to an object (e.g. to use in the callbacks)
    void* get_reference(int type = 0);
    /// @brief Clear all references
    void clear_references() { references.clear(); }

  protected:
    const char*     bt_name = {0};
    esp_bd_addr_t   peer_bd_addr;
    ReconnectStatus reconnect_status = NoReconnect;
    unsigned long   reconnect_timout = 0;
    unsigned int    default_reconnect_timout = 10000;
    bool            is_autoreconnect_allowed = false;
    // A2DPDefaultVolumeControl default_volume_control;
    // A2DPVolumeControl *volume_control_ptr = nullptr;
    esp_bd_addr_t last_connection = {0, 0, 0, 0, 0, 0};
    bool          is_start_disabled = false;
    bool          is_target_status_active = true;
    void (*connection_state_callback)(esp_a2d_connection_state_t state, void* obj) = nullptr;
    void (*audio_state_callback)(esp_a2d_audio_state_t state, void* obj) = nullptr;
    void (*audio_state_callback_post)(esp_a2d_audio_state_t state, void* obj) = nullptr;
    void*                      connection_state_obj = nullptr;
    void*                      audio_state_obj = nullptr;
    void*                      audio_state_obj_post = nullptr;
    const char*                m_a2d_conn_state_str[4] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
    const char*                m_a2d_audio_state_str[4] = {"Suspended", "Started", "Suspended", "Suspended"};
    const char*                m_avrc_playback_state_str[5] = {"stopped", "playing", "paused", "forward seek", "reverse seek"};
    const char*                m_esp_bt_gap_discovery_state_t[2] = {"Stopped", "Started"};
    esp_a2d_audio_state_t      audio_state = ESP_A2D_AUDIO_STATE_SUSPEND;
    esp_a2d_connection_state_t connection_state = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    UBaseType_t                task_priority = configMAX_PRIORITIES - 10;
    // volume
    uint8_t    volume_value = 0;
    bool       is_volume_used = false;
    bool       is_bluedroid_initialized = false;
    BaseType_t task_core = 1;

    int                                  event_queue_size = 20;
    int                                  event_stack_size = 3072;
    esp_bt_mode_t                        bt_mode = ESP_BT_MODE_CLASSIC_BT;
    std::vector<esp_avrc_rn_event_ids_t> avrc_rn_events = {ESP_AVRC_RN_VOLUME_CHANGE};

    QueueHandle_t        app_task_queue = nullptr;
    TaskHandle_t         app_task_handle = nullptr;
    std::map<int, void*> references;

    esp_bluedroid_config_t bluedroid_config{};

    virtual esp_err_t   esp_a2d_connect(esp_bd_addr_t peer) = 0;
    virtual const char* last_bda_nvs_name() = 0;
    virtual bool        get_last_connection();
    virtual void        set_last_connection(esp_bd_addr_t bda);
    virtual bool        has_last_connection();

    // change the scan mode
    virtual void set_scan_mode_connectable(bool connectable);
    virtual void set_scan_mode_connectable_default() = 0;

    virtual bool      bt_start();
    virtual esp_err_t bluedroid_init();
    virtual esp_err_t esp_a2d_disconnect(esp_bd_addr_t remote_bda) = 0;
    virtual void      app_task_start_up();
    virtual void      app_task_shut_down();
    virtual bool      app_send_msg(bt_app_msg_t* msg);
    virtual void      app_task_handler(void* arg);
    virtual void      app_work_dispatched(bt_app_msg_t* msg);
    virtual bool      isSource() = 0;
    // GAP callback
    virtual void app_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) = 0;
    /// callback function for AVRCP controller
    virtual void app_rc_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param) = 0;
    /// callback function for A2DP source
    virtual void app_a2d_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) = 0;
    // handler for bluetooth stack enabled events
    virtual void av_hdl_stack_evt(uint16_t event, void* p_param) = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    esp_bt_discovery_mode_t discoverability = ESP_BT_GENERAL_DISCOVERABLE;
    virtual void            app_rc_tg_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param) = 0;
    virtual void            av_hdl_avrc_tg_evt(uint16_t event, void* p_param) = 0;
#endif
};

extern BluetoothA2DPCommon* actual_bluetooth_a2dp_common;
