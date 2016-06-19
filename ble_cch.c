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
    if(p_ble_evt->evt.gatts_evt.params.write.handle == p_cch_service->charT_handles.value_handle)
    {
        // Get data
        sd_ble_gatts_value_get(p_cch_service->conn_handle, p_cch_service->charT_handles.value_handle, &rx_data);
    }
    else if(p_ble_evt->evt.gatts_evt.params.write.handle == p_cch_service->charT_handles.cccd_handle)
    {
        // Get data
        sd_ble_gatts_value_get(p_cch_service->conn_handle, p_cch_service->charT_handles.cccd_handle, &rx_data);
    }
		else if(p_ble_evt->evt.gatts_evt.params.write.handle == p_cch_service->charD_handles.value_handle)
    {
        // Get data
        sd_ble_gatts_value_get(p_cch_service->conn_handle, p_cch_service->charD_handles.value_handle, &rx_data);
    }
    else if(p_ble_evt->evt.gatts_evt.params.write.handle == p_cch_service->charD_handles.cccd_handle)
    {
        // Get data
        sd_ble_gatts_value_get(p_cch_service->conn_handle, p_cch_service->charD_handles.cccd_handle, &rx_data);
    }
}

void ble_cch_service_on_ble_evt(ble_cch_t * p_cch_service, ble_evt_t * p_ble_evt)
{

    // Implement switch case handling BLE events related to our service. 
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
    
		// ADD TEMP
    ble_uuid_t          charT_uuid;
    ble_uuid128_t       base_uuid = BLE_UUID_CCH_BASE_UUID;
    ble_gatts_char_md_t charT_md;
		ble_gatts_attr_md_t cccdT_md;
    ble_gatts_attr_md_t attrT_md;
    ble_gatts_attr_t    attr_charT_value;
		uint8_t valueT;
	
		// Add a custom characteristic UUID
		charT_uuid.uuid      = BLE_UUID_TEMP_CHARACTERISTC_UUID;
    sd_ble_uuid_vs_add(&base_uuid, &charT_uuid.type);
    APP_ERROR_CHECK(err_code);
       
    memset(&charT_md, 0, sizeof(charT_md));
    charT_md.char_props.read = 1;
    charT_md.char_props.write = 1;
    
    // Configuring Client Characteristic Configuration Descriptor metadata and add to char_md structure
   
    memset(&cccdT_md, 0, sizeof(cccdT_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccdT_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccdT_md.write_perm);
    cccdT_md.vloc                = BLE_GATTS_VLOC_STACK;    
    charT_md.p_cccd_md           = &cccdT_md;
    charT_md.char_props.notify   = 1;
   
    // Configure the attribute metadata
    memset(&attrT_md, 0, sizeof(attrT_md)); 
    attrT_md.vloc        = BLE_GATTS_VLOC_STACK;   
    
    // Set read/write security levels to our characteristic
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attrT_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attrT_md.write_perm);
    
    
    // Configure the characteristic value attribute
    memset(&attr_charT_value, 0, sizeof(attr_charT_value));        
    attr_charT_value.p_uuid      = &charT_uuid;
    attr_charT_value.p_attr_md   = &attrT_md;
    
    // Set characteristic length in number of bytes
    attr_charT_value.max_len     = 1;
    attr_charT_value.init_len    = 1;
    valueT            					 = 0x78;
    attr_charT_value.p_value     = &valueT;

    // Add our new characteristic to the service
    err_code = sd_ble_gatts_characteristic_add(p_cch_service->service_handle,
                                       &charT_md,
                                       &attr_charT_value,
                                       &p_cch_service->charT_handles);
    APP_ERROR_CHECK(err_code);
			
		
		//ADD DOOR
		ble_uuid_t          charD_uuid;
    ble_gatts_char_md_t charD_md;
		ble_gatts_attr_md_t cccdD_md;
    ble_gatts_attr_md_t attrD_md;
    ble_gatts_attr_t    attr_charD_value;
		uint8_t valueD;
	
		// Add a custom characteristic UUID
		charD_uuid.uuid      = BLE_UUID_DOOR_CHARACTERISTC_UUID;
    sd_ble_uuid_vs_add(&base_uuid, &charD_uuid.type);
    APP_ERROR_CHECK(err_code);
       
    memset(&charD_md, 0, sizeof(charD_md));
    charD_md.char_props.read = 1;
    charD_md.char_props.write = 1;
    
    // Configuring Client Characteristic Configuration Descriptor metadata and add to char_md structure
    memset(&cccdD_md, 0, sizeof(cccdD_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccdD_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccdD_md.write_perm);
    cccdD_md.vloc                = BLE_GATTS_VLOC_STACK;    
    charD_md.p_cccd_md           = &cccdD_md;
    charD_md.char_props.notify   = 1;
   
    // Configure the attribute metadata
    memset(&attrD_md, 0, sizeof(attrD_md)); 
    attrD_md.vloc        = BLE_GATTS_VLOC_STACK;   
    
    // Set read/write security levels to our characteristic
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attrD_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attrD_md.write_perm);
    
    
    // Configure the characteristic value attribute
    memset(&attr_charD_value, 0, sizeof(attr_charD_value));        
    attr_charD_value.p_uuid      = &charD_uuid;
    attr_charD_value.p_attr_md   = &attrD_md;
    
    // Set characteristic length in number of bytes
    attr_charD_value.max_len     = 1;
    attr_charD_value.init_len    = 1;
    valueD            					 = 0x7F;
    attr_charD_value.p_value     = &valueD;

    // Add our new characteristic to the service
    err_code = sd_ble_gatts_characteristic_add(p_cch_service->service_handle,
                                       &charD_md,
                                       &attr_charD_value,
                                       &p_cch_service->charD_handles);
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
    
    // Set our service connection handle to default value. I.e. an invalid handle since we are not yet in a connection.
    p_cch_service->conn_handle = BLE_CONN_HANDLE_INVALID;

    // Add our service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &service_uuid,
                                        &p_cch_service->service_handle);
    
    APP_ERROR_CHECK(err_code);
    
    // Call the function our_char_add() to add our new characteristic to the service. 
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

        hvx_params.handle = p_cch_service->charT_handles.value_handle;
        hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset = 0;
        hvx_params.p_len  = &len;
        hvx_params.p_data = (uint8_t*)temperature_value;  

        sd_ble_gatts_hvx(p_cch_service->conn_handle, &hvx_params);
    }   
}

void cch_door_characteristic_update(ble_cch_t *p_cch_service, uint8_t *door_value)
{
    // Update characteristic value
    if (p_cch_service->conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        uint16_t               len = 1;
        ble_gatts_hvx_params_t hvx_params;
        memset(&hvx_params, 0, sizeof(hvx_params));

        hvx_params.handle = p_cch_service->charD_handles.value_handle;
        hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset = 0;
        hvx_params.p_len  = &len;
        hvx_params.p_data = (uint8_t*)door_value;  

        sd_ble_gatts_hvx(p_cch_service->conn_handle, &hvx_params);
    }   
}

