#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "softdevice_handler.h"
#include "app_timer.h"
#include "device_manager.h"
#include "pstorage.h"
#include "eddystone_timeslot.h"

#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "led.h"
#include "debug.h"

#define IS_SRVC_CHANGED_CHARACT_PRESENT     	0                                          /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define CENTRAL_LINK_COUNT              			0                                 /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           			1                                 /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define APP_TIMER_PRESCALER             0                                 /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                 /**< Size of timer operation queues. */

#define APP_CFG_NON_CONN_ADV_TIMEOUT    0                                 /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables the time-out. */
#define NON_CONNECTABLE_ADV_INTERVAL    MSEC_TO_UNITS(100, UNIT_0_625_MS) /**< The advertising interval for non-connectable advertisement (100 ms). This value can vary between 100 ms and 10.24 s). */

#define SEC_PARAM_BOND                       1                                          /**< Perform bonding. */
#define SEC_PARAM_MITM                       0                                          /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                       0                                          /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS                   0                                          /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES            BLE_GAP_IO_CAPS_NONE                       /**< No I/O capabilities. */
#define SEC_PARAM_OOB                        0                                          /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE               7                                          /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE               16                                         /**< Maximum encryption key size. */

// Eddystone common data
#define APP_EDDYSTONE_UUID              0xFEAA                            /**< UUID for Eddystone beacons according to specification. */
#define APP_EDDYSTONE_RSSI              0xEE                              /**< 0xEE = -18 dB is the approximate signal strength at 0 m. */

// Eddystone UID data
#define APP_EDDYSTONE_UID_FRAME_TYPE    0x00                              /**< UID frame type is fixed at 0x00. */
#define APP_EDDYSTONE_UID_RFU           0x00, 0x00                        /**< Reserved for future use according to specification. */
#define APP_EDDYSTONE_UID_ID            0x01, 0x02, 0x03, 0x04, \
                                        0x05, 0x06                        /**< Mock values for 6-byte Eddystone UID ID instance.  */
#define APP_EDDYSTONE_UID_NAMESPACE     0xAA, 0xAA, 0xBB, 0xBB, \
                                        0xCC, 0xCC, 0xDD, 0xDD, \
                                        0xEE, 0xEE                        /**< Mock values for 10-byte Eddystone UID ID namespace. */

// Eddystone URL data
#define APP_EDDYSTONE_URL_FRAME_TYPE    0x10                              /**< URL Frame type is fixed at 0x10. */
#define APP_EDDYSTONE_URL_SCHEME        0x00                              /**< 0x00 = "http://www" URL prefix scheme according to specification. */
#define APP_EDDYSTONE_URL_URL           0x6e, 0x6f, 0x72, 0x64, \
                                        0x69, 0x63, 0x73, 0x65, \
                                        0x6d,0x69, 0x00                   /**< "nordicsemi.com". Last byte suffix 0x00 = ".com" according to specification. */

#define APP_COMPANY_IDENTIFIER               0x0059                                     /**< Company identifier for Nordic Semiconductor ASA. as per www.bluetooth.org. */

#define BEACON_UUID 0xff, 0xfe, 0x2d, 0x12, 0x1e, 0x4b, 0x0f, 0xa4,\
                    0x99, 0x4e, 0xce, 0xb5, 0x31, 0xf4, 0x05, 0x45 
#define BEACON_ADV_INTERVAL                  400                                        /**< The Beacon's advertising interval, in milliseconds*/
#define BEACON_MAJOR                         0x1234                                     /**< The Beacon's Major*/
#define BEACON_MINOR                         0x5678                                     /**< The Beacon's Minor*/
#define BEACON_RSSI                          0xC3                                       /**< The Beacon's measured RSSI at 1 meter distance in dBm. */

#define DEVICE_NAME                          "Nordic_HRM_adv"                           /**< Name of device. Will be included in the advertising data. */
#define APP_ADV_INTERVAL                     480                                         /**< The advertising interval (in units of 0.625 ms. This value corresponds to 300 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS           180                                        /**< The advertising timeout in units of seconds. */

#define MIN_CONN_INTERVAL                    MSEC_TO_UNITS(500, UNIT_1_25_MS)           /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL                    MSEC_TO_UNITS(1000, UNIT_1_25_MS)          /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                        0                                          /**< Slave latency. */
#define CONN_SUP_TIMEOUT                     MSEC_TO_UNITS(4000, UNIT_10_MS)            /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY        APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER)/**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT         3                                          /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                        /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

static dm_application_instance_t             m_app_handle;                              /**< Application identifier allocated by device manager. */

