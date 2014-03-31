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

#include <stdio.h>
#include <stdlib.h>
#include "bluelib.h"
#include <glib.h>
#include <unistd.h>

#include "gatt_def.h"

#define TEST_SEC_LEVEL SECURITY_LEVEL_HIGH
#define RETRY_MAX  7
static void usage(void)
{
    printf("Description: This program realizes a get_ble_tree on two devices"
           " simultaneously.\nUsage: multi_slave_test <MAC address 1>  <MAC "
           "address 2>  <file name 1> <file name 2>\n");
}

static char     *mac_1 = NULL;
static char     *mac_2 = NULL;
static int       error_cnt = 0;
static dev_ctx_t dev_1;
static dev_ctx_t dev_2;

// Return 0 if no errors
// Returns 1 if function need to be called again
static int check_errors(dev_ctx_t *dev_ctx, int code)
{
    printf("Error code = %d\n", code);
    if (code != BL_NO_ERROR) {
        printf("error count: %d\n", error_cnt);
        if (error_cnt < RETRY_MAX)
            error_cnt++;
        else
            exit(-1);

        switch(code) {
            case BL_NO_ERROR:
            case BL_ALREADY_CONNECTED_ERROR:
            case BL_MTU_ALREADY_EXCHANGED_ERROR:
            case BL_UNICITY_ERROR:
            case BL_NOT_NOTIFIABLE_ERROR:
            case BL_NOT_INDICABLE_ERROR:
                /* For test */
            case EINVAL:
            case BL_REQUEST_FAIL_ERROR:
                error_cnt = 0;
                printf("OK going on\n");
                return 0;

            case BL_NO_CTX_ERROR:
                while(bl_init(NULL)) {
                    printf("error count: %d\n", error_cnt);
                    if (error_cnt < RETRY_MAX)
                        error_cnt++;
                    else
                        exit(-1);
                }
                return 1;
                break;

            case BL_RECONNECTION_NEEDED_ERROR:
            case BL_DISCONNECTED_ERROR:
            case BL_NO_CALLBACK_ERROR:
                for(;;) {
                    printf("Next try in 10 seconds\n");
                    sleep(10);
                    printf("Try to reconnect\n");
                    int ret = bl_connect(dev_ctx);
                    if ((ret != BL_NO_ERROR) &&
                        (ret != BL_ALREADY_CONNECTED_ERROR) &&
                        (ret != BL_NOT_NOTIFIABLE_ERROR )) {
                        printf("ERROR <%d>\n", ret);
                        if (error_cnt < RETRY_MAX) {
                            error_cnt++;
                            printf("error count: %d\n", error_cnt);
                        } else
                            exit(-1);
                    } else {
                        printf("Reconnected\n");
                        return 1;
                    }
                }
                break;

            case BL_SEND_REQUEST_ERROR:
                return 1;
                break;

            default:
                // FATAL ERRROS
            case BL_MALLOC_ERROR:
            case BL_LE_ONLY_ERROR:
            case BL_MISSING_ARGUMENT_ERROR:
            case BL_PROTOCOL_ERROR:
                exit(-1);
        }
    } else {
        error_cnt = 0;
        return 0;
    }
}

static int check_gerrors(dev_ctx_t *dev_ctx, GError *gerr)
{
    int ret = 0;
    if (gerr) {
        printf("%s", gerr->message);
        ret = check_errors(dev_ctx, gerr->code);
        g_error_free(gerr);
        gerr = NULL;
    } else {
        error_cnt = 0;
    }
    return ret;
}

