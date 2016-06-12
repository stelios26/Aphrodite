/*  CCH Beacon
 *  Copyright (c) 2016 Stelios Ioakim.
 *  All Rights Reserved.
 *
 *  Developed by s.ioakim@engino.net
 */

#ifndef __ble_cch_h
#define __ble_cch_h

#include <stdint.h>
#include "ble.h"
#include "ble_srv_common.h"

#define BLE_UUID_CCH_BASE_UUID              {{0xAC, 0xFA, 0x23, 0xAA, 0x33, 0x78, 0xA3, 0x19, 0xAA, 0xF4, 0x44, 0xA9, 0x22, 0xA9, 0x3F, 0xFD}} // 128-bit base UUID
#define BLE_UUID_CCH_SERVICE_UUID           0xCFBA 

#define BLE_UUID_DOOR_CHARACTERISTC_UUID     0xD003 // Just a random, but recognizable value
#define BLE_UUID_TEMP_CHARACTERISTC_UUID		 0xFA12
typedef struct
{
    uint16_t                    conn_handle;    /**< Handle of the current connection (as provided by the BLE stack, is BLE_CONN_HANDLE_INVALID if not in a connection).*/
    uint16_t                    service_handle; /**< Handle of Our Service (as provided by the BLE stack). */
    // Add handles for the characteristic attributes to our struct
    ble_gatts_char_handles_t    charT_handles;
    ble_gatts_char_handles_t    charD_handles;
    
}ble_cch_t;
 
void ble_cch_service_on_ble_evt(ble_cch_t * p_cch_service, ble_evt_t * p_ble_evt);

void cch_service_init(ble_cch_t * p_our_service);

void cch_termperature_characteristic_update(ble_cch_t *p_cch_service, int32_t *temperature_value);

void cch_door_characteristic_update(ble_cch_t *p_cch_service, uint8_t *door_value);

/** @} */
#endif
