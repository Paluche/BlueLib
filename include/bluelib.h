/*
 *  BlueLib - Abstraction layer for Bluetooth Low Energy softwares
 *
 *  Copyright (C) 2013  Netatmo
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _BLUELIB_H_
#define _BLUELIB_H_

#include <stdint.h>
#include <glib.h>
#include <errno.h>

// Bluez
#include "gattrib.h"
#include "uuid.h"
#include "att.h"

// BlueLib
#include "bluelib_gatt.h"

// Bluelib errors.
// Every function of bluelib that doesn't return an error status int,
// take a pointer of GError pointer as argument. In case of an error the
// pointer won't be NULL and point to a *GError.
// Bluelib also use the errors given by BlueZ which are defined in errno.h.
#define BL_ERROR_DOMAIN                bl_error_domain()
GQuark bl_error_domain(void);

#define BL_NO_ERROR                     0
#define BL_MALLOC_ERROR                -1
#define BL_DISCONNECTED_ERROR          -2
#define BL_NO_CALLBACK_ERROR           -3
#define BL_HANDLE_ORDER_ERROR          -4
#define BL_NOT_INIT_ERROR              -5
#define BL_ALREADY_CONNECTED_ERROR     -7
#define BL_LE_ONLY_ERROR               -8
#define BL_MTU_ALREADY_EXCHANGED_ERROR -9
#define BL_MISSING_ARGUMENT_ERROR     -10
#define BL_SEND_REQUEST_ERROR         -11
#define BL_RECONNECTION_NEEDED_ERROR  -12
#define BL_UNICITY_ERROR              -15
#define BL_REQUEST_FAIL_ERROR         -16
#define BL_PROTOCOL_ERROR             -17
#define BL_NOT_NOTIFIABLE_ERROR       -18
#define BL_NOT_INDICABLE_ERROR        -19

#define INVALID_HANDLE             0x0000

// Bluetooth Low Energy
//
// Example of architecture
//
// Handle          |
// Start service A | Primary A
//                 | | Include of A
// Char A.1 handle | | Characteristic A.1
//                 | | | Descriptor A.1.a
//                 | | | Descriptor A.1.b
//                 | | | Descriptor A.1.c
// Char A.2 handle | |
//                 | | Characteristic A.2
//                 | | | Descriptor A.2.a
// End service A   | | | Descriptor A.2.b
//                 |
// Start service B | Primary B
// End service B   | | Characteristic B.1
//

// NOTES:
// - For every function that ask for a primary service, if you do not
//   specify the primary service (setting it to NULL) the search will be made
//   among all handlers.
//
// - The Handles can change anytime be careful using structures type bl_*_t.
//   All of them has handle in it and every function that use them can
//   work only if we are asssured that the handles mapping haven't changed.
//
// - In order to know when the handle mapping has changed. I recommend you
//   to subsribe to the notification "service changed" (correspondant UUID
//   defined in gatt_def.h).
//
// - Also notifications are based on handles and may need to be reassigned in
//   case the service changed.

/********************** Initialisation of the context **********************/
// NOTE: Set the arguments to (NULL, NULL, NULL, 0, 0) for default values.
int bl_init(const char *src, const char *dst, const char *dst_type, int psm,
    const int sec_level);


/******************** Connect/Disconnect from a device *********************/
// Connect to a device.
int bl_connect(char *mac_dst, char *dst_type);

// Disconnect from the device, delete the nofication list.
int bl_disconnect(void);

typedef int (user_cb_fct_t)(void);
// Set a function to call each time you succeed to connect.
// If the connection go well, the return value of bl_connect will be the one
// of user_cb_fct_t.
int bl_set_connect_cb(user_cb_fct_t *func);


/********************* Get the state of the connection *********************/
typedef enum state {
  STATE_DISCONNECTED,
  STATE_CONNECTING,
  STATE_CONNECTED
} conn_state_t;

conn_state_t get_conn_state(void);