int main(int argc, char **argv)
{
    printf("multi_slave_test  Copyright (C) 2014 Hubert Lefevre\nThis program"
           " comes with ABSOLUTELY NO WARRANTY\nThis is free software, and yo"
           "u are welcome to redistribute it\nunder certain conditions; See t"
           "he GNU General Public License\nfor more details.\n\n");
    if(argc != 5) {
        usage();
        return 0;
    }

    mac_1 = argv[1];
    mac_2 = argv[2];
    char *file_path_1 = argv[3];
    char *file_path_2 = argv[4];

    int          ret_int;
    GError      *gerr             = NULL;

    // Device 1
    GSList       *bl_primary_list_1  = NULL;
    bl_primary_t *bl_primary_1       = NULL;
    GSList       *bl_included_list_1 = NULL;
    GSList       *bl_char_list_1     = NULL;
    bl_char_t    *bl_char_1          = NULL;
    GSList       *bl_desc_list_1     = NULL;
    bl_value_t   *bl_value_1         = NULL;

    // Device 1
    GSList       *bl_primary_list_2  = NULL;
    bl_primary_t *bl_primary_2       = NULL;
    GSList       *bl_included_list_2 = NULL;
    GSList       *bl_char_list_2     = NULL;
    bl_char_t    *bl_char_2          = NULL;
    GSList       *bl_desc_list_2     = NULL;
    bl_value_t   *bl_value_2         = NULL;

    FILE  *file_1 = fopen(file_path_1, "w");
    FILE  *file_2 = fopen(file_path_2, "w");

    if (!file_1 || !file_2)
        return -1;

    // Initialisation
    bl_init(&gerr);
    if (check_gerrors(NULL, gerr)) {
        printf("ERROR: Unable to initalise BlueLib\n");
        return -1;
    }
#define DEVICE_INIT(D)                                                       \
    dev_init(&dev_##D, NULL, mac_##D, NULL, 0, TEST_SEC_LEVEL);              \
    do {                                                                     \
        ret_int = bl_connect(&dev_##D);                                      \
    } while (check_errors(&dev_##D, ret_int));                               \
                                                                             \
    if (ret_int)                                                             \
        goto disconnect;

    DEVICE_INIT(1);
    DEVICE_INIT(2);

    printf("In progress\n");

    // Retrieve devices name
#define GET_DEVICE_NAME(D)                                                   \
    do {                                                                     \
        bl_value_##D = bl_read_char(&dev_##D, GATT_CHARAC_DEVICE_NAME_STR,   \
                                    NULL, &gerr);                            \
    } while(check_gerrors(&dev_##D, gerr));                                  \
                                                                             \
    if (bl_value_##D) {                                                      \
        char device_name_str[bl_value_##D->data_size + 1];                   \
        memcpy(device_name_str, bl_value_##D->data,                          \
               bl_value_##D->data_size);                                     \
        device_name_str[bl_value_##D->data_size] = '\0';                     \
        fprintf(file_##D, "Device name: %s\n", device_name_str);             \
        bl_value_free(bl_value_##D);                                         \
    } else {                                                                 \
        printf("Impossible to retrieve the name of the device\n");           \
        goto disconnect;                                                     \
    }                                                                        \
    fprintf(file_##D, "Handle |\n");

    GET_DEVICE_NAME(1);
    GET_DEVICE_NAME(2);

    // Retrieve primary services list
#define GET_PRIMARY_LIST(D)                                                  \
    do {                                                                     \
        bl_primary_list_##D = bl_get_all_primary(&dev_##D, NULL, &gerr);     \
    } while (check_gerrors(&dev_##D, gerr));

    GET_PRIMARY_LIST(1);
    GET_PRIMARY_LIST(2);

    if (bl_primary_list_1 && bl_primary_list_2) {
        GSList *lp_1 = bl_primary_list_1;
        GSList *lp_2 = bl_primary_list_2;
        while (lp_1 || lp_2) {
            // Initialisation
            putchar('.');
#define PRIMARY_DATA_INIT(D)                                                 \
            if (lp_##D) {                                                    \
                bl_primary_##D = lp_##D->data;                               \
                bl_primary_fprint(file_##D, lp_##D->data);                   \
            } else {                                                         \
                bl_primary_##D = NULL;                                       \
            }
            PRIMARY_DATA_INIT(1);
            PRIMARY_DATA_INIT(2);

            // Get all included in the primary service
#define GET_INCLUDED(D)                                                      \
            if (bl_primary_##D) {                                            \
                do {                                                         \
                    bl_included_list_##D = bl_get_included(&dev_##D,         \
                                                           bl_primary_##D,   \
                                                           &gerr);           \
                } while (check_gerrors(&dev_##D, gerr));                     \
                                                                             \
                if (bl_included_list_##D) {                                  \
                    bl_included_list_fprint(file_##D, bl_included_list_##D); \
                    fprintf(file_##D, "       |\n");                         \
                    bl_included_list_free(bl_included_list_##D);             \
                }                                                            \
            }
            GET_INCLUDED(1);
            GET_INCLUDED(2);

            // Get all characteristics in the primary service
#define GET_CHAR_LIST(D)                                                     \
            if (bl_primary_##D) {                                            \
                do {                                                         \
                    bl_char_list_##D =                                       \
                    bl_get_all_char_in_primary(&dev_##D, bl_primary_##D,     \
                                               &gerr);                       \
                } while (check_gerrors(&dev_##D, gerr));                     \
            } else {                                                         \
                bl_char_list_##D = NULL;                                     \
            }
            GET_CHAR_LIST(1);
            GET_CHAR_LIST(2);

            if (bl_char_list_1) {
                GSList *lc_1 = bl_char_list_1;
                GSList *lc_2 = bl_char_list_2;
                while (lc_1 || lc_2) {
                    putchar('.');
#define CHAR_DATA_INIT(D)                                                    \
                    if (lp_##D) {                                            \
                        bl_char_##D = lc_##D->data;                          \
                        bl_char_fprint(file_##D, bl_char_##D);               \
                    } else {                                                 \
                        bl_char_##D = NULL;                                  \
                    }
                    CHAR_DATA_INIT(1);
                    CHAR_DATA_INIT(2);

                    // Get all descriptors of the characteristic
#define GET_DESCRIPTOR(D)                                                    \
                    {                                                        \
                        bl_char_t *next_bl_char_##D = NULL;                  \
                        if (lc_##D->next)                                    \
                            next_bl_char_##D = lc_##D->next->data;           \
                        do {                                                 \
                            bl_desc_list_##D =                               \
                                bl_get_all_desc_by_char(&dev_##D,            \
                                                        bl_char_##D,         \
                                                        next_bl_char_##D,    \
                                                        bl_primary_##D,      \
                                                        &gerr);              \
                        } while (check_gerrors(&dev_##D, gerr));             \
                    }                                                        \
                    if (bl_desc_list_##D)                                    \
                        bl_desc_list_fprint(file_##D, bl_desc_list_##D);     \
                    bl_desc_list_free(bl_desc_list_##D);

                    GET_DESCRIPTOR(1);
                    GET_DESCRIPTOR(2);

                    // Next char on list
#define NEXT_CHAR(D)                                                        \
                    if (lc_##D) {                                           \
                        if (lc_##D->next)                                   \
                            fprintf(file_##D, "       | |\n");              \
                        lc_##D = lc_##D->next;                              \
                    }

                    NEXT_CHAR(1);
                    NEXT_CHAR(2)
                }
                bl_char_list_free(bl_char_list_1);
            }

            // Next primary service
#define NEXT_PRIMARY(D)                                                     \
            if (lp_##D) {                                                   \
                if (lp_##D->next)                                           \
                fprintf(file_##D, "       |\n");                            \
                lp_##D = lp_##D->next;                                      \
            }

            NEXT_PRIMARY(1);
            NEXT_PRIMARY(2);
        }

        bl_primary_list_free(bl_primary_list_1);
        bl_primary_list_free(bl_primary_list_2);
    }
    printf("\nAll done!\n");

disconnect:
#define DISCONNECT(D)                                                        \
    printf("Disconnecting\n");                                               \
    if (file_##D)                                                            \
        fclose(file_##D);                                                    \
    bl_disconnect(&dev_##D);
    DISCONNECT(1);
    DISCONNECT(2);

    bl_stop();
    return 0;
}
