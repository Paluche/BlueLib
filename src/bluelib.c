/*
 *  BlueLib - Abstraction layer for Bluetooth Low Energy softwares
 *
 *  Copyright (C) 2011  Nokia Corporation
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include <glib.h>
#include <errno.h>
#include <malloc.h>

#include "bluelib.h"
#include "callback.h"
#include "conn_state.h"

#include "btio.h"
#include "att.h"
#include "gattrib.h"
#include "gatt.h"
#include "utils.h"

#define printf(...) printf("[BL] " __VA_ARGS__)

/********************************* Helpers *********************************/
static void disconnect_io(dev_ctx_t *dev_ctx)
{
    if (STATE_DISCONNECTED == get_conn_state(dev_ctx))
        return;

    g_attrib_unref(dev_ctx->attrib);
    dev_ctx->attrib = NULL;
    dev_ctx->opt_mtu = 0;

    g_io_channel_shutdown(dev_ctx->iochannel, FALSE, NULL /*usually *GError*/);
    g_io_channel_unref(dev_ctx->iochannel);
    dev_ctx->iochannel = NULL;

    set_conn_state(dev_ctx, STATE_DISCONNECTED);
}

gboolean channel_watcher(GIOChannel *chan, GIOCondition cond,
                         gpointer user_data)
{
    disconnect_io(user_data);
    printf("Connection lost\n");
    return FALSE;
}

GQuark bl_error_domain(void)
{
    return g_quark_from_static_string("Bluelib error domain");
}
#define PROPAGATE_ERROR                                                     \
    do {                                                                    \
        CLEAR_GERROR                                                        \
        g_propagate_error(gerr, err);                                       \
    } while (0)

#define CLEAR_GERROR                                                        \
    if (*gerr) {                                                            \
        g_error_free(*gerr);                                                \
        *gerr = NULL;                                                       \
    }

/******************************** Asserts **********************************/
static inline int handle_assert(cb_ctx_t *cb_ctx, uint16_t *start_handle,
                                uint16_t *end_handle,
                                bl_primary_t *bl_primary, GError **gerr)
{
    // Default range
    *start_handle         = 0x0001;
    *end_handle           = 0xffff;
    cb_ctx->end_handle_cb = 0xffff;

    if (bl_primary != NULL) {
        *start_handle         = bl_primary->start_handle;
        *end_handle           = bl_primary->end_handle;
        cb_ctx->end_handle_cb = bl_primary->end_handle;
    }

    if (start_handle > end_handle) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_HANDLE_ORDER_ERROR,
                                  "Error end_handle before start_handle\n");
        PROPAGATE_ERROR;
        return BL_HANDLE_ORDER_ERROR;
    }

    if ((start_handle == INVALID_HANDLE) || (end_handle == INVALID_HANDLE)) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, EINVAL,
                                  "Invalid handle\n");
        PROPAGATE_ERROR;
        return EINVAL;
    }

    return BL_NO_ERROR;
}
#define BLUELIB_ENTER                                                       \
  if (dev_ctx == NULL)                                                      \
    return BL_NO_CTX_ERROR;                                                 \
                                                                            \
  if (!is_event_loop_running())                                             \
    return BL_NOT_INIT_ERROR

#define BLUELIB_ENTER_GERR                                                  \
  if (dev_ctx == NULL){                                                     \
    GError *err = g_error_new(BL_ERROR_DOMAIN, BL_NO_CTX_ERROR,             \
                              "No context given\n");                        \
    PROPAGATE_ERROR;                                                        \
    return NULL;                                                            \
  }

#define ASSERT_CONNECTED                                                    \
    if (get_conn_state(dev_ctx) != STATE_CONNECTED) {                       \
        printf("Error: Not connected\n");                                   \
        ret = BL_DISCONNECTED_ERROR;                                        \
        goto exit;                                                          \
    }                                                                       \
if (!is_event_loop_running()) {                                             \
    printf("Error: Not connected\n");                                       \
    ret = BL_DISCONNECTED_ERROR;                                            \
    goto exit;                                                              \
}

#define ASSERT_CONNECTED_GERR                                               \
    if (get_conn_state(dev_ctx) != STATE_CONNECTED) {                       \
        GError *err = g_error_new(BL_ERROR_DOMAIN,                          \
                                  BL_DISCONNECTED_ERROR,                    \
                                  "Not connected\n");                       \
        PROPAGATE_ERROR;                                                    \
        goto exit;                                                          \
    }                                                                       \