/*************************** Get Primary Service ***************************/
// Get a specific primary service.
// Return the primary service associated to this UUID, if unique.
bl_primary_t *bl_get_primary(char *uuid_str, GError **gerr);

// Get all the primary service associated of an UUID.
// Return a list of primary services (bl_primary_t *).
GSList *bl_get_all_primary(char *uuid_str, GError **gerr);

// Get all the primary services of a device.
// Return a list of primary services (bl_primary_t *).
GSList *bl_get_all_primary_device(GError **gerr);


/************************* Get Included Services ***************************/
// Get all the included services of a primary service.
// Returns a list of included services (bl_included_t *).
GSList *bl_get_included(bl_primary_t *bl_primary, GError **gerr);


/************************** Get characteristics ****************************/
// Get a specific characteristic associated to an UUID on a primary service.
// Returns the characteristic associated to this uuid, if unique.
bl_char_t *bl_get_char(char *uuid_str, bl_primary_t *bl_primary,
    GError **gerr);

// Get all characteristics associated to an UUID on a primary service.
// Returns a list of characteristics (bl_char_t *) associated to the UUID
GSList *bl_get_all_char(char *uuid_str, bl_primary_t *bl_primary,
    GError **gerr);

// Get all characteristics on a primary service.
// Returns a list of characteristics (bl_char_t *).
GSList *bl_get_all_char_in_primary(bl_primary_t *bl_primary, GError **gerr);


/***************************** Get descriptors *****************************/
// Search a specific descriptor of the unique characteristic associated to
// the UUID on a primary service.
// Returns the characteristic descriptor if found, else NULL.
bl_desc_t *bl_get_desc(char *char_uuid_str, bl_primary_t *bl_primary,
  char *desc_uuid_str, GError **gerr);

// Get all the descriptors of the unique characteristic associated to the
// UUID on a primary service.
// Returns a list of characteristic descriptor (bl_desc_t *).
GSList *bl_get_all_desc(char *uuid_str, bl_primary_t *bl_primary,
    GError **gerr);

// Search a specific descriptor of the specified characteristic.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
// Returns the characteristic descriptor if found, else NULL.
bl_desc_t *bl_get_desc_by_char(bl_char_t *start_bl_char,
    bl_char_t *end_bl_char, bl_primary_t *bl_primary, char *desc_uuid_str,
    GError **gerr);

// Get all the descriptors of a specified characteristic on a primary
// service.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
// Returns a list of characteristic descriptor (bl_desc_t *).
GSList *bl_get_all_desc_by_char(bl_char_t *start_bl_char,
    bl_char_t *end_bl_char, bl_primary_t *bl_primary, GError **gerr);


/************************** Read characteristic value **********************/
// NOTE: Read functions by UUID doesn't supply the blob readings. Use the
// dedicated functions for blob reading by UUID.
// Read a characteristic value by UUID on a primary service.
bl_value_t *bl_read_char(char *uuid_str, bl_primary_t *bl_primary,
    GError **gerr);

// Read all the characteristics value associated to this UUID.
// Return a list of values (bl_value_t *).
GSList *bl_read_char_all(char *uuid_str, bl_primary_t *bl_primary,
    GError **gerr);

// Equivalent to bl_read_char but supplying the blob reading.
bl_value_t *bl_read_char_blob(char *uuid_str, bl_primary_t *bl_primary,
  GError **gerr);

// Equivalent to bl_read_char_all but supplying the blob reading.
// Return a list of values (bl_value_t *).
GSList *bl_read_char_all_blob(char *uuid_str, bl_primary_t *bl_primary,
    GError **gerr);

// Read a characteristic value of a characteristic.
bl_value_t *bl_read_char_by_char(bl_char_t *bl_char, GError **gerr);


/******************************* Read descriptor ***************************/
// Read a descriptor of a characteristic by UUID on a primary service.
bl_value_t *bl_read_desc(char *char_uuid_str, bl_primary_t *bl_primary,
    char *desc_uuid_str, GError **gerr);

