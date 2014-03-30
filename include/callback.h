/*
 *  BlueLib - Abstraction layer for Bluetooth Low Energy softwares
 *
 *  Copyright (C) 2013  Netatmo
 *  Copyright (C) 2014  Hubert Lefevre
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

#ifndef _CALLBACK_H_
#define _CALLBACK_H_

#include <stdint.h>
#include "bluelib.h"

typedef struct {
    dev_ctx_t *dev_ctx;
    GMutex     pending_cb_mtx;
    uint16_t   end_handle_cb; // Used in only some callbacks.

    // Return value from the callback functions
    void      *cb_ret_pointer;
    int        cb_ret_val;
    char       cb_ret_msg[1024];
} cb_ctx_t;

// Initializes the structure you must give to every callback in user_data
void init_cb_ctx(cb_ctx_t *cb_ctx, dev_ctx_t *dev_ctx);

// Event loop
int  start_event_loop(GError **gerr);
void stop_event_loop(void);
int  is_event_loop_running(void);

// Block the main thread while waiting for the callback
int wait_for_cb(cb_ctx_t *cb_ctx, void **ret_pointer, GError **gerr);

// Callbacks
void connect_cb(GIOChannel *io, GError *err, gpointer user_data);
void primary_all_cb(GSList *services, guint8 status,
                    gpointer user_data);
void primary_by_uuid_cb(GSList *ranges, guint8 status,
                        gpointer user_data);
void included_cb(GSList *includes, guint8 status, gpointer user_data);
void char_by_uuid_cb(GSList *characteristics, guint8 status,
                     gpointer user_data);
void char_desc_cb(guint8 status, const guint8 *pdu, guint16 plen,
                  gpointer user_data);
void read_by_hnd_cb(guint8 status, const guint8 *pdu, guint16 plen,
                    gpointer user_data);
void read_by_uuid_cb(guint8 status, const guint8 *pdu,
                     guint16 plen, gpointer user_data);
void write_req_cb(guint8 status, const guint8 *pdu, guint16 plen,
                  gpointer user_data);
void exchange_mtu_cb(guint8 status, const guint8 *pdu, guint16 plen,
                     gpointer user_data);
#endif
