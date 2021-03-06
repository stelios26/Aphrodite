#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "nrf_adc.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_bas.h"
#include "ble_dis.h"
#include "ble_conn_params.h"
#include "softdevice_handler.h"
#include "app_timer.h"
#include "device_manager.h"
#include "pstorage.h"
#include "eddystone_timeslot.h"

#ifdef BLE_DFU_APP_SUPPORT
#include "ble_dfu.h"
#include "dfu_app_handler.h"
#endif // BLE_DFU_APP_SUPPORT

#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "led.h"
#include "debug.h"
#include "ble_cch.h"

#define IS_SRVC_CHANGED_CHARACT_PRESENT     	1                                          	/**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define CENTRAL_LINK_COUNT              			0                                 					/**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           			1                                 					/**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define APP_TIMER_PRESCALER             			0                                 					/**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE         			4                                 					/**< Size of timer operation queues. */

#define BATTERY_LEVEL_MEAS_INTERVAL          	APP_TIMER_TICKS(2000, APP_TIMER_PRESCALER) 	/**< Battery level measurement interval (ticks). */
#define TEMPERATURE_MEAS_INTERVAL          		APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER) 	/**< Battery level measurement interval (ticks). */
#define DOOR_MEAS_INTERVAL          					APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER) 	/**< Battery level measurement interval (ticks). */

#define APP_CFG_NON_CONN_ADV_TIMEOUT    			0                                					 	/**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables the time-out. */
#define NON_CONNECTABLE_ADV_INTERVAL    			MSEC_TO_UNITS(100, UNIT_0_625_MS) 					/**< The advertising interval for non-connectable advertisement (100 ms). This value can vary between 100 ms and 10.24 s). */

#define SEC_PARAM_BOND                       	1                                          	/**< Perform bonding. */
#define SEC_PARAM_MITM                       	0                                          	/**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                       	0                                          	/**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS                   	0                                          	/**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES            	BLE_GAP_IO_CAPS_NONE                       	/**< No I/O capabilities. */
#define SEC_PARAM_OOB                        	0                                          	/**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE               	7                                          	/**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE               	16                                         	/**< Maximum encryption key size. */

#ifdef BLE_DFU_APP_SUPPORT
#define DFU_REV_MAJOR                    0x00                                       /** DFU Major revision number to be exposed. */
#define DFU_REV_MINOR                    0x01                                       /** DFU Minor revision number to be exposed. */
#define DFU_REVISION                     ((DFU_REV_MAJOR << 8) | DFU_REV_MINOR)     /** DFU Revision number to be exposed. Combined of major and minor versions. */
#define APP_SERVICE_HANDLE_START         0x000C                                     /**< Handle of first application specific service when when service changed characteristic is present. */
#define BLE_HANDLE_MAX                   0xFFFF                                     /**< Max handle value in BLE. */

STATIC_ASSERT(IS_SRVC_CHANGED_CHARACT_PRESENT);                                     /** When having DFU Service support in application the Service Changed Characteristic should always be present. */
#endif // BLE_DFU_APP_SUPPORT

#define DEVICE_NAME                          "EddystoneP"                           /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME                    "ManufacturerA"                      /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                     480                                         /**< The advertising interval (in units of 0.625 ms. This value corresponds to 300 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS           300                                        /**< The advertising timeout in units of seconds. */

#define MIN_CONN_INTERVAL                    MSEC_TO_UNITS(500, UNIT_1_25_MS)           /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL                    MSEC_TO_UNITS(1000, UNIT_1_25_MS)          /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                        0                                          /**< Slave latency. */
#define CONN_SUP_TIMEOUT                     MSEC_TO_UNITS(4000, UNIT_10_MS)            /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY        APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER)/**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT         3                                          /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       			0xDEADBEEF                        /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */


static dm_application_instance_t             m_app_handle;                              /**< Application identifier allocated by device manager. */

