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

#include <malloc.h>
#include <stdint.h>

#include <bluetooth/uuid.h>

#include "bluelib_gatt.h"

/*
 * Struct creators
 */
bl_primary_t *bl_primary_new(char *uuid_str, const gboolean changed,
    const uint16_t start_handle, const uint16_t end_handle)
{
  bl_primary_t *new_bl_primary = malloc(sizeof(bl_primary_t));

  if (new_bl_primary == NULL)
    return NULL;

  if (uuid_str)
    strcpy(new_bl_primary->uuid_str, uuid_str);
  else
    new_bl_primary->uuid_str[0] = '\0';
  memcpy(&new_bl_primary->changed,      &changed,      sizeof(gboolean));
  memcpy(&new_bl_primary->start_handle, &start_handle, sizeof(uint16_t));
  memcpy(&new_bl_primary->end_handle,   &end_handle,   sizeof(uint16_t));

  return new_bl_primary;
}

bl_included_t *bl_included_new(char *uuid_str, const uint16_t handle,
    const uint16_t start_handle, const uint16_t end_handle)
{
  bl_included_t *new_bl_included = malloc(sizeof(bl_included_t));

  if (new_bl_included == NULL)
   return NULL;

  if (uuid_str)
    strcpy(new_bl_included->uuid_str, uuid_str);
  else
    new_bl_included->uuid_str[0] = '\0';
  memcpy(&new_bl_included->handle,       &handle,       sizeof(uint16_t));
  memcpy(&new_bl_included->start_handle, &start_handle, sizeof(uint16_t));
  memcpy(&new_bl_included->end_handle,   &end_handle,   sizeof(uint16_t));

  return new_bl_included;
}

bl_char_t *bl_char_new(char *uuid_str, const uint16_t handle,
    const uint8_t properties, const uint16_t value_handle)
{
  bl_char_t *new_bl_char = malloc(sizeof(bl_char_t));
  if (new_bl_char == NULL)
    return NULL;

  if (uuid_str)
    strcpy(new_bl_char->uuid_str, uuid_str);
  else
    new_bl_char->uuid_str[0] = '\0';
  memcpy(&new_bl_char->handle,       &handle,       sizeof(uint16_t));
  memcpy(&new_bl_char->properties,   &properties,   sizeof(uint8_t));
  memcpy(&new_bl_char->value_handle, &value_handle, sizeof(uint16_t));

  return new_bl_char;
}

bl_desc_t *bl_desc_new(char *uuid_str, const uint16_t handle)
{
  bl_desc_t *new_bl_desc = malloc(sizeof(bl_desc_t));
  if (new_bl_desc == NULL)
    return NULL;

  if (uuid_str)
    strcpy(new_bl_desc->uuid_str, uuid_str);
  else
    new_bl_desc->uuid_str[0] = '\0';
  new_bl_desc->handle        = handle;
  return new_bl_desc;
}

bl_value_t *bl_value_new(char *uuid_str, const uint16_t handle,
    const size_t data_size, uint8_t *data)
{
  bl_value_t *new_bl_value = malloc(sizeof(bl_value_t));
  if (new_bl_value == NULL)
    return NULL;

  // Initialisation
  if (uuid_str)
    strcpy(new_bl_value->uuid_str, uuid_str);
  else
    new_bl_value->uuid_str[0] = '\0';

  memcpy(&new_bl_value->handle,    &handle,    sizeof(uint16_t));
  memcpy(&new_bl_value->data_size, &data_size, sizeof(size_t));
  new_bl_value->data = malloc(new_bl_value->data_size*sizeof(uint8_t));
  memcpy(new_bl_value->data, data, new_bl_value->data_size*sizeof(uint8_t));
  return new_bl_value;
}

/*
 * Struct copy
 */
bl_primary_t *bl_primary_cpy(bl_primary_t *bl_primary)
{
  if (bl_primary) {
    return bl_primary_new(bl_primary->uuid_str, bl_primary->changed,
        bl_primary->start_handle, bl_primary->end_handle);
  }
  return NULL;
}

bl_included_t *bl_included_cpy(bl_included_t *bl_included)
{
  if (bl_included) {
    return bl_included_new(bl_included->uuid_str, bl_included->handle,
        bl_included->start_handle, bl_included->end_handle);
  }
  return NULL;
}

bl_char_t *bl_char_cpy(bl_char_t *bl_char)
{
  if (bl_char) {
    return bl_char_new(bl_char->uuid_str, bl_char->handle,
        bl_char->properties, bl_char->value_handle);
  }
  return NULL;
}

bl_desc_t *bl_desc_cpy(bl_desc_t *bl_desc)
{
  if (bl_desc) {
    return bl_desc_new(bl_desc->uuid_str, bl_desc->handle);
  }
  return NULL;
}