if (!is_event_loop_running()) {                                             \
    GError *err = g_error_new(BL_ERROR_DOMAIN,                              \
                              BL_DISCONNECTED_ERROR,                        \
                              "Event loop not running\n");                  \
    PROPAGATE_ERROR;                                                        \
    goto exit;                                                              \
}


/***************************** Global functions ****************************/

/************************* Initialisation functions ************************/
int bl_init(GError **gerr)
{
    return start_event_loop(gerr);
}

int dev_init(dev_ctx_t *dev_ctx, const char *src, const char *dst,
              const char *dst_type, int psm, const int sec_level)
{
    BLUELIB_ENTER;
    dev_ctx->opt_src      = g_strdup(src);
    dev_ctx->opt_dst      = g_strdup(dst);
    dev_ctx->opt_dst_type = g_strdup(dst_type);
    dev_ctx->opt_psm      = psm;
    dev_ctx->current_mac  = NULL;

    if (sec_level == SECURITY_LEVEL_HIGH)
        dev_ctx->opt_sec_level = g_strdup("high");

    else if (sec_level == SECURITY_LEVEL_MEDIUM)
        dev_ctx->opt_sec_level = g_strdup("medium");

    else
        dev_ctx->opt_sec_level = g_strdup("low");

    return BL_NO_ERROR;
}



/******************** Connect/Disconnect from a device *********************/
// Connect to a device
int bl_connect(dev_ctx_t *dev_ctx, char *mac_dst, char *dst_type)
{
    GError  *gerr = NULL;
    int      ret;
    cb_ctx_t cb_ctx;

    BLUELIB_ENTER;

    init_cb_ctx(&cb_ctx, dev_ctx);

    if (get_conn_state(dev_ctx) != STATE_DISCONNECTED) {
        printf("Error: Already connected to a device\n");
        ret = BL_ALREADY_CONNECTED_ERROR;
        goto error;
    }

    if (!mac_dst) {
        printf("Error: Remote Bluetooth address required\n");
        ret = EINVAL;
        goto error;
    }

    // Check if the address MAC is correct
    for (int i = 0; i < MAC_SZ; i++) {
        if ((i%3 < 2) &&
            (!(((mac_dst[i] >= '0') && (mac_dst[i] <= '9')) ||
               ((mac_dst[i] >= 'a') && (mac_dst[i] <= 'f')) ||
               ((mac_dst[i] >= 'A') && (mac_dst[i] <= 'F')))))
            goto wrongmac;
        else if ((i%3 == 2) && (mac_dst[i] != ':'))
            goto wrongmac;
    }
    if (mac_dst[MAC_SZ] != '\0')
        goto wrongmac;

    g_free(dev_ctx->opt_dst);
    dev_ctx->opt_dst = g_strdup(mac_dst);

    g_free(dev_ctx->opt_dst_type);
    if (dst_type)
        dev_ctx->opt_dst_type = g_strdup(dst_type);
    else
        dev_ctx->opt_dst_type = g_strdup("public");

    printf("Attempting to connect to %s\n", dev_ctx->opt_dst);
    set_conn_state(dev_ctx, STATE_CONNECTING);
    dev_ctx->iochannel = gatt_connect(dev_ctx->opt_src, dev_ctx->opt_dst,
                                     dev_ctx->opt_dst_type,
                                     dev_ctx->opt_sec_level,
                                     dev_ctx->opt_psm, dev_ctx->opt_mtu,
                                     connect_cb, &gerr);

    if (gerr) {
        printf("Error <%d %s>\n", gerr->code, gerr->message);
        set_conn_state(dev_ctx, STATE_DISCONNECTED);
        ret = gerr->code;
        g_error_free(gerr);
        goto error;
    }

    if (!dev_ctx->iochannel) {
        printf("Error: iochannel NULL\n");
        set_conn_state(dev_ctx, STATE_DISCONNECTED);
        ret = BL_SEND_REQUEST_ERROR;
        goto error;
    }

    g_io_add_watch(dev_ctx->iochannel, G_IO_HUP, channel_watcher, NULL);

    ret = wait_for_cb(&cb_ctx, NULL, NULL);
    if (ret) {
        printf("Error: CallBack error\n");
        set_conn_state(dev_ctx, STATE_DISCONNECTED);
        stop_event_loop();
        goto error;
    }

    dev_ctx->current_mac = mac_dst;
    ret = BL_NO_ERROR;
    if (dev_ctx->connect_cb_fct)
        return dev_ctx->connect_cb_fct();
    return ret;

wrongmac:
    printf("Error: Address MAC invalid\n");
    ret = EINVAL;
error:
    return ret;;
}