static uint16_t                              m_conn_handle = BLE_CONN_HANDLE_INVALID;   /**< Handle of the current connection. */

static ble_gap_adv_params_t m_adv_params;                                 /**< Parameters to be passed to the stack when starting advertising. */

static ble_beacon_init_t beacon_init;

static ble_uuid_t m_adv_uuids[] =                                                       /**< Universally unique service identifiers. */
{
    {BLE_UUID_HEART_RATE_SERVICE,         BLE_UUID_TYPE_BLE},
    {BLE_UUID_BATTERY_SERVICE,            BLE_UUID_TYPE_BLE},
    {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}
};

//static uint8_t eddystone_url_data[] =   /**< Information advertised by the Eddystone URL frame type. */
//{
//    APP_EDDYSTONE_URL_FRAME_TYPE,   // Eddystone URL frame type.
//    APP_EDDYSTONE_RSSI,             // RSSI value at 0 m.
 //   APP_EDDYSTONE_URL_SCHEME,       // Scheme or prefix for URL ("http", "http://www", etc.)
 //   APP_EDDYSTONE_URL_URL           // URL with a maximum length of 17 bytes. Last byte is suffix (".com", ".org", etc.)
//};

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
		debug_printf("Assert from softdevice. failed miserably. resetting... \r\n");
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
		uint32_t err_code;	

    // Prepare wakeup buttons.
		debug_printf("Cycle power to restart. Bye. \r\n");
		
    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
//    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
						debug_printf("Entering fast adverts... \r\n");
            break;
        case BLE_ADV_EVT_IDLE:
						debug_printf("Entering Idle mode. \r\n");
            sleep_mode_enter();
            break;
        default:
            break;
    }
}

/**@brief Function for initializing the advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
	
	uint32_t      err_code;
    ble_advdata_t advdata;

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = true;
    advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = m_adv_uuids;

    ble_adv_modes_config_t options = {0};
    options.ble_adv_fast_enabled  = BLE_ADV_FAST_ENABLED;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;

    err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
    if (err_code == NRF_SUCCESS)
			debug_printf("Advertising initialised!\r\n");
		else
		{
			debug_printf("Ooops.. Something is wrong with initialising advertising..\r\n");
			APP_ERROR_CHECK(err_code);
		}
		
		
   // uint32_t      err_code;
    //ble_advdata_t advdata;
    //uint8_t       flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    //ble_uuid_t    adv_uuids[] = {{APP_EDDYSTONE_UUID, BLE_UUID_TYPE_BLE}};

    //uint8_array_t eddystone_data_array;                             // Array for Service Data structure.
/** @snippet [Eddystone data array] */
    //eddystone_data_array.p_data = (uint8_t *) eddystone_url_data;   // Pointer to the data to advertise.
    //eddystone_data_array.size = sizeof(eddystone_url_data);         // Size of the data to advertise.
/** @snippet [Eddystone data array] */

   // ble_advdata_service_data_t service_data;                        // Structure to hold Service Data.
   // service_data.service_uuid = APP_EDDYSTONE_UUID;                 // Eddystone UUID to allow discoverability on iOS devices.
   // service_data.data = eddystone_data_array;                       // Array for service advertisement data.

    // Build and set advertising data.
    //memset(&advdata, 0, sizeof(advdata));

    //advdata.name_type               = BLE_ADVDATA_NO_NAME;
    //advdata.flags                   = flags;
    //advdata.service_data_count      = 1;

    //err_code = ble_advdata_set(&advdata, NULL);
    //if (err_code == NRF_SUCCESS)
		//	debug_printf("Advertising data set!\r\n");
		////else
		//{
			//debug_printf("Ooops.. Something is wrong with setting advertising data..\r\n");
			//APP_ERROR_CHECK(err_code);
		//}

    // Initialize advertising parameters (used when starting advertising).
    //memset(&m_adv_params, 0, sizeof(m_adv_params));
		
		//debug_printf("Initialising all advertising parameters...\r\n");
    //m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
    //m_adv_params.p_peer_addr = NULL;                                // Undirected advertisement.
    //m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    //m_adv_params.interval    = NON_CONNECTABLE_ADV_INTERVAL;
   // m_adv_params.timeout     = APP_CFG_NON_CONN_ADV_TIMEOUT;
}