bl_value_t *bl_value_cpy(bl_value_t *bl_value)
{
  if (bl_value) {
    return bl_value_new(bl_value->uuid_str, bl_value->handle,
        bl_value->data_size, bl_value->data);
  }
  return NULL;
}

/*
 * Struct destructors
 */
void struct_free(void *bl_struct)
{
  if (bl_struct) {
      free(bl_struct);
    bl_struct = NULL;
  }
}

void bl_value_free(bl_value_t *bl_value)
{
  if (bl_value) {
    if (bl_value->data)
      free(bl_value->data);
    free(bl_value);
    bl_value = NULL;
  }
}

static void free_func(gpointer data)
{
  if (data)
    free(data);
}

static void value_free_func(gpointer data)
{
  if (((bl_value_t *) data)->data)
    free(((bl_value_t *) data)->data);
  free_func(data);
}

void list_free(GSList *list)
{
  g_slist_free_full(list, free_func);
}

void bl_value_list_free(GSList *list)
{
  g_slist_free_full(list, value_free_func);
}

/*
 * Print function
 */
#define PRINT(file, ...)        \
  if (file)                     \
    fprintf(file, __VA_ARGS__); \
  else                          \
    printf(__VA_ARGS__);

void bl_primary_fprint(FILE *f, bl_primary_t *bl_primary)
{
  if (!bl_primary) {
    printf("ERROR: No data\n");
    return;
  }
  PRINT(f, "0x%04x | Primary: UUID: ", bl_primary->start_handle);
  if (bl_primary->uuid_str[0]) {
    PRINT(f, "%s", bl_primary->uuid_str);
  } else
    PRINT(f, "(nil)");
  PRINT(f, ", start handle: 0x%04x, end handle: 0x%04x\n",
      bl_primary->start_handle, bl_primary->end_handle);
}

void bl_included_fprint(FILE *f, bl_included_t *bl_included)
{
  if (!bl_included) {
    printf("ERROR: No data\n");
    return;
  }
  PRINT(f, "0x%04x | | Included: UUID: ", bl_included->handle);
  if (bl_included->uuid_str[0]) {
    PRINT(f, "%s", bl_included->uuid_str);
  } else
    PRINT(f, "(nil)");

  PRINT(f, "start handle 0x%04x, end handle 0x%04x\n",
      bl_included->start_handle, bl_included->end_handle);
}

void bl_char_fprint(FILE *f, bl_char_t *bl_char)
{
  if (!bl_char) {
    printf("ERROR: No data\n");
    return;
  }
  PRINT(f, "0x%04x | | Characteristic: UUID: ", bl_char->handle);
  if (bl_char->uuid_str[0]) {
    PRINT(f, "%s", bl_char->uuid_str);
  } else
    PRINT(f, "(nil)");
  PRINT(f, ", properties: 0x%04x, value handle: 0x%04x\n",
      bl_char->properties, bl_char->value_handle);
}

void bl_desc_fprint(FILE *f, bl_desc_t *bl_desc)
{
  if (!bl_desc) {
    printf("ERROR: No data\n");
    return;
  }
  PRINT(f, "0x%04x | | | Descriptor: UUID: ", bl_desc->handle);
  if (bl_desc->uuid_str[0]) {
    PRINT(f, "%s", bl_desc->uuid_str);
  } else
    PRINT(f, "(nil)");
  PRINT(f, "\n");
}

void bl_value_fprint(FILE *f, bl_value_t *bl_value)
{
  if (!bl_value) {
    printf("ERROR: No data\n");
    return;
  }
  PRINT(f, "Value: UUID: %s; handle: 0x%04x, ",
      bl_value->uuid_str, bl_value->handle);

  if (bl_value->data && bl_value->data_size) {
    PRINT(f, "size: %d, data: 0x",(int) bl_value->data_size);
    for (int i = 0; i < bl_value->data_size; i++)
      PRINT(f, "%02x", (uint8_t) bl_value->data[i]);
    PRINT(f, " \n");
  } else {
    PRINT(f, "No data <%p %lu>\n", bl_value->data, bl_value->data_size);
  }
}


void list_fprint(FILE *f, GSList *list, int type)
{
  if (list == NULL) {
    printf("ERROR: No data\n");
    return;
  }
  for (GSList *l = list; l; l = l->next) {
    if (l->data) {
      switch (type) {
        case 0:
          bl_primary_fprint(f, l->data);
          break;
        case 1:
          bl_included_fprint(f, l->data);
          break;
        case 2:
          bl_char_fprint(f, l->data);
          break;
        case 3:
          bl_desc_fprint(f, l->data);
          break;
        case 4:
          bl_value_fprint(f, l->data);
        default:
          break;
      }
    } else
        printf("ERROR: No data\n");
  }
}