// Disconnect from the device, delete the nofication list.
int bl_disconnect(dev_ctx_t *dev_ctx)
{
    int ret = BL_NO_ERROR;
    cb_ctx_t cb_ctx;

    BLUELIB_ENTER;

    init_cb_ctx(&cb_ctx, dev_ctx);

    if (get_conn_state(dev_ctx) != STATE_DISCONNECTED)
        disconnect_io(dev_ctx);
    printf("Disconnected\n");
    if (is_event_loop_running())
        stop_event_loop();
    return ret;;
}

// Set a function to call each time you succeed to connect
// If the connection go well, the return value of bl_connect will be the one
// of user_cb_fct_t.
int bl_set_connect_cb(dev_ctx_t *dev_ctx, user_cb_fct_t func)
{
    dev_ctx->connect_cb_fct = func;
    return BL_NO_ERROR;
}


/************************* Primary Service Discovery ***********************/
// Get all the primary service associated of an UUID.
// Return a list of primary services (bl_primary_t *).
GSList *bl_get_all_primary(dev_ctx_t *dev_ctx, char *uuid_str, GError **gerr)
{
    GSList *ret = NULL;
    cb_ctx_t cb_ctx;

    CLEAR_GERROR;
    BLUELIB_ENTER_GERR;
    ASSERT_CONNECTED_GERR;

    init_cb_ctx(&cb_ctx, dev_ctx);

    if (uuid_str) {
        bt_uuid_t uuid;
        bt_string_to_uuid(&uuid, uuid_str);
        if (!gatt_discover_primary(dev_ctx->attrib, &uuid, primary_by_uuid_cb,
                                   NULL)) {
            GError *err = g_error_new(BL_ERROR_DOMAIN, BL_SEND_REQUEST_ERROR,
                                      "Unable to send request\n");
            PROPAGATE_ERROR;
            goto exit;
        }
    } else if (!gatt_discover_primary(dev_ctx->attrib, NULL, primary_all_cb,
                                      NULL)) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_SEND_REQUEST_ERROR,
                                  "Unable to send request\n");
        PROPAGATE_ERROR;
        goto exit;
    }

    if (wait_for_cb(&cb_ctx, (void **) &ret, gerr))
        goto exit;
    if ((ret != NULL) && (uuid_str)) {
        // Add uuid to each bl_primary of the list
        for (GSList *l = ret; l; l = l->next)
            strcpy(((bl_primary_t *)(l->data))->uuid_str, uuid_str);
    }
exit:
    return ret;;
}

// Get a specific primary service.
// Return the primary service associated to this UUID, if unique.
bl_primary_t *bl_get_primary(dev_ctx_t *dev_ctx, char *uuid_str, GError **gerr)
{
    CLEAR_GERROR;
    bl_primary_t *bl_primary      = NULL;
    GSList       *bl_primary_list = bl_get_all_primary(dev_ctx, uuid_str,
                                                       gerr);

    if (*gerr || !bl_primary_list)
        return NULL;

    if (bl_primary_list->next) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_UNICITY_ERROR,
                                  "Primary not unique\n");
        PROPAGATE_ERROR;
        bl_primary_list_free(bl_primary_list);
        return NULL;
    }

    if (bl_primary_list->data)
        bl_primary = bl_primary_cpy(bl_primary_list->data);
    bl_primary_list_free(bl_primary_list);
    return bl_primary;
}

// Get all the primary services of a device.
// Return a list of primary services (bl_primary_t *).
GSList *bl_get_all_primary_device(dev_ctx_t *dev_ctx, GError **gerr)
{
    return bl_get_all_primary(dev_ctx, NULL, gerr);
}


/************************** Get Included Services **************************/
// Get all the included service of a primary service.
// Returns a list of included services (bl_included_t *).
GSList *bl_get_included(dev_ctx_t *dev_ctx, bl_primary_t *bl_primary,
                        GError **gerr)
{
    GSList *ret = NULL;
    cb_ctx_t cb_ctx;

    CLEAR_GERROR;
    BLUELIB_ENTER_GERR;
    ASSERT_CONNECTED_GERR;

    init_cb_ctx(&cb_ctx, dev_ctx);

    // Initialisation to default range.
    uint16_t start_handle = 0x0001;
    uint16_t end_handle   = 0xffff;

    if (handle_assert(&cb_ctx, &start_handle, &end_handle, bl_primary, gerr))
        goto exit;

    if (!gatt_find_included(dev_ctx->attrib, start_handle,
                                              end_handle, included_cb, NULL))
    {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_SEND_REQUEST_ERROR,
                                  "Unable to send request\n");
        PROPAGATE_ERROR;
        goto exit;
    }

    wait_for_cb(&cb_ctx, (void **) &ret, gerr);
