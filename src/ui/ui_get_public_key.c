/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *   (c) 2021 COZ Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "ux.h"
#include "glyphs.h"

#include "ui_get_public_key.h"
#include "constants.h"
#include "globals.h"
#include "io.h"
#include "sw.h"
#include "validate.h"
#include "transaction_types.h"
#include "format.h"
#include "utils.h"
#include "menu.h"
#include "shared_context.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

static char g_address[35];  // 34 + \0

#ifdef HAVE_BAGL

// Step with icon and text
UX_STEP_NOCB(ux_get_pub_key_confirm_addr_step, pn, {&C_icon_eye, "Confirm Address"});
// Step with title/text for address
UX_STEP_NOCB(ux_get_pub_key_address_step,
             bnnn_paging,
             {
                 .title = "Address",
                 .text = g_address,
             });

// Step with approve button
UX_STEP_CB(ux_get_pub_key_approve_step,
           pb,
           ui_action_validate_pubkey(true, true),
           {
               &C_icon_validate_14,
               "Approve",
           });

// Step with reject button
UX_STEP_CB(ux_get_pub_key_reject_step,
           pb,
           ui_action_validate_pubkey(false, true),
           {
               &C_icon_crossmark,
               "Reject",
           });

// FLOW to display address:
// #1 screen: eye icon + "Confirm Address"
// #2 screen: display address
// #3 screen: approve button
// #4 screen: reject button
UX_FLOW(ux_get_pub_key_pubkey_flow,
        &ux_get_pub_key_confirm_addr_step,
        &ux_get_pub_key_address_step,
        &ux_get_pub_key_approve_step,
        &ux_get_pub_key_reject_step);

#else

static void review_choice(bool confirm) {
     ui_action_validate_pubkey(confirm, false);
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_VERIFIED, ui_menu_main);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_REJECTED, ui_menu_main);
    }
}

static void ui_get_public_key_nbgl(void) {
    nbgl_useCaseAddressReview(
        g_address,
        NULL,
        &C_icon_neo_n3_64x64,
        "Verify " APPNAME "\naddress",
        NULL,
        review_choice);
}

#endif

int ui_display_address() {
    if (G_context.req_type != CONFIRM_ADDRESS || G_context.state != STATE_NONE) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    memset(g_address, 0, sizeof(g_address));
    char address[ADDRESS_LEN] = {0};  // address in base58 check encoded format
    if (!address_from_pubkey(G_context.raw_public_key, address, sizeof(address))) {
        return io_send_sw(SW_CONVERT_TO_ADDRESS_FAIL);
    }
    snprintf(g_address, sizeof(g_address), "%s", address);

#ifdef HAVE_BAGL
    ux_flow_init(0, ux_get_pub_key_pubkey_flow, NULL);
#else
    ui_get_public_key_nbgl();
#endif

    return 0;
}
