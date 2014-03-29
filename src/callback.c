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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <glib.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>

#include "uuid.h"
#include "gattrib.h"
#include "att.h"
#include "gatt.h"

#include "bluelib.h"
#include "bluelib_gatt.h"
#include "conn_state.h"
#include "callback.h"
#include "gatt_def.h"

//  Callback global Context
static GMainLoop  *event_loop    = NULL;
static GThread    *event_thread  = NULL;
static GMutex     *cb_mutex = NULL;

// The callback and the functions are running in two seperate thread, we need
// to use transport variables to return the results.


#define CB_TIMEOUT_S 120 /* For every function that have a callback function.
                          * We will wait 2 minutes before returning */

//#define DEBUG_ON       // Activate the Debug print
#ifdef DEBUG_ON
#define printf_dbg(...) printf("[CB] " __VA_ARGS__)
#else
#define printf_dbg(...)
#endif

/*
 * Helpers
 */
#define PROPAGATE_ERROR               \
    do {                              \
        if (gerr && *gerr) {          \
            g_error_free(*gerr);      \
            *gerr = NULL;             \
        }                             \
        g_propagate_error(gerr, err); \
    } while (0)

/*
 * Global functions
 */
void init_cb_ctx(cb_ctx_t *cb_ctx, dev_ctx_t *dev_ctx)
{
    cb_ctx->dev_ctx = dev_ctx;
    g_mutex_init(&cb_ctx->pending_cb_mtx);
    g_mutex_lock(&cb_ctx->pending_cb_mtx);

    cb_ctx->end_handle_cb  = 0;
    cb_ctx->cb_ret_pointer = NULL;
    cb_ctx->cb_ret_val     = BL_NO_ERROR;
}


int wait_for_cb(cb_ctx_t *cb_ctx, void **ret_pointer, GError **gerr)
{
    int wait_cnt = 0;
    if (!g_mutex_trylock(&cb_ctx->pending_cb_mtx) && is_event_loop_running()) {
        // Reset return value
        cb_ctx->cb_ret_val     = BL_NO_CALLBACK_ERROR;
        cb_ctx->cb_ret_pointer = NULL;

        printf_dbg("Waiting for callback\n");
        while (is_event_loop_running() &&
               !g_mutex_trylock(&cb_ctx->pending_cb_mtx)) {
            usleep(100000);

            if (wait_cnt < CB_TIMEOUT_S*10) {
                wait_cnt++;
            } else {
                GError *err = g_error_new(BL_ERROR_DOMAIN,
                                          BL_NO_CALLBACK_ERROR,
                                          "Timeout no callback received\n");
                printf_dbg("%s", err->message);
                PROPAGATE_ERROR;
                set_conn_state(cb_ctx->dev_ctx, STATE_DISCONNECTED);
                return BL_NO_CALLBACK_ERROR;
            }
        }
    }

    if (!is_event_loop_running()) {
        set_conn_state(cb_ctx->dev_ctx, STATE_DISCONNECTED);
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_DISCONNECTED_ERROR,
                                  "Event loop is not running\n");
        printf_dbg("%s", err->message);
        PROPAGATE_ERROR;
        return BL_DISCONNECTED_ERROR;
    } else
        printf_dbg("Callback returned <%d, %p>\n", cb_ctx->cb_ret_val, cb_ctx->cb_ret_pointer);

    if (cb_ctx->cb_ret_val != BL_NO_ERROR) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, cb_ctx->cb_ret_val, "%s",
                                  cb_ctx->cb_ret_msg);
        PROPAGATE_ERROR;
    }

    if (*cb_ctx->cb_ret_msg != '\0') {
        printf_dbg("%s", cb_ctx->cb_ret_msg);
    }
    strcpy(cb_ctx->cb_ret_msg, "\0");
    if (ret_pointer)
        *ret_pointer = cb_ctx->cb_ret_pointer;
    return cb_ctx->cb_ret_val;
}


/*
 * Event loop thread
 */
static gpointer _event_thread(gpointer data)
{
    printf_dbg("Event loop START\n");
    g_mutex_lock(cb_mutex);
    event_loop = g_main_loop_new(NULL, FALSE);
    g_mutex_unlock(cb_mutex);
    g_main_loop_run(event_loop);
    g_main_loop_unref(event_loop);
    event_loop = NULL;
    printf_dbg("Event loop EXIT\n");
    g_thread_exit(0);
    return 0;
}

