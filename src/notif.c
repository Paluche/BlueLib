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

#include "bluelib.h"

#include <malloc.h>
#include <bluetooth/uuid.h>

#include "att.h"
#include "gatt_def.h"

#define printf(...) printf("[NOTIF] " __VA_ARGS__)

extern GAttrib *attrib;

// Add a notification by UUID.
int bl_add_notif(char *uuid_str, bl_primary_t *bl_primary,
    GAttribNotifyFunc func, void *user_data, uint8_t opcode)
{
  GError *gerr = NULL;

  // Get the characteristic associated to the UUID
  bl_char_t *bl_char= bl_get_char(uuid_str, bl_primary, &gerr);

  if (gerr) {
    printf("%s\n", gerr->message);
    return gerr->code;
  }

  if (has_event_by_uuid(attrib, uuid_str)) {
    printf("Notification substitute\n");
    g_attrib_unregister(attrib, uuid_str);
  }
  int ret = bl_add_notif_by_char(bl_char, NULL, bl_primary, func, user_data,
      opcode);
  bl_char_free(bl_char);
  return ret;
}

// Add notification by charasteristic.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
int bl_add_notif_by_char(bl_char_t *start_bl_char, bl_char_t *end_bl_char,
    bl_primary_t *bl_primary, GAttribNotifyFunc func, void *user_data,
    uint8_t opcode)
{
  GError *gerr = NULL;
  uint8_t value;
  if (gerr)
   goto gerror;

  if ((!start_bl_char) ||
      ((opcode == ATT_OP_HANDLE_NOTIFY) &&
       !(start_bl_char->properties & ATT_CHAR_PROPER_NOTIFY)) ||
       ((opcode == ATT_OP_HANDLE_IND) &&
       !(start_bl_char->properties & ATT_CHAR_PROPER_INDICATE)))
    goto error;

  // Register to the notification
  bl_desc_t *client_char_conf = bl_get_desc_by_char(start_bl_char,
      end_bl_char, bl_primary, GATT_CLIENT_CHARAC_CFG_UUID_STR, &gerr);

  if (gerr)
    goto gerror;

  if (!client_char_conf)
    goto error;

  att_put_u16((opcode == ATT_OP_HANDLE_IND) ?
      GATT_CLIENT_CHARAC_CFG_IND_BIT :
      GATT_CLIENT_CHARAC_CFG_NOTIF_BIT, &value);

  if (bl_write_desc_by_desc(client_char_conf, &value, 2))
    goto error;

  if (!g_attrib_register(attrib, opcode, start_bl_char->uuid_str,
        start_bl_char->value_handle, func, attrib, user_data)) {
    printf("Malloc error");
    return BL_MALLOC_ERROR;
  }

  if (client_char_conf)
    bl_desc_free(client_char_conf);
  return BL_NO_ERROR;

error:
  if (client_char_conf)
    bl_desc_free(client_char_conf);
  if (opcode == ATT_OP_HANDLE_IND) {
    printf("Characteristic not indicable\n");
    return BL_NOT_INDICABLE_ERROR;
  } else {
    printf("Characteristic not notifiable\n");
    return BL_NOT_NOTIFIABLE_ERROR;
  }
gerror:
  printf("%s\n", gerr->message);
  return gerr->code;
}

// Retrieve a UUID from a handle.
char *bl_get_notif_uuid(uint16_t handle)
{
  return event_get_uuid_by_handle(attrib, handle);
}

// Remove a notification by UUID.
int bl_remove_notif(char *uuid_str)
{
  if (!attrib)
    return BL_DISCONNECTED_ERROR;

  g_attrib_unregister(attrib, uuid_str);
  return BL_NO_ERROR;
}

// Remove a notification by characteristic.
int bl_remove_notif_by_char(bl_char_t *bl_char)
{
  if (!attrib)
    return BL_DISCONNECTED_ERROR;

  char *uuid_str = bl_get_notif_uuid(bl_char->handle);

  g_attrib_unregister(attrib, uuid_str);
  return BL_NO_ERROR;
}

// Remove all notification registered.
int bl_remove_all_notif(void)
{
  if (!attrib)
    return BL_DISCONNECTED_ERROR;

  g_attrib_unregister_all(attrib);
  return BL_NO_ERROR;
}

// Print the notification list currently registered.
void bl_notif_list_print(void)
{
  event_list_print(attrib);
}

void bl_notif_indication_resp(void)
{
  int16_t  olen;
	uint8_t *opdu;
  size_t   plen;
  opdu = g_attrib_get_buffer(attrib, &plen);
	olen = enc_confirmation(opdu, plen);

	if (olen > 0)
		g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, NULL);
}