/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code;

    //err_code = sd_ble_gap_adv_start(&m_adv_params);
		err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
		if (err_code == NRF_SUCCESS)	
			debug_printf("Advertisements started!!\r\n");
		else
		{
			debug_printf("Ooops.. Something went wrong with starting adverts..\r\n");
			APP_ERROR_CHECK(err_code);
		}

}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
//	uint32_t err_code;
	
	/* Initialise timer module */
	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
	
  // Initialize timer module.
  // Create timers.
  /*err_code = app_timer_create(&m_battery_timer_id,
                              APP_TIMER_MODE_REPEATED,
                              battery_level_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);
    */
	/* YOUR_JOB: Create any timers to be used by the application.
                 Below is an example of how to create a timer.
                 For every new timer needed, increase the value of the macro APP_TIMER_MAX_TIMERS by
                 one.
    uint32_t err_code;
    err_code = app_timer_create(&m_app_timer_id, APP_TIMER_MODE_REPEATED, timer_timeout_handler);
    APP_ERROR_CHECK(err_code); */
}

/**@brief Function for starting application timers.
 */
static void application_timers_start(void)
{
	debug_printf("NO TIMERS TO START!!\r\n");
			
	//fill me up :)
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in]   p_evt   Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        if (err_code == NRF_SUCCESS)
					debug_printf("BLE_CONN_PARAMS_EVT_FAILED, disconnecting!\r\n");
				else
				{
					debug_printf("Tried to disconnect after BLE_CONN_PARAMS_EVT_FAILED but there was an error!\r\n");
					APP_ERROR_CHECK(err_code);
				}
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
		debug_printf("Connection parameters error! \r\n");                    
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    if (err_code == NRF_SUCCESS)
			debug_printf("Softdevice configured with links and ble parameters!\r\n");
		else
		{
			debug_printf("Ooops.. Something is wrong with setting softdevice parameters..\r\n");
			APP_ERROR_CHECK(err_code);
		}
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    if (err_code == NRF_SUCCESS)
			debug_printf("Device name set!\r\n");
		else
		{
			debug_printf("Ooops.. Something went wrong with setting the name..\r\n");
			APP_ERROR_CHECK(err_code);
		}

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_HEART_RATE_SENSOR_HEART_RATE_BELT);
    if (err_code == NRF_SUCCESS)
			debug_printf("Appearance set!\r\n");
		else
		{
			debug_printf("Ooops.. Something is wrong with setting the appearrance..\r\n");
			APP_ERROR_CHECK(err_code);
		}

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    if (err_code == NRF_SUCCESS)
			debug_printf("Connection parameters set!\r\n");
		else
		{
			debug_printf("Ooops.. Something is wrong with setting connection parameters..\r\n");
			APP_ERROR_CHECK(err_code);
		}
}

/**@brief Function for initializing services that will be used by the application.
 *
 * @details Initialize the Heart Rate, Battery and Device Information services.
 */
static void services_init(void)
{
  //fill me up :)
	debug_printf("SERVICES EMPTY..\r\n");
			
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
						
						debug_printf("Connected :)\r\n");
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            app_beacon_stop();
            
            break;

        case BLE_GAP_EVT_DISCONNECTED:

						app_beacon_start();
            // when not using the timeslot implementation, it is necessary to initialize the advertizing data again.
            advertising_init();
            err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
            APP_ERROR_CHECK(err_code);
						debug_printf("Disconnected, Advertising peripheral again \r\n");            
            
            break;

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    dm_ble_evt_handler(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
    app_beacon_on_sys_evt(sys_evt);
    ble_advertising_on_sys_evt(sys_evt);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;
        
		// Initialize the SoftDevice handler module.
		SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL);
    
    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
		if (err_code == NRF_SUCCESS)
			debug_printf("Softdevice configured with links and ble parameters!\r\n");
		else
		{
			debug_printf("Ooops.. Something is wrong with setting softdevice parameters..\r\n");
			APP_ERROR_CHECK(err_code);
		}
    
    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);
    
    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    if (err_code == NRF_SUCCESS)
			debug_printf("Softdevice enabled successfully!\r\n");
		else
		{
			debug_printf("Ooops.. Something is wrong with enabling softdevice..\r\n");
			APP_ERROR_CHECK(err_code);
		}
		
		// Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    if (err_code == NRF_SUCCESS)
			debug_printf("Softdevice registered for BLE events!\r\n");
		else
		{
			debug_printf("Ooops.. Something went wrong with registering to BLE events..\r\n");
			APP_ERROR_CHECK(err_code);
		}

    // Register with the SoftDevice handler module for SYS events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    if (err_code == NRF_SUCCESS)
			debug_printf("Softdevice registered for SYS events!\r\n");
		else
		{
			debug_printf("Ooops.. Something went wrong with registering to SYS events..\r\n");
			APP_ERROR_CHECK(err_code);
		}
}