int start_event_loop(GError **gerr)
{
    cb_mutex = malloc(sizeof(GMutex));
    if (cb_mutex == NULL) {
        GError *err = g_error_new(BL_ERROR_DOMAIN, BL_MALLOC_ERROR,
                                  "Start event loop: Malloc error\n");
        PROPAGATE_ERROR;
        goto error1;
    }
    g_mutex_init(cb_mutex);

    event_thread = g_thread_try_new("event_loop", _event_thread, NULL, gerr);

    for (int cnt = 0;
         (!is_event_loop_running()) && (cnt < 60) && (event_thread != NULL);
         cnt++) {
        sleep(1);
        printf_dbg("wait for event loop\n");
    }

    if (event_thread == NULL) {
        printf_dbg("%s\n", (*gerr)->message);
        goto error2;
    }

    return 0;

error2:
    free(cb_mutex);
    cb_mutex = NULL;

error1:
    return -1;
}

void stop_event_loop(void)
{
    g_mutex_lock(cb_mutex);
    if (event_loop)
        g_main_loop_quit(event_loop);
    g_mutex_unlock(cb_mutex);
}

int is_event_loop_running(void)
{
    if (!cb_mutex)
        return 0;
    g_mutex_lock(cb_mutex);
    int ret = ((event_thread != NULL) && (event_loop != NULL));
    g_mutex_unlock(cb_mutex);
    return ret;
}

/*
 * Callback functions
 */
void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
    cb_ctx_t *cb_ctx = user_data;

    printf_dbg("[CB] IN connect_cb\n");
    if (err) {
        set_conn_state(cb_ctx->dev_ctx, STATE_DISCONNECTED);
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "%s", err->message);
        goto error;
    }
    cb_ctx->dev_ctx->attrib = g_attrib_new(cb_ctx->dev_ctx->iochannel);
    set_conn_state(cb_ctx->dev_ctx, STATE_CONNECTED);
    strcpy(cb_ctx->cb_ret_msg, "Connection successful\n");
    cb_ctx->cb_ret_val = BL_NO_ERROR;

error:
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT connect_cb\n");
}

void primary_all_cb(GSList *services, guint8 status,
                    gpointer user_data)
{
    cb_ctx_t *cb_ctx = user_data;

    GSList *l = NULL;
    GSList *bl_primary_list = NULL;

    printf_dbg("[CB] IN Primary_all_cb\n");
    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "Primary callback: Failure: %s\n",
                att_ecode2str(status));
        goto error;
    }

    if (services == NULL) {
        cb_ctx->cb_ret_val = BL_NO_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "Primary callback: Nothing found\n");
        goto exit;
    }

    for (l = services; l; l = l->next) {
        struct gatt_primary *prim = l->data;
        bl_primary_t *bl_primary = bl_primary_new(prim->uuid, prim->changed,
                                                  prim->range.start, prim->range.end);

        g_free(prim);

        if (bl_primary == NULL) {
            cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
            strcpy(cb_ctx->cb_ret_msg, "Primary callback: Malloc error\n");
            goto error;
        }
        if (bl_primary_list == NULL) {
            bl_primary_list = g_slist_alloc();
            if (bl_primary_list == NULL) {
                cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
                strcpy(cb_ctx->cb_ret_msg, "Primary callback: Malloc error\n");
                goto error;
            }
            bl_primary_list->data = bl_primary;
        } else {
            bl_primary_list = g_slist_append(bl_primary_list, bl_primary);
        }
    }

    cb_ctx->cb_ret_val = BL_NO_ERROR;
    cb_ctx->cb_ret_pointer = bl_primary_list;
    strcpy(cb_ctx->cb_ret_msg, "Primary callback: Sucess\n");
    goto exit;

error:
    if (bl_primary_list)
        bl_primary_list_free(bl_primary_list);
exit:
    if (l)
        g_slist_free(l);
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT primary_all_cb\n");
}

