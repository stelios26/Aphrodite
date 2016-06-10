/*  CCH Beacon
 *  Copyright (c) 2016 Stelios Ioakim.
 *  All Rights Reserved.
 *
 *  Developed by s.ioakim@engino.net
 */

/**
 *  @file
 *  @brief       BLE service CCH
 *  @addtogroup  CCH-beacon
 *
 *  @{
 */

#include <stdint.h>
#include <string.h>

#include "nrf_gpio.h"
#include "ble_cch.h"
#include "ble_srv_common.h"
#include "app_error.h"

static void on_ble_write(ble_cch_t * p_cch_service, ble_evt_t * p_ble_evt)
{
    // Decclare buffer variable to hold received data. The data can only be 32 bit long.
    uint32_t data_buffer;
    
		// Pupulate ble_gatts_value_t structure to hold received data and metadata.
    ble_gatts_value_t rx_data;
    rx_data.len = sizeof(uint32_t);
    rx_data.offset = 0;
    rx_data.p_value = (uint8_t*)&data_buffer;
    
    // Check if write event is performed on our characteristic or the CCCD
    if(p_ble_evt->evt.gatts_evt.params.write.handle == p_cch_service->char_handles.value_handle)
    {
        // Get data
        sd_ble_gatts_value_get(p_cch_service->conn_handle, p_cch_service->char_handles.value_handle, &rx_data);
    }
    else if(p_ble_evt->evt.gatts_evt.params.write.handle == p_cch_service->char_handles.cccd_handle)
    {
        // Get data
        sd_ble_gatts_value_get(p_cch_service->conn_handle, p_cch_service->char_handles.cccd_handle, &rx_data);
    }
}

void ble_cch_service_on_ble_evt(ble_cch_t * p_cch_service, ble_evt_t * p_ble_evt)
{

    // OUR_JOB: Step 3.D Implement switch case handling BLE events related to our service. 
    switch (p_ble_evt->header.evt_id)
    {        
        case BLE_GATTS_EVT_WRITE:
            on_ble_write(p_cch_service, p_ble_evt);
            break;
        case BLE_GAP_EVT_CONNECTED:
            p_cch_service->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            p_cch_service->conn_handle = BLE_CONN_HANDLE_INVALID;
            break;
        default:
            // No implementation needed.
            break;
    }
}

static uint32_t cch_char_add(ble_cch_t * p_cch_service)
{
    uint32_t   err_code = 0; // Variable to hold return codes from library and softdevice functions
    
    // Add a custom characteristic UUID
    ble_uuid_t          char_uuid;
    ble_uuid128_t       base_uuid = BLE_UUID_CCH_BASE_UUID;
    
		char_uuid.uuid      = BLE_UUID_DOOR_CHARACTERISTC_UUID;
    sd_ble_uuid_vs_add(&base_uuid, &char_uuid.type);
    APP_ERROR_CHECK(err_code);
    
    ble_gatts_char_md_t char_md;
    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read = 1;
    char_md.char_props.write = 1;

    
    // Configuring Client Characteristic Configuration Descriptor metadata and add to char_md structure
    ble_gatts_attr_md_t cccd_md;
    memset(&cccd_md, 0, sizeof(cccd_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc                = BLE_GATTS_VLOC_STACK;    
    char_md.p_cccd_md           = &cccd_md;
    char_md.char_props.notify   = 1;
   
    
    // Configure the attribute metadata
    ble_gatts_attr_md_t attr_md;
    memset(&attr_md, 0, sizeof(attr_md)); 
    attr_md.vloc        = BLE_GATTS_VLOC_STACK;   
    
    // Set read/write security levels to our characteristic
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    
    
    // Configure the characteristic value attribute
    ble_gatts_attr_t    attr_char_value;
    memset(&attr_char_value, 0, sizeof(attr_char_value));        
    attr_char_value.p_uuid      = &char_uuid;
    attr_char_value.p_attr_md   = &attr_md;
    
    // OUR_JOB: Step 2.H, Set characteristic length in number of bytes
    attr_char_value.max_len     = 1;
    attr_char_value.init_len    = 1;
    uint8_t value            		= 0x78;
    attr_char_value.p_value     = &value;

    // OUR_JOB: Step 2.E, Add our new characteristic to the service
    err_code = sd_ble_gatts_characteristic_add(p_cch_service->service_handle,
                                       &char_md,
                                       &attr_char_value,
                                       &p_cch_service->char_handles);
    APP_ERROR_CHECK(err_code);
    
    return NRF_SUCCESS;
}

void cch_service_init(ble_cch_t * p_cch_service)
{
    uint32_t   err_code; // Variable to hold return codes from library and softdevice functions

    //Declare 16-bit service and 128-bit base UUIDs and add them to the BLE stack
    ble_uuid_t        service_uuid;
    ble_uuid128_t     base_uuid = BLE_UUID_CCH_BASE_UUID;
    service_uuid.uuid = BLE_UUID_CCH_SERVICE_UUID;
    err_code = sd_ble_uuid_vs_add(&base_uuid, &service_uuid.type);
    APP_ERROR_CHECK(err_code);    
    
    // OUR_JOB: Step 3.B, Set our service connection handle to default value. I.e. an invalid handle since we are not yet in a connection.
    p_cch_service->conn_handle = BLE_CONN_HANDLE_INVALID;

    // FROM_SERVICE_TUTORIAL: Add our service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &service_uuid,
                                        &p_cch_service->service_handle);
    
    APP_ERROR_CHECK(err_code);
    
    // OUR_JOB: Call the function our_char_add() to add our new characteristic to the service. 
    cch_char_add(p_cch_service);
}

void cch_termperature_characteristic_update(ble_cch_t *p_cch_service, int32_t *temperature_value)
{
    // Update characteristic value
    if (p_cch_service->conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        uint16_t               len = 1;
        ble_gatts_hvx_params_t hvx_params;
        memset(&hvx_params, 0, sizeof(hvx_params));

        hvx_params.handle = p_cch_service->char_handles.value_handle;
        hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset = 0;
        hvx_params.p_len  = &len;
        hvx_params.p_data = (uint8_t*)temperature_value;  

        sd_ble_gatts_hvx(p_cch_service->conn_handle, &hvx_params);
    }   
}