exit:
    return ret;;
}


/*************************** Get characteristics ***************************/
// Get all characteristics associated to an UUID on a primary service.
// Returns a list of characteristics (bl_char_t *) associated to the UUID
GSList *bl_get_all_char(dev_ctx_t *dev_ctx, char *uuid_str,
                        bl_primary_t *bl_primary, GError **gerr)
{
    GSList *ret = NULL;
    cb_ctx_t cb_ctx;

    CLEAR_GERROR;
    BLUELIB_ENTER_GERR;
    ASSERT_CONNECTED_GERR;

    init_cb_ctx(&cb_ctx, dev_ctx);

    // Intialisation to default range.
    uint16_t start_handle;
    uint16_t end_handle;

    if (handle_assert(&cb_ctx, &start_handle, &end_handle, bl_primary, gerr))
        goto exit;

    bt_uuid_t *puuid = NULL;

    if (uuid_str) {
        bt_uuid_t uuid;
        bt_string_to_uuid(&uuid, uuid_str);
        puuid = &uuid;
    }

    if (!gatt_discover_char(dev_ctx->attrib, start_handle, end_handle, puuid,
                            char_by_uuid_cb, NULL)) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_SEND_REQUEST_ERROR,
                                  "Unable to send request\n");
        PROPAGATE_ERROR;
        goto exit;
    }

    wait_for_cb(&cb_ctx, (void **) &ret, gerr);
exit:
    return ret;;
}

// Get a specific characteristic associated to an UUID on a primary service.
// Returns the characteristic associated to this uuid, if unique.
bl_char_t *bl_get_char(dev_ctx_t *dev_ctx, char *uuid_str,
                       bl_primary_t *bl_primary, GError **gerr)
{
    CLEAR_GERROR;
    bl_char_t *bl_char      = NULL;
    GSList    *bl_char_list = bl_get_all_char(dev_ctx, uuid_str, bl_primary,
                                              gerr);

    if ((!bl_char_list) || (*gerr)) {
        return NULL;
    }

    if (bl_char_list->next != NULL) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_UNICITY_ERROR,
                                  "Characteristic not unique\n");
        PROPAGATE_ERROR;
        bl_char_list_free(bl_char_list);
        return NULL;
    }
    if (bl_char_list->data);
    bl_char = bl_char_cpy(bl_char_list->data);
    bl_char_list_free(bl_char_list);
    return bl_char;
}

// Get all characteristics on a primary service.
// Returns a list of characteristics (bl_char_t *).
GSList *bl_get_all_char_in_primary(dev_ctx_t *dev_ctx, bl_primary_t *bl_primary,
                                   GError **gerr)
{
    CLEAR_GERROR;
    return bl_get_all_char(dev_ctx, NULL, bl_primary, gerr);
}


/****************************** Get Descriptors ****************************/
// Get all the descriptors of a specified characteristic on a primary
// service.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
// Returns a list of characteristic descriptor (bl_desc_t *).
GSList *bl_get_all_desc_by_char(dev_ctx_t *dev_ctx, bl_char_t *start_bl_char,
                                bl_char_t *end_bl_char,
                                bl_primary_t *bl_primary, GError **gerr)
{
    GSList   *ret = NULL;
    uint16_t  start_handle;
    uint16_t  end_handle;
    cb_ctx_t cb_ctx;

    CLEAR_GERROR;
    BLUELIB_ENTER_GERR;
    ASSERT_CONNECTED_GERR;

    init_cb_ctx(&cb_ctx, dev_ctx);

    if (start_bl_char) {
        start_handle = start_bl_char->handle + 1;
    } else {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_MISSING_ARGUMENT_ERROR,
                                  "Start characteristic needed\n");
        PROPAGATE_ERROR;
        goto exit;
    }

    if (end_bl_char) {
        end_handle = end_bl_char->handle - 1;
    } else
        end_handle = 0xffff;

    if (bl_primary) {
        if (end_handle > bl_primary->end_handle)
            end_handle = bl_primary->end_handle;
    }

    if (start_handle > end_handle) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_HANDLE_ORDER_ERROR,
                                  "The handle of end_bl_char before the one "
                                  "of start_bl_char\n");
        PROPAGATE_ERROR;
        goto exit;
    }
    if (!gatt_discover_char_desc(dev_ctx->attrib, start_handle, end_handle,
                                 char_desc_cb, NULL)) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_SEND_REQUEST_ERROR,
                                  "Unable to send request\n");
        PROPAGATE_ERROR;
        goto exit;
    }
    wait_for_cb(&cb_ctx, (void **) &ret, gerr);