static uint16_t                              m_conn_handle = BLE_CONN_HANDLE_INVALID;   /**< Handle of the current connection. */
static ble_bas_t                             m_bas;                                     /**< Structure used to identify the battery service. */

static ble_gap_adv_params_t m_adv_params;                                 /**< Parameters to be passed to the stack when starting advertising. */

static ble_beacon_init_t beacon_init;

int32_t adc_sample;

ble_cch_t m_cch_service;

APP_TIMER_DEF(m_battery_timer_id);                                                      /**< Battery timer. */
APP_TIMER_DEF(m_temperature_timer_id);
APP_TIMER_DEF(m_door_timer_id);

static ble_uuid_t m_adv_uuids[] =                                                       /**< Universally unique service identifiers. */
{
    {BLE_UUID_BATTERY_SERVICE,            BLE_UUID_TYPE_BLE},
    {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}
};

#ifdef BLE_DFU_APP_SUPPORT
static ble_dfu_t                         m_dfus;                                    /**< Structure used to identify the DFU service. */
#endif // BLE_DFU_APP_SUPPORT


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

static void temperature_timeout_handler(void * p_context)
{
    // Update temperature and characteristic value.
    int32_t temperature = 0;    // Declare variable holding temperature value
    static int32_t previous_temperature = 15; // Declare a variable to store current temperature until next measurement.
    
		nrf_adc_start();
  
		//full scale 1.2V is 10 bits 1024, prescaler * 3, -750 for 0 then add 25 which is 750 and 10mv per C after that.
		temperature = -(25 + (((3 * ((1200 * adc_sample) / 1023)) - 750)/10));
	
	
    
    // Check if current temperature is different from last temperature
    if(temperature != previous_temperature)
    {
        // If new temperature then send notification
        cch_termperature_characteristic_update(&m_cch_service, &temperature);
    }
    
    // Save current temperature until next measurement
    previous_temperature = temperature;
}

static void door_timeout_handler(void * p_context)
{
    // Update door and characteristic value.
    uint8_t door = 0;    // Declare variable holding door value
    static uint8_t previous_door = 128; // Declare a variable to store current door until next measurement.
    
    door = !nrf_gpio_pin_read(2); // Get door
    
    // Check if current temperature is different from last temperature
    if(door != previous_door)
    {
        // If new temperature then send notification
        cch_door_characteristic_update(&m_cch_service, &door);
    }
    
    // Save current temperature until next measurement
    previous_door = door;
}

/**@brief Function for performing battery measurement and updating the Battery Level characteristic
 *        in Battery Service.
 */
