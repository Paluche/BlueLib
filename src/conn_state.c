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
#include "conn_state.h"
#include "callback.h"
#include <stdio.h>


void set_conn_state(dev_ctx_t *dev_ctx, conn_state_t state)
{
  printf("[CONN STATE] %d => %d\n", dev_ctx->conn_state, state);
  dev_ctx->conn_state = state;
}

conn_state_t get_conn_state(dev_ctx_t *dev_ctx)
{
  return dev_ctx->conn_state;
}