exit:
    return ret;;
}

// Get all the descriptors of the unique characteristic associated to the
// UUID on a primary service.
// Returns a list of characteristic descriptor (bl_desc_t *).
GSList *bl_get_all_desc(dev_ctx_t *dev_ctx, char *uuid_str,
                        bl_primary_t *bl_primary, GError **gerr)
{
    bl_char_t *bl_char = bl_get_char(dev_ctx, uuid_str, bl_primary, gerr);

    if ((!bl_char) || (*gerr))
        return NULL;

    GSList *ret = bl_get_all_desc_by_char(dev_ctx, bl_char, NULL, bl_primary,
                                          gerr);
    bl_char_free(bl_char);
    return ret;
}

// BlueZ returns UUID with letter in lowcase.
static void lowcase_str(char *dst, char *src)
{
    for (int i = 0; ;i++) {

        if (('A' <= src[i]) && (src[i] <= 'F'))
            dst[i] = src[i] + 0x20;
        else
            dst[i] = src[i];
        if (!src[i])
            break;
    }
}

// Find a specific descriptor by UUID, on a list of bl_desc_t *.
static bl_desc_t *find_desc(GSList *bl_desc_list, char *desc_uuid_str)
{
    // The uuid generated by the callback are in lowcase
    // Translate upcase letters in lowcase
    bl_desc_t *bl_desc = NULL;

    char desc_uuid_tmp[MAX_LEN_UUID_STR];
    lowcase_str(desc_uuid_tmp, desc_uuid_str);

    for (GSList *l = bl_desc_list; l; l = l->next) {
        if (l->data) {

            if (!strcmp(((bl_desc_t *)(l->data))->uuid_str, desc_uuid_str))
                bl_desc = bl_desc_cpy(l->data);
        } else {
            printf("Error: NO DATA\n");
        }
    }
    bl_char_list_free(bl_desc_list);
    return bl_desc;
}

// Search a specific descriptor of the specified characteristic.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
// Returns the characteristic descriptor if found, else NULL.
bl_desc_t *bl_get_desc_by_char(dev_ctx_t *dev_ctx, bl_char_t *start_bl_char,
                               bl_char_t *end_bl_char,
                               bl_primary_t *bl_primary, char *desc_uuid_str,
                               GError **gerr)
{
    *gerr = NULL; //FIXME CLEAR GERR ?
    GSList *bl_desc_list = bl_get_all_desc_by_char(dev_ctx, start_bl_char,
                                                   end_bl_char, bl_primary,
                                                   gerr);

    if ((!bl_desc_list) || (*gerr))
        return NULL;

    return find_desc(bl_desc_list, desc_uuid_str);
}

// Search a specific descriptor of the unique characteristic associated to
// the UUID on a primary service.
// Returns the characteristic descriptor if found, else NULL.
bl_desc_t *bl_get_desc(dev_ctx_t *dev_ctx, char *char_uuid_str,
                       bl_primary_t *bl_primary, char *desc_uuid_str,
                       GError **gerr)
{
    *gerr = NULL;
    GSList *bl_desc_list = bl_get_all_desc(dev_ctx, char_uuid_str, bl_primary,
                                           gerr);

    if ((!bl_desc_list) || *gerr)
        return NULL;

    return find_desc(bl_desc_list, desc_uuid_str);
}


/************************* Read characteristic value ***********************/
// Read by handle.
static bl_value_t *read_by_hnd(dev_ctx_t *dev_ctx, uint16_t handle,
                               GError **gerr)
{
    bl_value_t *ret = NULL;
    *gerr = NULL;
    cb_ctx_t cb_ctx;

    BLUELIB_ENTER_GERR;
    ASSERT_CONNECTED_GERR;

    init_cb_ctx(&cb_ctx, dev_ctx);

    if (handle == INVALID_HANDLE) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, EINVAL,
                                  "Invalid handle\n");
        PROPAGATE_ERROR;
        goto exit;
    }

    if (!gatt_read_char(dev_ctx->attrib, handle, read_by_hnd_cb,
                        dev_ctx->attrib)) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_SEND_REQUEST_ERROR,
                                  "Unable to send request\n");
        PROPAGATE_ERROR;
        goto exit;
    }

    wait_for_cb(&cb_ctx, (void **) &ret, gerr);

    // Add handle to the value
    if (ret)
        ret->handle = handle;
exit:
    return ret;;
}