void primary_by_uuid_cb(GSList *ranges, guint8 status,
                        gpointer user_data)
{
    GSList   *l;
    GSList   *bl_primary_list = NULL;
    cb_ctx_t *cb_ctx = user_data;

    printf_dbg("[CB] IN primary_by_uuid_cb\n");
    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "Primary by UUID callback: Failure: %s\n",
                att_ecode2str(status));
        goto error;
    }
    if (ranges == NULL) {
        cb_ctx->cb_ret_val = BL_NO_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "Primary by UUID callback: Nothing found\n");
        goto exit;
    }

    for (l = ranges; l; l = l->next) {
        struct att_range *range = l->data;
        bl_primary_t *bl_primary = bl_primary_new(NULL, 0, range->start,
                                                  range->end);
        free(range);

        if (bl_primary == NULL) {
            cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
            strcpy(cb_ctx->cb_ret_msg, "Primary by UUID callback: Malloc error\n");
            goto error;
        }
        if (bl_primary_list == NULL) {
            bl_primary_list = g_slist_alloc();

            if (bl_primary_list == NULL) {
                cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
                strcpy(cb_ctx->cb_ret_msg, "Primary by UUID callback: Malloc error\n");
                goto error;
            }
            bl_primary_list->data = bl_primary;
        } else {
            bl_primary_list = g_slist_append(bl_primary_list, bl_primary);
        }
    }
    cb_ctx->cb_ret_val = BL_NO_ERROR;
    cb_ctx->cb_ret_pointer = bl_primary_list;
    goto exit;

error:
    if (bl_primary_list)
        bl_primary_list_free(bl_primary_list);
exit:
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT primary_by_uuid_cb\n");
}

void included_cb(GSList *includes, guint8 status, gpointer user_data)
{
    GSList   *l                = NULL;
    GSList   *bl_included_list = NULL;
    cb_ctx_t *cb_ctx           = user_data;

    printf_dbg("[CB] IN included_cb\n");
    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "Included callback: Failure: %s\n",
                att_ecode2str(status));
        goto error;
    }

    if (includes == NULL) {
        cb_ctx->cb_ret_val = BL_NO_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "Included callback: Nothing found\n");
        goto exit;
    }

    for (l = includes; l; l = l->next) {
        struct gatt_included *incl = l->data;
        bl_included_t *bl_included = bl_included_new(incl->uuid, incl->handle,
                                                     incl->range.start, incl->range.end);
        if (bl_included == NULL) {
            cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
            strcpy(cb_ctx->cb_ret_msg, "Included callback: Malloc error\n");
            goto error;
        }
        if (bl_included_list == NULL) {
            bl_included_list = g_slist_alloc();
            if (bl_included_list == NULL) {
                cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
                strcpy(cb_ctx->cb_ret_msg, "Included callback: Malloc error\n");
                goto error;
            }
            bl_included_list->data = bl_included;
        } else {
            bl_included_list = g_slist_append(bl_included_list, bl_included);
        }
    }

    cb_ctx->cb_ret_val     = BL_NO_ERROR;
    cb_ctx->cb_ret_pointer = bl_included_list;
    goto exit;

error:
    if (bl_included_list)
        bl_included_list_free(bl_included_list);
exit:
    if (l)
        g_slist_free(l);
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT included_cb\n");
}

void char_by_uuid_cb(GSList *characteristics, guint8 status,
                     gpointer user_data)
{
    GSList   *l            = NULL;
    GSList   *bl_char_list = NULL;
    cb_ctx_t *cb_ctx       = user_data;

    printf_dbg("[CB] IN char_by_uuid\n");
    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "Characteristic by UUID callback: Failure: %s\n",
                att_ecode2str(status));
        goto error;
    }

    for (l = characteristics; l; l = l->next) {
        // Extract data
        struct gatt_char *chars = l->data;
        bl_char_t *bl_char = bl_char_new(chars->uuid, chars->handle,
                                         chars->properties, chars->value_handle);

        // Add it to the characteristic
        if (bl_char == NULL) {
            cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
            strcpy(cb_ctx->cb_ret_msg, "Characteristic by UUID callback: Malloc error\n");
            goto error;
        }

        // Append it to the list
        if (bl_char_list == NULL) {
            bl_char_list = g_slist_alloc();
            if (bl_char_list == NULL) {
                cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
                strcpy(cb_ctx->cb_ret_msg, "Characteristic by UUID callback: Malloc error\n");
                goto error;
            }
            bl_char_list->data = bl_char;
        } else {
            bl_char_list = g_slist_append(bl_char_list, bl_char);
        }
    }

    cb_ctx->cb_ret_val     = BL_NO_ERROR;
    cb_ctx->cb_ret_pointer = bl_char_list;
    goto exit;

error:
    if (bl_char_list)
        bl_char_list_free(bl_char_list);