static void battery_level_update(void)
{
    uint32_t err_code;
    uint8_t  battery_level;

    battery_level = 79;

    err_code = ble_bas_battery_level_update(&m_bas, battery_level);
    if ((err_code != NRF_SUCCESS) &&
        (err_code != NRF_ERROR_INVALID_STATE) &&
        (err_code != BLE_ERROR_NO_TX_PACKETS) &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
    {
        APP_ERROR_HANDLER(err_code);
				debug_printf("Error updating battery level \r\n");
    }
}


/**@brief Function for handling the Battery measurement timer timeout.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *
 * @param[in]   p_context   Pointer used for passing some arbitrary information (context) from the
 *                          app_start_timer() call to the timeout handler.
 */
static void battery_level_meas_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    battery_level_update();
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
						ble_advertising_start(BLE_ADV_MODE_FAST);//endless advertising
            //sleep_mode_enter();
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


#ifdef BLE_DFU_APP_SUPPORT
/**@brief Function for stopping advertising.
 */
static void advertising_stop(void)
{
    uint32_t err_code;

		//stop eddystone as well
		app_beacon_stop();
    
		err_code = sd_ble_gap_adv_stop();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for loading application-specific context after establishing a secure connection.
 *
 * @details This function will load the application context and check if the ATT table is marked as
 *          changed. If the ATT table is marked as changed, a Service Changed Indication
 *          is sent to the peer if the Service Changed CCCD is set to indicate.
 *
 * @param[in] p_handle The Device Manager handle that identifies the connection for which the context
 *                     should be loaded.
 */
static void app_context_load(dm_handle_t const * p_handle)
{
    uint32_t                 err_code;
    static uint32_t          context_data;
    dm_application_context_t context;

    context.len    = sizeof(context_data);
    context.p_data = (uint8_t *)&context_data;

    err_code = dm_application_context_get(p_handle, &context);
    if (err_code == NRF_SUCCESS)
    {
        // Send Service Changed Indication if ATT table has changed.
        if ((context_data & (DFU_APP_ATT_TABLE_CHANGED << DFU_APP_ATT_TABLE_POS)) != 0)
        {
            err_code = sd_ble_gatts_service_changed(m_conn_handle, APP_SERVICE_HANDLE_START, BLE_HANDLE_MAX);
            if ((err_code != NRF_SUCCESS) &&
                (err_code != BLE_ERROR_INVALID_CONN_HANDLE) &&
                (err_code != NRF_ERROR_INVALID_STATE) &&
                (err_code != BLE_ERROR_NO_TX_PACKETS) &&
                (err_code != NRF_ERROR_BUSY) &&
                (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
            {
                APP_ERROR_HANDLER(err_code);
            }
        }

        err_code = dm_application_context_delete(p_handle);
        APP_ERROR_CHECK(err_code);
    }
    else if (err_code == DM_NO_APP_CONTEXT)
    {
        // No context available. Ignore.
    }
    else
    {
        APP_ERROR_HANDLER(err_code);
    }
}


/** @snippet [DFU BLE Reset prepare] */
/**@brief Function for preparing for system reset.
 *
 * @details This function implements @ref dfu_app_reset_prepare_t. It will be called by
 *          @ref dfu_app_handler.c before entering the bootloader/DFU.
 *          This allows the current running application to shut down gracefully.
 */
static void reset_prepare(void)
{
    uint32_t err_code;

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        // Disconnect from peer.
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
				//stop eddystone
        app_beacon_stop();
    }
    else
    {
        // If not connected, the device will be advertising. Hence stop the advertising.
        advertising_stop();
				app_beacon_stop();
    }

    err_code = ble_conn_params_stop();
    APP_ERROR_CHECK(err_code);

    nrf_delay_ms(500);
}
/** @snippet [DFU BLE Reset prepare] */
#endif // BLE_DFU_APP_SUPPORT

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
	uint32_t err_code;
	
	/* Initialise timer module */
	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
	
  // Create timers.
  err_code = app_timer_create(&m_battery_timer_id,
                              APP_TIMER_MODE_REPEATED,
                              battery_level_meas_timeout_handler);
  if (err_code == NRF_SUCCESS)
		debug_printf("Battery timer initialised!\r\n");
	else
	{
		debug_printf("Error with initialising battery timer.\r\n");
		APP_ERROR_CHECK(err_code);
	}
	
	err_code = app_timer_create(&m_temperature_timer_id,
                              APP_TIMER_MODE_REPEATED,
                              temperature_timeout_handler);
	
	err_code = app_timer_create(&m_door_timer_id,
                              APP_TIMER_MODE_REPEATED,
                              door_timeout_handler);
		    
}

/**@brief Function for starting application timers.
 */
static void application_timers_start(void)
{
		uint32_t err_code;
		
		// Start application timers.
    err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
    if (err_code == NRF_SUCCESS)
			debug_printf("Battery timer started!\r\n");
		else
		{
			debug_printf("Error with starting battery timer.\r\n");
			APP_ERROR_CHECK(err_code);
		}
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

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_UNKNOWN);
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
		
		sd_ble_gap_tx_power_set(4);
}

/**@brief Function for initializing services that will be used by the application.
 *
 * @details Initialize the Heart Rate, Battery and Device Information services.
 */
static void services_init(void)
{
		uint32_t       err_code;
		
		ble_bas_init_t 	bas_init;
		ble_dis_init_t 	dis_init;
		
		// Initialize Battery Service.
    memset(&bas_init, 0, sizeof(bas_init));

    // Here the sec level for the Battery Service can be changed/increased.
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init.battery_level_char_attr_md.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_report_read_perm);

    bas_init.evt_handler          = NULL;
    bas_init.support_notification = true;
    bas_init.p_report_ref         = NULL;
    bas_init.initial_batt_level   = 100;

    err_code = ble_bas_init(&m_bas, &bas_init);
    if (err_code == NRF_SUCCESS)
			debug_printf("Battery service initialised!\r\n");
		else
		{
			debug_printf("Error with initialising battery service.\r\n");
			APP_ERROR_CHECK(err_code);
		}

    // Initialize Device Information Service.
    memset(&dis_init, 0, sizeof(dis_init));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);

    err_code = ble_dis_init(&dis_init);
    if (err_code == NRF_SUCCESS)
			debug_printf("Device Information service initialised!\r\n");
		else
		{
			debug_printf("Error with initialising device information service.\r\n");
			APP_ERROR_CHECK(err_code);
		}		
		
		// Initialize CCH custom Service.
		cch_service_init(&m_cch_service);
		
		#ifdef BLE_DFU_APP_SUPPORT
    /** @snippet [DFU BLE Service initialization] */
    ble_dfu_init_t   dfus_init;

    // Initialize the Device Firmware Update Service.
    memset(&dfus_init, 0, sizeof(dfus_init));

    dfus_init.evt_handler   = dfu_app_on_dfu_evt;
    dfus_init.error_handler = NULL;
    dfus_init.evt_handler   = dfu_app_on_dfu_evt;
    dfus_init.revision      = DFU_REVISION;

    err_code = ble_dfu_init(&m_dfus, &dfus_init);
    APP_ERROR_CHECK(err_code);

    dfu_app_reset_prepare_set(reset_prepare);
    dfu_app_dm_appl_instance_set(m_app_handle);
    /** @snippet [DFU BLE Service initialization] */