// Read all the characteristics value associated to this UUID.
// Return a list of values (bl_value_t *).
GSList *bl_read_char_all(dev_ctx_t *dev_ctx, char *uuid_str,
                         bl_primary_t *bl_primary, GError **gerr)
{
    GSList *ret = NULL;
    cb_ctx_t cb_ctx;

    BLUELIB_ENTER_GERR;
    ASSERT_CONNECTED_GERR;

    init_cb_ctx(&cb_ctx, dev_ctx);

    if (uuid_str == NULL) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_SEND_REQUEST_ERROR,
                                  "UUID needed\n");
        PROPAGATE_ERROR;
        goto exit;
    }

    // Initialisation to default range.
    uint16_t start_handle;
    uint16_t end_handle;

    if (handle_assert(&cb_ctx, &start_handle, &end_handle, bl_primary, gerr))
        goto exit;

    bt_uuid_t uuid;
    bt_string_to_uuid(&uuid, uuid_str);

    if (!gatt_read_char_by_uuid(dev_ctx->attrib, start_handle, end_handle,
                                &uuid,
                                read_by_uuid_cb, NULL)) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_SEND_REQUEST_ERROR,
                                  "Unable to send request\n");
        PROPAGATE_ERROR;
        goto exit;
    }

    wait_for_cb(&cb_ctx, (void **) &ret, gerr);

    if (ret) {
        // Add the value of the UUID to each of the values
        for (GSList *l = ret; l; l = l->next) {
            strcpy(((bl_value_t *)(l->data))->uuid_str, uuid_str);
        }
    }
exit:
    return ret;;
}

// Read a characteristic value by UUID on a primary service.
bl_value_t *bl_read_char(dev_ctx_t *dev_ctx, char *uuid_str,
                         bl_primary_t *bl_primary, GError **gerr)
{
    CLEAR_GERROR;
    bl_value_t *ret           = NULL;
    GSList     *bl_value_list = bl_read_char_all(dev_ctx, uuid_str, bl_primary,
                                                 gerr);

    if (*gerr || (!bl_value_list) || (!bl_value_list->data))
        return NULL;

    if (bl_value_list->next) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_UNICITY_ERROR,
                                  "Characteristic not unique\n");
        PROPAGATE_ERROR;
        bl_value_list_free(bl_value_list);
        return NULL;
    }

    if (bl_value_list->data)
        ret = bl_value_cpy(bl_value_list->data);
    bl_value_list_free(bl_value_list);
    return ret;
}

// Equivalent to bl_read_char but supplying the blob reading.
bl_value_t *bl_read_char_blob(dev_ctx_t *dev_ctx, char *uuid_str,
                              bl_primary_t *bl_primary,
                              GError **gerr)
{
    bl_char_t *bl_char = bl_get_char(dev_ctx, uuid_str, bl_primary, gerr);

    if (*gerr || !bl_char)
        return NULL;

    bl_value_t *ret = bl_read_char_by_char(dev_ctx, bl_char, gerr);
    bl_char_free(bl_char);
    return ret;
}

// Equivalent to bl_read_char_all but supplying the blob reading.
// Return a list of values (bl_value_t *).
GSList *bl_read_char_all_blob(dev_ctx_t *dev_ctx, char *uuid_str,
                              bl_primary_t *bl_primary,
                              GError **gerr)
{
    GSList *list = bl_get_all_char(dev_ctx, uuid_str, bl_primary, gerr);

    if (*gerr || !list)
        return NULL;

    for (GSList *l = list; l && l->data; l = l->next) {
        bl_value_t *bl_value = bl_read_char_by_char(dev_ctx, l->data, gerr);
        bl_char_free(l->data);
        l->data = bl_value;
    }
    return list;
}

// Read a characteristic value of a characteristic.
bl_value_t *bl_read_char_by_char(dev_ctx_t *dev_ctx, bl_char_t *bl_char,
                                 GError **gerr)
{
    bl_value_t *bl_value = read_by_hnd(dev_ctx, bl_char->value_handle, gerr);

    if (!bl_value)
        return NULL;

    // Add UUID to value
    strcpy(bl_value->uuid_str, bl_char->uuid_str);

    return bl_value;
}

/******************************* Read descriptor ***************************/
// Read a descriptor of a characteristic by UUID on a primary service.
bl_value_t *bl_read_desc(dev_ctx_t *dev_ctx, char *char_uuid_str,
                         bl_primary_t *bl_primary,
                         char *desc_uuid_str, GError **gerr)
{
    bl_desc_t *bl_desc = bl_get_desc(dev_ctx, char_uuid_str, bl_primary,
                                     desc_uuid_str, gerr);

    if (*gerr || !bl_desc)
        return NULL;

    bl_value_t *ret = read_by_hnd(dev_ctx, bl_desc->handle, gerr);
    bl_desc_free(bl_desc);
    return ret;
}

