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

#ifndef _BLUELIB_GATT_H_
#define _BLUELIB_GATT_H_

#include <glib.h>

typedef struct {
  char        uuid_str[MAX_LEN_UUID_STR];
  gboolean    changed;
  uint16_t    start_handle;
  uint16_t    end_handle;
} bl_primary_t;

typedef struct {
  char        uuid_str[MAX_LEN_UUID_STR];
  uint16_t    handle;
  uint16_t    start_handle;
  uint16_t    end_handle;
} bl_included_t;

typedef struct {
  char        uuid_str[MAX_LEN_UUID_STR];
  uint16_t    handle;
  uint8_t     properties;
  uint16_t    value_handle;
} bl_char_t;

typedef struct {
  char        uuid_str[MAX_LEN_UUID_STR];
  uint16_t    handle;
} bl_desc_t;

typedef struct {
  char        uuid_str[MAX_LEN_UUID_STR];
  uint16_t    handle;
  size_t      data_size;
  uint8_t    *data;
} bl_value_t;


#define MAC_SZ 17

// Struct creators
bl_primary_t *bl_primary_new(char *uuid_str, gboolean changed,
    uint16_t start_handle, uint16_t end_handle);

bl_included_t *bl_included_new(char *uuid_str, const uint16_t handle,
    const uint16_t start_handle, const uint16_t end_handle);

bl_char_t *bl_char_new(char *uuid_str, const uint16_t handle,
    const uint8_t properties, const uint16_t value_handle);

bl_desc_t *bl_desc_new(char *uuid_str, const uint16_t handle);

bl_value_t *bl_value_new(char *uuid_str, const uint16_t handle,
    const size_t data_size, uint8_t *data);

// Struct copy
bl_primary_t  *bl_primary_cpy  (bl_primary_t  *bl_primary);
bl_included_t *bl_included_cpy (bl_included_t *bl_included);
bl_char_t     *bl_char_cpy     (bl_char_t     *bl_char);
bl_desc_t     *bl_desc_cpy     (bl_desc_t     *bl_desc);
bl_value_t    *bl_value_cpy    (bl_value_t    *bl_value);

// Struct destructors
void struct_free(void *bl_struct);
#define bl_primary_free(bl_primary)   struct_free(bl_primary)
#define bl_included_free(bl_included) struct_free(bl_included)
#define bl_char_free(bl_char)         struct_free(bl_char)
#define bl_desc_free(bl_desc)         struct_free(bl_desc)
void bl_value_free(bl_value_t *bl_value);

// List destructors
void list_free(GSList *list);
#define bl_primary_list_free(list)    list_free(list)
#define bl_included_list_free(list)   list_free(list)
#define bl_char_list_free(list)       list_free(list)
#define bl_desc_list_free(list)       list_free(list)
void bl_value_list_free(GSList *list);

// Print Struct
#define bl_primary_print(bl_primary)  bl_primary_fprint(NULL, bl_primary)
#define bl_include_print(bl_included) bl_included_fprint(NULL, bl_included)
#define bl_char_print(bl_char)        bl_char_fprint(NULL, bl_char)
#define bl_desc_print(bl_desc)        bl_desc_fprint(NULL, bl_desc)
#define bl_value_print(bl_value)      bl_value_fprint(NULL, bl_value)

// Print Struct in file
void bl_primary_fprint( FILE *f, bl_primary_t  *bl_primary);
void bl_included_fprint(FILE *f, bl_included_t *bl_included);
void bl_char_fprint(    FILE *f, bl_char_t     *bl_char);
void bl_desc_fprint(    FILE *f, bl_desc_t     *bl_desc);
void bl_value_fprint(   FILE *f, bl_value_t    *bl_value);

// Print list
#define bl_primary_list_print(list)   list_fprint(NULL, list, 0)
#define bl_included_list_print(list)  list_fprint(NULL, list, 1)
#define bl_char_list_print(list)      list_fprint(NULL, list, 2)
#define bl_desc_list_print(list)      list_fprint(NULL, list, 3)
#define bl_value_list_print(list)     list_fprint(NULL, list, 4)
#define handle_list_print(list)       list_fprint(NULL, list, 5)

// Print list in file
#define bl_primary_list_fprint(f, list)  list_fprint(f, list, 0)
#define bl_included_list_fprint(f, list) list_fprint(f, list, 1)
#define bl_char_list_fprint(f, list)     list_fprint(f, list, 2)
#define bl_desc_list_fprint(f, list)     list_fprint(f, list, 3)
#define bl_value_list_fprint(f, list)    list_fprint(f, list, 4)
#define handle_list_fprint(f, list)      list_fprint(f, list, 5)
void list_fprint(FILE *f, GSList *list, int type);
#endif