exit:
    if (l)
        g_slist_free(l);
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT char_by_uuid\n");
}

static GSList *bl_desc_list = NULL;
void char_desc_cb(guint8 status, const guint8 *pdu, guint16 plen,
                  gpointer user_data) {
    struct att_data_list *list   = NULL;
    guint8                format;
    uint16_t              handle = 0xffff;
    int                   i;
    char                  uuid_str[MAX_LEN_UUID_STR];
    uint8_t              *value;
    cb_ctx_t             *cb_ctx = user_data;

    printf_dbg("[CB] IN char_desc_cb\n");
    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "Characteristic descriptor "
                "callback: Failure: %s\n", att_ecode2str(status));
        goto exit;
    }

    list = dec_find_info_resp(pdu, plen, &format);
    if (list == NULL) {
        cb_ctx->cb_ret_val = BL_NO_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "Characteristic descriptor callback: Nothing found\n");
        goto exit;
    }

    for (i = 0; i < list->num; i++) {
        bt_uuid_t uuid;

        value = list->data[i];
        handle = att_get_u16(value);

        if (format == 0x01)
            uuid = att_get_uuid16(&value[2]);
        else
            uuid = att_get_uuid128(&value[2]);

        bt_uuid_to_string(&uuid, uuid_str, MAX_LEN_UUID_STR);
        if (strcmp(uuid_str, GATT_PRIM_SVC_UUID_STR) &&
            strcmp(uuid_str, GATT_SND_SVC_UUID_STR)  &&
            strcmp(uuid_str, GATT_INCLUDE_UUID_STR)  &&
            strcmp(uuid_str, GATT_CHARAC_UUID_STR)) {
            bl_desc_t *bl_desc = bl_desc_new(uuid_str, handle);
            if (bl_desc == NULL) {
                cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
                strcpy(cb_ctx->cb_ret_msg, "Characteristic descriptor callback: Malloc "
                       "error\n");
                goto exit;
            }
            if (bl_desc_list == NULL) {
                bl_desc_list = g_slist_alloc();
                if (bl_desc_list == NULL) {
                    cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
                    strcpy(cb_ctx->cb_ret_msg, "Characteristic descriptor callback: Malloc "
                           "error\n");
                    goto exit;
                }
                bl_desc_list->data = bl_desc;
            } else {
                bl_desc_list = g_slist_append(bl_desc_list, bl_desc);
            }
        } else {
            printf_dbg("Reach end of descriptor list\n");
            goto exit;
        }
    }
    if ((handle != 0xffff) && (handle < cb_ctx->end_handle_cb)) {
        printf_dbg("[CB] OUT with asking for a new request\n");
        if (gatt_discover_char_desc(cb_ctx->dev_ctx->attrib, handle + 1,
                                    cb_ctx->end_handle_cb, char_desc_cb,
                                    cb_ctx)) {
            goto next;
        }
        cb_ctx->cb_ret_val = BL_SEND_REQUEST_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "Unable to send request\n");
    }

exit:
    if (bl_desc_list) {
        // Return what we got if we add something
        cb_ctx->cb_ret_val = BL_NO_ERROR;
        cb_ctx->cb_ret_pointer = bl_desc_list;
    }
    bl_desc_list = NULL;
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
next:
    if (list)
        att_data_list_free(list);
    printf_dbg("[CB] OUT char_desc_cb\n");
}

void read_by_hnd_cb(guint8 status, const guint8 *pdu, guint16 plen,
                    gpointer user_data)
{
    uint8_t   data[plen];
    ssize_t   vlen;
    cb_ctx_t *cb_ctx = user_data;

    printf_dbg("[CB] IN read_by_hnd_cb\n");
    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "Read by handle callback: Failure: %s\n",
                att_ecode2str(status));
        goto error;
    }

    if (data == NULL) {
        cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "Read by handle callback: Malloc error\n");
        goto error;
    }

    vlen = dec_read_resp(pdu, plen, data, sizeof(data));
    if (vlen < 0) {
        cb_ctx->cb_ret_val = BL_PROTOCOL_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "Read by handle callback: Protocol error\n");
        goto error;
    }

    cb_ctx->cb_ret_pointer = bl_value_new(NULL, 0, vlen, data);
    if (cb_ctx->cb_ret_pointer == NULL) {
        cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "Read by handle callback: Malloc error\n");
    }

    cb_ctx->cb_ret_val = BL_NO_ERROR;
    goto exit;