// Read all the descriptors of a characteristic by UUID on a primary service.
// Return a list of values (bl_value_t *).
GSList *bl_read_all_desc(dev_ctx_t *dev_ctx, char *char_uuid_str,
                         bl_primary_t *bl_primary, GError **gerr)
{
    GSList *list = bl_get_all_desc(dev_ctx, char_uuid_str, bl_primary, gerr);

    if (*gerr || !list)
        return NULL;

    for (GSList *l = list; l && l->data; l = l->next) {
        bl_value_t *bl_value = bl_read_desc_by_desc(dev_ctx, l->data, gerr);
        bl_desc_free(l->data);
        l->data = bl_value;
    }
    return list;
}

// Read descriptor by descriptor.
bl_value_t *bl_read_desc_by_desc(dev_ctx_t *dev_ctx, bl_desc_t *bl_desc,
                                 GError **gerr)
{
    return read_by_hnd(dev_ctx, bl_desc->handle, gerr);
}

// Read descriptor by characteristic.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
bl_value_t *bl_read_desc_by_char(dev_ctx_t *dev_ctx, bl_char_t *start_bl_char,
                                 bl_char_t *end_bl_char,
                                 bl_primary_t *bl_primary,
                                 char *desc_uuid_str, GError **gerr)
{
    bl_desc_t *bl_desc = bl_get_desc_by_char(dev_ctx, start_bl_char,
                                             end_bl_char, bl_primary,
                                             desc_uuid_str, gerr);

    if (*gerr || !bl_desc)
        return NULL;

    bl_value_t *ret = bl_read_desc_by_desc(dev_ctx, bl_desc, gerr);
    bl_desc_free(bl_desc);
    return ret;
}


/************************ Write characteristic value ***********************/
// Write a characteristic by handle.
static int write_by_hnd(dev_ctx_t *dev_ctx, uint16_t handle, uint8_t *value,
                        size_t size, int type)
{
    int ret;
    cb_ctx_t cb_ctx;

    BLUELIB_ENTER;
    ASSERT_CONNECTED;

    init_cb_ctx(&cb_ctx, dev_ctx);

    if (handle == INVALID_HANDLE) {
        printf("Error: Invalid handle\n");
        ret = EINVAL;
        goto exit;
    }

    if ((size == 0) || (value == NULL)) {
        printf("Error: Invalid value\n");
        ret = EINVAL;
        goto exit;
    }

    if (type) {
        if (!gatt_write_char(dev_ctx->attrib, handle, value, size,
                             write_req_cb, NULL)) {
            printf("Error: Unable to send request\n");
            ret = BL_SEND_REQUEST_ERROR;
            goto exit;
        }
        ret = wait_for_cb(&cb_ctx, NULL, NULL);
    } else {
        if (!gatt_write_cmd(dev_ctx->attrib, handle, value, size, NULL,
                            NULL)) {
            printf("Error: Unable to send write cmd\n");
            ret = BL_SEND_REQUEST_ERROR;
            goto exit;
        }
        ret = BL_NO_ERROR;
    }

exit:
    return ret;;
}

// Write a characteristic value by UUID on a primary service
int bl_write_char(dev_ctx_t *dev_ctx, char *uuid_str, bl_primary_t *bl_primary,
                  uint8_t *value, size_t size, int type)
{
    int ret;
    GError *gerr = NULL;
    bl_char_t *bl_char= bl_get_char(dev_ctx, uuid_str, bl_primary,
                                    &gerr);

    if (gerr) {
        printf("Error: %s\n", gerr->message);
        ret = gerr->code;
        g_error_free(gerr);
        goto exit;
    }

    if (!bl_char) {
        printf("Error: No characteristic found\n");
        return EINVAL;
    }

    ret = write_by_hnd(dev_ctx, bl_char->value_handle, value, size, type);
exit:
    if (bl_char)
        bl_char_free(bl_char);
    return ret;
}

// Write a characteristic value by characteristic.
int bl_write_char_by_char(dev_ctx_t *dev_ctx, bl_char_t *bl_char,
                          uint8_t *value, size_t size, int type)
{
    return write_by_hnd(dev_ctx, bl_char->value_handle, value, size, type);
}


