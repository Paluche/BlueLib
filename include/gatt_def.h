/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
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

#ifndef _GATT_DEF_H_
#define _GATT_DEF_H_


/* GATT Profile Attribute types */
#define GATT_PRIM_SVC_UUID_STR                "2800"
#define GATT_SND_SVC_UUID_STR                 "2801"
#define GATT_INCLUDE_UUID_STR                 "2802"
#define GATT_CHARAC_UUID_STR                  "2803"

/* GATT Characteristic Types */
#define GATT_CHARAC_DEVICE_NAME_STR           "2A00"
#define GATT_CHARAC_APPEARANCE_STR            "2A01"
#define GATT_CHARAC_PERIPHERAL_PRIV_FLAG_STR  "2A02"
#define GATT_CHARAC_RECONNECTION_ADDRESS_STR  "2A03"
#define GATT_CHARAC_PERIPHERAL_PREF_CONN_STR  "2A04"
#define GATT_CHARAC_SERVICE_CHANGED_STR       "2A05"

/* GATT Characteristic Descriptors */
#define GATT_CHARAC_EXT_PROPER_UUID_STR       "2900"
#define GATT_CHARAC_USER_DESC_UUID_STR        "2901"
#define GATT_CLIENT_CHARAC_CFG_UUID_STR       "2902"
#define GATT_SERVER_CHARAC_CFG_UUID_STR       "2903"
#define GATT_CHARAC_FMT_UUID_STR              "2904"
#define GATT_CHARAC_AGREG_FMT_UUID_STR        "2905"
#define GATT_CHARAC_VALID_RANGE_UUID_STR      "2906"
#define GATT_EXTERNAL_REPORT_REFERENCE_STR    "2907"
#define GATT_REPORT_REFERENCE_STR             "2908"

/* Client Characteristic Configuration bit field */
#define GATT_CLIENT_CHARAC_CFG_NOTIF_BIT      0x0001
#define GATT_CLIENT_CHARAC_CFG_IND_BIT        0x0002

#endif