error:
    if (cb_ctx->cb_ret_pointer)
        free(cb_ctx->cb_ret_pointer);
exit:
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT read_by_hnd_cb\n");
}

void read_by_uuid_cb(guint8 status, const guint8 *pdu, guint16 plen,
                     gpointer user_data)
{
    struct att_data_list *list;
    GSList               *bl_value_list = NULL;
    cb_ctx_t             *cb_ctx = user_data;

    printf_dbg("[CB] IN read_by_uuid_cb\n");
    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "Read by uuid callback: Failure: %s\n",
                att_ecode2str(status));
        goto error;
    }

    list = dec_read_by_type_resp(pdu, plen);
    if (list == NULL) {
        strcpy(cb_ctx->cb_ret_msg, "Read by uuid callback: Nothing found\n");
        cb_ctx->cb_ret_val = BL_NO_ERROR;
        goto error;
    }

    for (int i = 0; i < list->num; i++) {
        bl_value_t *bl_value = bl_value_new(NULL, att_get_u16(list->data[i]),
                                            list->len - 2, list->data[i] + 2);
        if (bl_value == NULL) {
            cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
            strcpy(cb_ctx->cb_ret_msg, "Read by uuid callback: Malloc error\n");
            goto error;
        }

        // Add it to the value list
        if (bl_value_list == NULL) {
            bl_value_list = g_slist_alloc();
            if (bl_value_list == NULL) {
                cb_ctx->cb_ret_val = BL_MALLOC_ERROR;
                strcpy(cb_ctx->cb_ret_msg, "Read by uuid callback: Malloc error\n");
                goto error;
            }
            bl_value_list->data = bl_value;
        } else {
            bl_value_list = g_slist_append(bl_value_list, bl_value);
        }
    }

    att_data_list_free(list);

    cb_ctx->cb_ret_pointer = bl_value_list;
    cb_ctx->cb_ret_val     = BL_NO_ERROR;
    goto exit;

error:
    if (bl_value_list)
        bl_value_list_free(bl_value_list);
exit:
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT read_by_uuid_cb\n");
}

void write_req_cb(guint8 status, const guint8 *pdu, guint16 plen,
                  gpointer user_data)
{
    cb_ctx_t *cb_ctx = user_data;

    printf_dbg("[CB] IN write_req_cb\n");
    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "Write request callback: Failure: %s\n",
                att_ecode2str(status));
        goto end;
    }

    if (!dec_write_resp(pdu, plen) && !dec_exec_write_resp(pdu, plen)) {
        cb_ctx->cb_ret_val = BL_PROTOCOL_ERROR;
        printf("Write request callback: Protocol error\n");
        goto end;
    }

    cb_ctx->cb_ret_val = BL_NO_ERROR;
    strcpy(cb_ctx->cb_ret_msg, "Write request callback: Success\n");
end:
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT write_req_cb\n");
}

void exchange_mtu_cb(guint8 status, const guint8 *pdu, guint16 plen,
                     gpointer user_data)
{
    uint16_t  mtu;
    cb_ctx_t *cb_ctx = user_data;

    printf_dbg("[CB] IN exchange_mtu_cb\n");

    if (status) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        sprintf(cb_ctx->cb_ret_msg, "MTU exchange callback: Failure: %s\n",
                att_ecode2str(status));
        goto error;
    }

    if (!dec_mtu_resp(pdu, plen, &mtu)) {
        cb_ctx->cb_ret_val = BL_PROTOCOL_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "MTU exchange callback: PROTOCOL ERROR\n");
        goto error;
    }

    mtu = MIN(mtu, cb_ctx->dev_ctx->opt_mtu);
    /* Set new value for MTU in client */
    if (!g_attrib_set_mtu(cb_ctx->dev_ctx->attrib, mtu)) {
        cb_ctx->cb_ret_val = BL_REQUEST_FAIL_ERROR;
        strcpy(cb_ctx->cb_ret_msg, "MTU exchange callback: Unable to set new MTU value "
               "in client\n");
    } else {
        sprintf(cb_ctx->cb_ret_msg, "MTU exchange callback: Success: %d\n", mtu);
        cb_ctx->cb_ret_val = BL_NO_ERROR;
    }
error:
    g_mutex_unlock(&cb_ctx->pending_cb_mtx);
    printf_dbg("[CB] OUT exchange_mtu_cb\n");
}