/**************************** Write descriptor *****************************/
// Write a descriptor of a characteristic by UUID on a primary service.
int bl_write_desc(dev_ctx_t *dev_ctx, char *char_uuid_str,
                  bl_primary_t *bl_primary, char *desc_uuid_str,
                  uint8_t *value, size_t size)
{
    int ret;
    GError *gerr = NULL;
    bl_desc_t *bl_desc = bl_get_desc(dev_ctx, char_uuid_str, bl_primary,
                                     desc_uuid_str, &gerr);

    if (gerr) {
        printf("Error: %s\n", gerr->message);
        ret = gerr->code;
        g_error_free(gerr);
        goto exit;
    }

    if (!bl_desc) {
        printf("Error: No descriptor found\n");
        return EINVAL;
    }

    ret = write_by_hnd(dev_ctx, bl_desc->handle, value, size, WRITE_REQ);
exit:
    if (bl_desc)
        bl_desc_free(bl_desc);
    return ret;
}

// Write descriptor by bl_desc_t.
int bl_write_desc_by_desc(dev_ctx_t *dev_ctx, bl_desc_t *bl_desc,
                          uint8_t *value, size_t size)
{
    return write_by_hnd(dev_ctx, bl_desc->handle, value, size, WRITE_REQ);
}

// Write a descriptor on a characteristic.
// Setting end_bl_char avoid uneeded packet by specifying the end of the zone
// to search, but the result is the same with or without.
int bl_write_desc_by_char(dev_ctx_t *dev_ctx, bl_char_t *start_bl_char,
                          bl_char_t *end_bl_char, bl_primary_t *bl_primary,
                          char *desc_uuid_str, uint8_t *value, size_t size)
{
    GError *gerr;
    bl_desc_t *bl_desc = bl_get_desc_by_char(dev_ctx, start_bl_char,
                                             end_bl_char, bl_primary,
                                             desc_uuid_str, &gerr);
    if (gerr || !bl_desc)
        return EINVAL;
    int ret = bl_write_desc_by_desc(dev_ctx, bl_desc, value, size);
    bl_desc_free(bl_desc);
    return ret;
}


/*************************** Set security level ****************************/
// Default: low
int bl_change_sec_level(dev_ctx_t *dev_ctx, int level)
{
    GError *gerr = NULL;
    BtIOSecLevel sec_level;
    int ret;

    BLUELIB_ENTER;
    ASSERT_CONNECTED;

    if (dev_ctx->opt_sec_level)
        g_free(dev_ctx->opt_sec_level);
    if (level == SECURITY_LEVEL_HIGH) {
        sec_level = BT_IO_SEC_HIGH;
        dev_ctx->opt_sec_level = g_strdup("high");
    }  else if (level == SECURITY_LEVEL_MEDIUM) {
        sec_level = BT_IO_SEC_MEDIUM;
        dev_ctx->opt_sec_level = g_strdup("medium");
    }  else {
        sec_level = BT_IO_SEC_LOW;
        dev_ctx->opt_sec_level = g_strdup("low");
    }

    if (dev_ctx->opt_psm) {
        printf("Change will take effect on reconnection\n");
        ret = BL_RECONNECTION_NEEDED_ERROR;
        goto exit;
    }

    bt_io_set(dev_ctx->iochannel, &gerr, BT_IO_OPT_SEC_LEVEL, sec_level,
              BT_IO_OPT_INVALID);

    if (gerr) {
        printf("Error: %s\n", gerr->message);
        g_error_free(gerr);
        return gerr->code;
    }
    ret = BL_NO_ERROR;
exit:
    return ret;
}


/************************* Change MTU for GATT/ATT *************************/
int bl_change_mtu(dev_ctx_t *dev_ctx, int value)
{
    int ret;
    cb_ctx_t cb_ctx;

    BLUELIB_ENTER;
    ASSERT_CONNECTED;

    init_cb_ctx(&cb_ctx, dev_ctx);

    if (dev_ctx->opt_psm) {
        printf("Error: Operation is only available for LE transport.\n");
        ret = BL_LE_ONLY_ERROR;
        goto exit;
    }

    if (dev_ctx->opt_mtu) {
        printf("Error: MTU exchange can only occur once per connection.\n");
        ret = BL_MTU_ALREADY_EXCHANGED_ERROR;
        goto exit;
    }

    errno = 0;
    dev_ctx->opt_mtu = value;
    if (errno != 0 || dev_ctx->opt_mtu < ATT_DEFAULT_LE_MTU) {
        printf("Error: Invalid value. Minimum MTU size is %d\n",
               ATT_DEFAULT_LE_MTU);
        ret = EINVAL;
        goto exit;
    }
    gatt_exchange_mtu(dev_ctx->attrib, dev_ctx->opt_mtu, exchange_mtu_cb,
                      NULL);

    ret = wait_for_cb(&cb_ctx, NULL, NULL);
exit:
    return ret;;
}
