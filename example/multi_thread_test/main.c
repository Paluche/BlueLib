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
    printf("Description: This program make a get_ble_tree on two different de"
           "vices, each get_ble_tree is made within two seperate threads.\nUs"
           "age: ble_tree <MAC address 1> <MAC address 2> <file name 1> <file"
           " name 2>\n");
}

static int   error_cnt = 0;
static dev_ctx_t dev_ctx;

// Return 0 if no errors
// Returns 1 if function need to be called again
static int check_errors(int thd_nb, int code)
{
    printf("[THD%d]Error code = %d\n", thd_nb, code);
    if (code != BL_NO_ERROR) {
        printf("[THD%d]error count: %d\n", thd_nb, error_cnt);
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
                printf("[THD%d] Going on anyway\n", thd_nb);
                return 0;

            case BL_NO_CTX_ERROR:
                while(bl_init(NULL)) {
                    printf("[THD%d] error count: %d\n", thd_nb, error_cnt);
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
                    printf("[THD%d] Next try in 10 seconds\n", thd_nb);
                    sleep(10);
                    printf("[THD%d] Try to reconnect\n", thd_nb);
                    int ret = bl_connect(&dev_ctx);
                    if ((ret != BL_NO_ERROR) &&
                        (ret != BL_ALREADY_CONNECTED_ERROR) &&
                        (ret != BL_NOT_NOTIFIABLE_ERROR )) {
                        printf("[THD%d] ERROR <%d>\n", thd_nb, ret);
                        if (error_cnt < RETRY_MAX) {
                            error_cnt++;
                            printf("[THD%d] Error count: %d\n", thd_nb, error_cnt);
                        } else
                            exit(-1);
                    } else {
                        printf("[THD%d] Reconnected\n", thd_nb);
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

static int check_gerrors(int thd_nb, GError *gerr)
{
    int ret = 0;
    if (gerr) {
        printf("[THD%d] %s", thd_nb, gerr->message);
        ret = check_errors(thd_nb, gerr->code);
        g_error_free(gerr);
        gerr = NULL;
    } else {
        error_cnt = 0;
    }
    return ret;
}



static int get_ble_tree(const int thd_nb, char *mac,
                        const char *file_path)
{

    int      ret_int;
    GError  *gerr             = NULL;
    GSList  *bl_primary_list  = NULL;
    GSList  *bl_included_list = NULL;
    GSList  *bl_char_list     = NULL;
    GSList  *bl_desc_list     = NULL;
    bl_value_t  *bl_value     = NULL;

    FILE  *file = fopen(file_path, "w");
    if (!file)
        return -1;
    // Initialisation
    bl_init(&gerr);
    if (check_gerrors(thd_nb, gerr)) {
        printf("[THD%d] ERROR: Unable to initalise BlueLib\n", thd_nb);
        return -1;
    }
    dev_init(&dev_ctx, NULL, mac, "random", 0, TEST_SEC_LEVEL);

    do {
        ret_int = bl_connect(&dev_ctx);
    } while (check_errors(thd_nb, ret_int));

    if (ret_int)
        return -1;

    do {
        bl_value = bl_read_char(&dev_ctx, GATT_CHARAC_DEVICE_NAME_STR, NULL,
                                &gerr);
    } while(check_gerrors(thd_nb, gerr));

    printf("[THD%d] In progress\n", thd_nb);
    if (bl_value) {
        char device_name_str[bl_value->data_size + 1];
        memcpy(device_name_str, bl_value->data,
               bl_value->data_size);
        device_name_str[bl_value->data_size] = '\0';
        fprintf(file, "Device name: %s\n", device_name_str);
        bl_value_free(bl_value);
    } else {
        printf("[THD%d] Impossible to retrieve the name of the device \n",
               thd_nb);
        goto disconnect;
    }

    fprintf(file, "Handle |\n");

    do {
        bl_primary_list  = bl_get_all_primary(&dev_ctx, NULL, &gerr);
    } while (check_gerrors(thd_nb, gerr));

    printf("[THD%d].", thd_nb);

    if (bl_primary_list) {
        for (GSList *lp = bl_primary_list; lp; lp = lp->next) {
            printf(".");
            bl_primary_t *bl_primary = lp->data;

            bl_primary_fprint(file, lp->data);

            // Get all included in the primary service
            do {
                bl_included_list = bl_get_included(&dev_ctx, bl_primary,
                                                   &gerr);
            } while (check_gerrors(thd_nb, gerr));

            if (bl_included_list) {
                bl_included_list_fprint(file, bl_included_list);
                fprintf(file, "       |\n");
                bl_included_list_free(bl_included_list);
            }

            // Get all characteristics in the primary service
            do {
                bl_char_list = bl_get_all_char_in_primary(&dev_ctx,
                                                          bl_primary, &gerr);
            } while (check_gerrors(thd_nb, gerr));

            if (bl_char_list) {
                for (GSList *lc = bl_char_list; lc; lc = lc->next) {
                    putchar('.');
                    bl_char_t *bl_char = lc->data;

                    bl_char_fprint(file, bl_char);

                    // Get all descriptors of the characteristic
                    {
                        bl_char_t *next_bl_char = NULL;
                        if (lc->next)
                            next_bl_char = lc->next->data;
                        do {
                            bl_desc_list =
                                bl_get_all_desc_by_char(&dev_ctx, bl_char,
                                                        next_bl_char,
                                                        bl_primary, &gerr);
                        } while (check_gerrors(thd_nb, gerr));
                    }
                    if (bl_desc_list)
                        bl_desc_list_fprint(file, bl_desc_list);
                    bl_desc_list_free(bl_desc_list);
                    if (lc->next)
                        fprintf(file, "       | |\n");
                }
                bl_char_list_free(bl_char_list);
            }
            if (lp->next)
                fprintf(file, "       |\n");
        }
        bl_primary_list_free(bl_primary_list);
    }
    printf("[THD%d] All done!\n", thd_nb);

disconnect:
    if (file)
        fclose(file);
    printf("[THD%d] Disconnecting\n", thd_nb);
    bl_disconnect(&dev_ctx);
    bl_stop();
    return 0;
}

typedef struct {
    char *mac;
    char *file_path;
} arg_t;

static gpointer thd2(gpointer data)
{
    arg_t *arg = data;

    get_ble_tree(2, arg->mac, arg->file_path);

    g_thread_exit(0);
    return NULL;
}

int main(int argc, char **argv)
{
    printf("get_ble_tree  Copyright (C) 2014 Hubert Lefevre\nThis program com"
           "es with ABSOLUTELY NO WARRANTY\nThis is free software, and you ar"
           "e welcome to redistribute it\nunder certain conditions; See the G"
           "NU General Public License\nfor more details.\n\n");
    if(argc != 5) {
        usage();
        return 0;
    }
    arg_t thd2_arg;

    char *mac          = argv[1];
    thd2_arg.mac       = argv[2];
    char *file_path    = argv[3];
    thd2_arg.file_path = argv[4];
    GError *gerr       = NULL;

    g_thread_try_new("Thread 2", thd2, &thd2_arg, &gerr);

    return get_ble_tree(1, mac, file_path);
}