// Read all the descriptors a characteristic by UUID on a primary service.
// Return a list of values (bl_value_t *).
GSList *bl_read_all_desc(char *char_uuid_str, bl_primary_t *bl_primary,
    GError **gerr);

// Read descriptor by descriptor.
bl_value_t *bl_read_desc_by_desc(bl_desc_t *bl_desc, GError **gerr);

// Read descriptor by characteristic.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
bl_value_t *bl_read_desc_by_char(bl_char_t *start_bl_char,
    bl_char_t *end_bl_char, bl_primary_t *bl_primary, char *desc_uuid_str,
    GError **gerr);


/************************** Write characteristic value *********************/
// Choose what type of write do you want to have:
#define WRITE_REQ 1 // Request, an ACK is received to confirm the write.
#define WRITE_CMD 0 // Command: no ACK in return.

// Write a characteristic value by UUID on a primary service
int bl_write_char(char *uuid_str, bl_primary_t *bl_primary, uint8_t *value,
    size_t size, int type);

// Write a characteristic value by characteristic.
int bl_write_char_by_char(bl_char_t *bl_char, uint8_t *value, size_t size,
    int type);


/**************************** Write descriptor *****************************/
// Write a descriptor of a characteristic by UUID on a primary service.
int bl_write_desc(char *char_uuid_str, bl_primary_t *bl_primary,
    char *desc_uuid_str, uint8_t *value, size_t size);

// Write a descriptor by bl_desc_t.
int bl_write_desc_by_desc(bl_desc_t *bl_desc, uint8_t *value, size_t size);

// Write a descriptor in a characteristic.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
int bl_write_desc_by_char(bl_char_t *start_bl_char, bl_char_t *end_bl_char,
    bl_primary_t *bl_primary, char *desc_uuid_str, uint8_t *value,
    size_t size);


/*************************** Set security level ****************************/
#define SECURITY_LEVEL_LOW    0 // Default
#define SECURITY_LEVEL_MEDIUM 1
#define SECURITY_LEVEL_HIGH   2
int bl_change_sec_level(int level);


/************************* Change MTU for GATT/ATT *************************/
int bl_change_mtu(int value);


/****************************** Notifications *******************************
 * NOTE: The notification list is part of the variable "attrib" which is
 * allocated at each connection. And free at each deconnection. Even not
 * planned.
 *
 * GAttribResultFunc:
 *  typedef void (*GAttribResultFunc) (const guint8 *pdu, guint16 len,
 *      gpointer user_data);
 *
 * pdu[0]:    The type of the notification. Notification or indication.
 * pdu[1->2]: The handle. Use the functon att_get_u16 to retrieve it.
 * pdu[3]:    First byte of the data.
 * len:       Length of pdu buffer.
 * user_data: The pointer that you gave when you added the notification.
 *
 * Opcodes:
 *  ATT_OP_HANDLE_NOTIFY for a notification
 *  ATT_OP_HANDLE_IND    for a indication
 */
#define NOTIF_PDU_HEADER_SIZE 3

// Add a notification by UUID.
int bl_add_notif(char *uuid_str, bl_primary_t *bl_primary,
    GAttribNotifyFunc func, void *user_data, uint8_t opcode);

// Add notification by characteristic.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
int bl_add_notif_by_char(bl_char_t *start_bl_char, bl_char_t *end_bl_char,
    bl_primary_t *bl_primary, GAttribNotifyFunc func, void *user_data,
    uint8_t opcode);

// Retrieve a UUID from a handle.
char *bl_get_notif_uuid(uint16_t handle);

// Remove a notification by UUID.
int bl_remove_notif(char *uuid_str);

// Remove a notification by characteristic.
int bl_remove_notif_by_char(bl_char_t *bl_char);

// Remove all notification registered.
int bl_remove_all_notif(void);

// Print the notification list currently registered.
void bl_notif_list_print(void);

// If you receveiced an indication call this function in your callback to
// acknowledge the indication.
void bl_notif_indication_resp(void);

#endif