#endif // BLE_DFU_APP_SUPPORT
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
            app_timer_start(m_temperature_timer_id, TEMPERATURE_MEAS_INTERVAL, NULL);
						app_timer_start(m_door_timer_id, DOOR_MEAS_INTERVAL, NULL);
            break;

        case BLE_GAP_EVT_DISCONNECTED:

						app_beacon_start();
            // when not using the timeslot implementation, it is necessary to initialize the advertizing data again.
            advertising_init();
            err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
            APP_ERROR_CHECK(err_code);
						app_timer_stop(m_temperature_timer_id);
						app_timer_stop(m_door_timer_id);
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
#ifdef BLE_DFU_APP_SUPPORT
    /** @snippet [Propagating BLE Stack events to DFU Service] */
    ble_dfu_on_ble_evt(&m_dfus, p_ble_evt);
    /** @snippet [Propagating BLE Stack events to DFU Service] */
#endif // BLE_DFU_APP_SUPPORT
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
		ble_cch_service_on_ble_evt(&m_cch_service, p_ble_evt);
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
		
		
	#ifdef BLE_DFU_APP_SUPPORT
		ble_enable_params.gatts_enable_params.service_changed = 1;
	#endif // BLE_DFU_APP_SUPPORT
    
    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);
    
		//memset(&ble_enable_params, 0, sizeof(ble_enable_params));
		ble_enable_params.common_enable_params.vs_uuid_count   = 2;
    //ble_enable_params.gatts_enable_params.attr_tab_size = 0x0900; //changing stack size?
		
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
	#ifdef BLE_DFU_APP_SUPPORT
    if (p_event->event_id == DM_EVT_LINK_SECURED)
    {
        app_context_load(p_handle);
    }
#endif // BLE_DFU_APP_SUPPORT
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

/**@brief Function for initializing Beacon advertiser.
 */
static void beacon_adv_init(void)
{ 
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
	adc_init();
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
