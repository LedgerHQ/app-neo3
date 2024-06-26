/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
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

#include "validate.h"
#include "menu.h"
#include "sw.h"
#include "io.h"
#include "crypto.h"
#include "globals.h"
#include "helper/send_response.h"

void ui_action_validate_pubkey(bool approved, bool go_back_to_menu) {
    if (approved) {
        helper_send_response_pubkey();
    } else {
        io_send_sw(SW_DENY);
    }

    if (go_back_to_menu) {
        ui_menu_main();
    }
}

void ui_action_validate_transaction(bool approved, bool go_back_to_menu) {
    if (approved) {
        G_context.state = STATE_APPROVED;

        if (crypto_sign_tx() < 0) {
            G_context.state = STATE_NONE;
            io_send_sw(SW_SIGN_FAIL);
        } else {
            io_send_response(&(const buffer_t){.ptr = G_context.tx_info.signature,
                                               .size = G_context.tx_info.signature_len,
                                               .offset = 0},
                             SW_OK);
        }
    } else {
        G_context.state = STATE_NONE;
        io_send_sw(SW_DENY);
    }

    if (go_back_to_menu) {
        ui_menu_main();
    }
}