/**@brief Function for handling the Device Manager events.
 *
 * @param[in]   p_evt   Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const    * p_handle,
                                           dm_event_t const     * p_event,
                                           ret_code_t           event_result)
{
		debug_printf("DM event!\r\n");
    APP_ERROR_CHECK(event_result);
    return NRF_SUCCESS;
}

/**@brief Function for the Device Manager initialization.
 *
 * @param[in] erase_bonds  Indicates whether bonding information should be cleared from
 *                         persistent storage during initialization of the Device Manager.
 */
static void device_manager_init(bool erase_bonds)
{
    uint32_t               err_code;
    dm_init_param_t        init_param = {.clear_persistent_data = erase_bonds};
    dm_application_param_t  register_param;
    
    // Initialize persistent storage module.
    err_code = pstorage_init();
    if (err_code == NRF_SUCCESS)
			debug_printf("Persistent storage module is ready!\r\n");
		else
		{
			debug_printf("Ooops.. Something went wrong with initialising the PS module..\r\n");
			APP_ERROR_CHECK(err_code);
		}
    
		err_code = dm_init(&init_param);
    if (err_code == NRF_SUCCESS)
			debug_printf("DM initialised!\r\n");
		else
		{
			debug_printf("Ooops.. Something went wrong with initialising the DM..\r\n");
			APP_ERROR_CHECK(err_code);
		}
    
		memset(&register_param.sec_param, 0, sizeof(ble_gap_sec_params_t));
    
    register_param.sec_param.bond         = SEC_PARAM_BOND;
    register_param.sec_param.mitm         = SEC_PARAM_MITM;
    register_param.sec_param.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    register_param.sec_param.oob          = SEC_PARAM_OOB;
    register_param.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    register_param.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    register_param.evt_handler            = device_manager_evt_handler;
    register_param.service_type           = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    if (err_code == NRF_SUCCESS)
			debug_printf("Registered parameters for DM!\r\n");
		else
		{
			debug_printf("Ooops.. Something went wrong with registering the parameters for the DM..\r\n");
			APP_ERROR_CHECK(err_code);
		}
}

/**@brief Function for handling a BeaconAdvertiser error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void beacon_advertiser_error_handler(uint32_t nrf_error)
{
		debug_printf("Beacon advertiser error!!\r\n");
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing Beacon advertiser.
 */
static void beacon_adv_init(void)
{
    static uint8_t beacon_uuid[] = {BEACON_UUID};
    
    memcpy(beacon_init.uuid.uuid128, beacon_uuid, sizeof(beacon_uuid));
    beacon_init.adv_interval  = BEACON_ADV_INTERVAL;
    beacon_init.major         = BEACON_MAJOR;
    beacon_init.minor         = BEACON_MINOR;
    beacon_init.manuf_id      = APP_COMPANY_IDENTIFIER;
    beacon_init.rssi          = BEACON_RSSI;
    beacon_init.error_handler = beacon_advertiser_error_handler;
    
    uint32_t err_code = sd_ble_gap_address_get(&beacon_init.beacon_addr);
    if (err_code == NRF_SUCCESS)
			debug_printf("Got the address!!\r\n");
		else
		{
			debug_printf("Ooops.. Something is wrong with getting the address..\r\n");
			APP_ERROR_CHECK(err_code);
		}
    
    app_beacon_init(&beacon_init);
}

/**@brief Function for doing power management.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

int main(void)
{
	uint32_t err_code;
	bool erase_bonds;
	
	/* Initialise debugging port */
  debug_init();
  debug_printf("Debug port opened... Initialising...\r\n");
	
	/* Initialise GPIO subsystem */
	err_code = nrf_drv_gpiote_init();
	if (err_code == NRF_SUCCESS)
		debug_printf("GPIOTE initialised!\r\n");
	else
	{
			debug_printf("Ooops.. Something is wrong with initialising GPIOTE..\r\n");
			APP_ERROR_CHECK(err_code);
	}

	led_init();
	debug_printf("Visual feedback should be active!\r\n");
	
	timers_init();
	ble_stack_init();
	beacon_adv_init();
	device_manager_init(erase_bonds);
	gap_params_init();
	advertising_init();
	services_init();
	conn_params_init();
	
	application_timers_start();
  advertising_start();
	
	led_rgb_set(LED_RED, true);
	led_rgb_set(LED_GREEN, false);
	led_rgb_set(LED_BLUE, false);
	
	debug_printf("Readyyy.. \r\n");
	
  while (true)
  {
		power_manage();
	}
}
