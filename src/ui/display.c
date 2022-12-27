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

#include "display.h"
#include "constants.h"
#include "../globals.h"
#include "../io.h"
#include "../sw.h"
#include "action/validate.h"
#include "../transaction/types.h"
#include "../common/format.h"
#include "utils.h"
#include "menu.h"
#include "../shared_context.h"

#ifdef HAVE_NBGL
#include "nbgl_fonts.h"
#include "nbgl_front.h"
#include "nbgl_debug.h"
#include "nbgl_page.h"
#include "nbgl_use_case.h"
#endif


static char g_dst_address[35];  // 34 + \0
static char g_system_fee[30];
static char g_network_fee[30];
static char g_total_fees[30];
static char g_token_amount[30];
static char g_vote_to[67];            // 33 bytes public key + \0
static char g_network[11];            // Target network the tx in tended for
                                      // ("MainNet", "TestNet" or uint32 network number for private nets)
static char g_valid_until_block[11];  // uint32 (=max 10 chars) + \0
static char g_scope[28];              // Longest combination is: "By Entry, Contracts, Groups" (27) + \0
static char g_title[64];              // generic step title
static char g_text[64];               // generic step text


/**
 * Hold state around displaying Signers and their properties
 */
struct display_ctx_t {
    enum e_state current_state;  // screen state
    int8_t s_index;              // track which signer is to be displayed
    uint8_t p_index;             // track which signer property is displayed (see also: e_signer_state)
    int8_t c_index;              // track which signer.contract is to be displayed
    int8_t g_index;              // track which signer.group is to be displayed
} display_ctx;

#define MAX_NUM_STEPS 13

static void add_witness_scope_flag(char *dest, size_t len, const char *flag_name) {
    if (strlen(dest) != 0) {
        strlcat(dest, ",", len);
    }
    strlcat(dest, flag_name, len);
}

static int parse_scope_name(witness_scope_e scope) {
    if (scope == NONE) {
        strcpy(g_scope, "None");
    } else if (scope == GLOBAL) {
        strcpy(g_scope, "Global");
    } else {
        memset(g_scope, 0, sizeof(scope));
        if (scope & CALLED_BY_ENTRY) {
            add_witness_scope_flag(g_scope, sizeof(g_scope), "By Entry");
        };

        if (scope & CUSTOM_CONTRACTS) {
            add_witness_scope_flag(g_scope, sizeof(g_scope), "Contracts");
        };

        if (scope & CUSTOM_GROUPS) {
            add_witness_scope_flag(g_scope, sizeof(g_scope), "Groups");
        }
    }
    return strlen(g_scope);
}


#ifdef HAVE_BAGL

enum e_signer_state { START = 0, INDEX = 1, ACCOUNT = 2, SCOPE = 3, CONTRACTS = 4, GROUPS = 5, END = 6 };

static enum e_signer_state signer_property[7] = {START, INDEX, ACCOUNT, SCOPE, CONTRACTS, GROUPS, END};

const ux_flow_step_t *ux_display_transaction_flow[MAX_NUM_STEPS + 1];

// This is a special function you must call for bnnn_paging to work properly in an edgecase.
// It does some weird stuff with the `G_ux` global which is defined by the SDK.
// No need to dig deeper into the code, a simple copy paste will do.
static void bnnn_paging_edgecase(void) {
    G_ux.flow_stack[G_ux.stack_count - 1].prev_index = G_ux.flow_stack[G_ux.stack_count - 1].index - 2;
    G_ux.flow_stack[G_ux.stack_count - 1].index--;
    ux_flow_relayout();
}

// Taken from Ledger's advanced display management docs
static void display_next_state(bool is_upper_delimiter) {
    if (is_upper_delimiter) {  // We're called from the upper delimiter.
        if (display_ctx.current_state == STATIC_SCREEN) {
            // Fetch new data.
            bool dynamic_data = get_next_data(DIRECTION_FORWARD);
            if (dynamic_data) {
                // We found some data to display so we now enter in dynamic mode.
                display_ctx.current_state = DYNAMIC_SCREEN;
            }

            // Move to the next step, which will display the screen.
            ux_flow_next();
        } else {
            // The previous screen was NOT a static screen, so we were already in a dynamic screen.

            // Fetch new data.
            bool dynamic_data = get_next_data(DIRECTION_BACKWARD);
            if (dynamic_data) {
                // We found some data so simply display it.
                ux_flow_next();
            } else {
                // There's no more dynamic data to display, so
                // update the current state accordingly.
                display_ctx.current_state = STATIC_SCREEN;

                // Display the previous screen which should be a static one.
                ux_flow_prev();
            }
        }
    } else {
        // We're called from the lower delimiter.
        if (display_ctx.current_state == STATIC_SCREEN) {
            // Fetch new data.
            bool dynamic_data = get_next_data(DIRECTION_BACKWARD);
            if (dynamic_data) {
                // We found some data to display so enter in dynamic mode.
                display_ctx.current_state = DYNAMIC_SCREEN;
            }

            // Display the data.
            ux_flow_prev();
        } else {
            // We're being called from a dynamic screen, so the user was already browsing the array.

            // Fetch new data.
            bool dynamic_data = get_next_data(DIRECTION_FORWARD);
            if (dynamic_data) {
                // We found some data, so display it.
                // Similar to `ux_flow_prev()` but updates layout to account for `bnnn_paging`'s
                // weird behaviour.
                bnnn_paging_edgecase();
            } else {
                // We found no data so make sure we update the state accordingly.
                display_ctx.current_state = STATIC_SCREEN;
                // Display the next screen
                ux_flow_next();
            }
        }
    }
}

// Step with icon and text
UX_STEP_NOCB(ux_display_review_step,
             pnn,
             {
                 &C_icon_eye,
                 "Review",
                 "Transaction",
             });

UX_STEP_NOCB(ux_display_dst_address_step,
             bnnn_paging,
             {
                 .title = "Destination addr",
                 .text = g_dst_address,
             });

UX_STEP_NOCB(ux_display_token_amount_step,
             bnnn_paging,
             {
                 .title = "Token amount",
                 .text = g_token_amount,
             });

UX_STEP_NOCB(ux_display_systemfee_step,
             bnnn_paging,
             {
                 .title = "System fee",
                 .text = g_system_fee,
             });

UX_STEP_NOCB(ux_display_network_step,
             bnnn_paging,
             {
                 .title = "Target network",
                 .text = g_network,
             });

UX_STEP_NOCB(ux_display_networkfee_step,
             bnnn_paging,
             {
                 .title = "Network fee",
                 .text = g_network_fee,
             });

UX_STEP_NOCB(ux_display_total_fee,
             bnnn_paging,
             {
                 .title = "Total fees",
                 .text = g_total_fees,
             });

UX_STEP_NOCB(ux_display_validuntilblock_step,
             bnnn_paging,
             {
                 .title = "Valid until height",
                 .text = g_valid_until_block,
             });

UX_STEP_NOCB(
    ux_display_no_arbitrary_script_step,
    bnnn_paging,
    {
        .title = "Error",
        .text = "Arbitrary contract scripts are not allowed. Go to Settings to enable signing of such transactions",
    });

UX_STEP_CB(ux_display_abort_step,
           pb,
           ui_action_validate_transaction(false),
           {
               &C_icon_validate_14,
               "Understood, abort..",
           });

UX_STEP_NOCB(ux_display_vote_to_step,
             bnnn_paging,
             {
                 .title = "Casting vote for",
                 .text = g_vote_to,
             });

UX_STEP_NOCB(ux_display_vote_retract_step, nn, {"Retracting vote", ""});

// 3 special steps for runtime dynamic screen generation, used to display attached signers and their properties
UX_STEP_INIT(ux_upper_delimiter, NULL, NULL, { display_next_state(true); });

UX_STEP_NOCB(ux_display_generic,
             bnnn_paging,
             {
                 .title = g_title,
                 .text = g_text,
             });

UX_STEP_INIT(ux_lower_delimiter, NULL, NULL, { display_next_state(false); });

// Step with approve button
UX_STEP_CB(ux_display_approve_step,
           pb,
           ui_action_validate_transaction(true),
           {
               &C_icon_validate_14,
               "Approve",
           });

// Step with reject button
UX_STEP_CB(ux_display_reject_step,
           pb,
           ui_action_validate_transaction(false),
           {
               &C_icon_crossmark,
               "Reject",
           });
#else

typedef enum item_kind_e {
    SIGNER,
    ACCOUNT,
    SCOPE,
    CONTRACT,
    GROUP,
} item_kind_t;

typedef struct item_signer_s {
    uint8_t signer_index;
} item_signer_t;

typedef struct item_account_s {
    uint8_t signer_index;
} item_account_t;

typedef struct item_scope_s {
    uint8_t signer_index;
} item_scope_t;

typedef struct item_contract_s {
    uint8_t signer_index;
    uint8_t contract_index;
} item_contract_t;

typedef struct item_group_s {
    uint8_t signer_index;
    uint8_t group_index;
} item_group_t;

typedef struct dynamic_item_s {
    item_kind_t kind;
    union {
        item_signer_t as_item_signer;
        item_account_t as_item_account;
        item_scope_t as_item_scope;
        item_contract_t as_item_contract;
        item_group_t as_item_group;
    } content;
} dynamic_item_t;

typedef struct dynamic_slot_s {
    char title[64];
    char text[64];
} dynamic_slot_t;

static void start_review(void);
static void ui_sign_message_nbgl(void);
static void rejectUseCaseChoice(void);
static nbgl_layoutTagValueList_t layout;
static nbgl_layoutTagValue_t current_pair;
static nbgl_layoutTagValue_t static_items[MAX_NUM_STEPS + 1];
static dynamic_slot_t dyn_slots[4];
static uint8_t static_items_nb;
static dynamic_item_t dyn_items[64];
static uint8_t dyn_items_nb;

static const nbgl_pageInfoLongPress_t review_final_long_press = {
    .text = "Sign message on\nNeo N3 network?",
    .icon = &C_icon_neo_n3_64x64,
    .longPressText = "Hold to sign",
    .longPressToken = 0,
    .tuneId = TUNE_TAP_CASUAL,
};

static void review_final_callback(bool confirmed) {
    if (confirmed) {
        ui_action_validate_transaction(true);
        nbgl_useCaseStatus("MESSAGE\nSIGNED", true, ui_menu_main);
    } else {
        rejectUseCaseChoice();
    }
}

static void rejectChoice(void) {
    ui_action_validate_transaction(false);
    nbgl_useCaseStatus("MESSAGE\nREJECTED",false,ui_menu_main);
}

static void rejectUseCaseChoice(void) {
    nbgl_useCaseConfirm("Reject message?",NULL,"Yes, reject","Go back to message",rejectChoice);
}

// function called by NBGL to get the current_pair indexed by "index"
static nbgl_layoutTagValue_t* get_single_action_review_pair(uint8_t index) {
    PRINTF("get_single_action_review_pair item %d\n", index);
    if (index < static_items_nb) {
        // No need to copy to dyn_slots as item and value are pointers to static values
        current_pair.item = static_items[index].item;
        current_pair.value = static_items[index].value;
    } else {
        dynamic_item_t *current_item = &dyn_items[index - static_items_nb];
        signer_t s;
        dynamic_slot_t *slot = &dyn_slots[index % 4];
        switch (current_item->kind) {
            case SIGNER:
                strlcpy(slot->title, "Signer", sizeof(slot->title));
                snprintf(slot->text, sizeof(slot->text), "%d of %d", current_item->content.as_item_signer.signer_index + 1, G_context.tx_info.transaction.signers_size);
                break;

            case ACCOUNT:
                s = G_context.tx_info.transaction.signers[current_item->content.as_item_account.signer_index];
                strlcpy(slot->title, "Account", sizeof(slot->title));
                format_hex(s.account, 20, slot->text, sizeof(slot->text));
                break;

            case SCOPE:
                s = G_context.tx_info.transaction.signers[current_item->content.as_item_scope.signer_index];
                strlcpy(slot->title, "Scope", sizeof(slot->title));
                int scope_size = parse_scope_name(s.scope);
                snprintf(slot->text, sizeof(slot->text), "%.*s", scope_size, g_scope);
                break;

            case CONTRACT:
                s = G_context.tx_info.transaction.signers[current_item->content.as_item_contract.signer_index];
                snprintf(slot->title, sizeof(slot->title), "Contract %d of %d", current_item->content.as_item_contract.contract_index + 1, s.allowed_contracts_size);
                format_hex(s.allowed_contracts[current_item->content.as_item_contract.contract_index], UINT160_LEN, slot->text, sizeof(slot->text));
                break;

            case GROUP:
                s = G_context.tx_info.transaction.signers[current_item->content.as_item_group.signer_index];
                snprintf(slot->title, sizeof(slot->title), "Group %d of %d", current_item->content.as_item_group.group_index + 1, s.allowed_groups_size);
                format_hex(s.allowed_groups[current_item->content.as_item_group.group_index], ECPOINT_LEN, slot->text, sizeof(slot->text));
                break;
        }
        current_pair.item = slot->title;
        current_pair.value = slot->text;
    }
    PRINTF("current_pair.item = %s\n", current_pair.item);
    PRINTF("current_pair.value = %s\n", current_pair.value);
    return &current_pair;
}

static void start_review(void) {
    layout.nbMaxLinesForValue = 0;
    layout.smallCaseForValue = true;
    layout.wrapping = true;
    layout.pairs = NULL; // to indicate that callback should be used
    layout.callback = get_single_action_review_pair;
    layout.startIndex = 0;
    layout.nbPairs = static_items_nb + dyn_items_nb;

    nbgl_useCaseStaticReview(&layout, &review_final_long_press, "Reject message", review_final_callback);
}

static void ui_sign_message_nbgl(void) {
    nbgl_useCaseReviewStart(&C_icon_neo_n3_64x64,
                            "Review message to\nsign on Neo N3\nnetwork",
                            "",
                            "Reject message",
                            start_review,
                            rejectUseCaseChoice);
}

#endif

void reset_signer_display_state() {
    display_ctx.current_state = STATIC_SCREEN;
    display_ctx.s_index = 0;
    display_ctx.g_index = -1;
    display_ctx.c_index = -1;
    display_ctx.p_index = 0;
}

static void create_transaction_flow(void) {

#ifdef HAVE_BAGL
    uint8_t index = 0;

    if (!G_context.tx_info.transaction.is_system_asset_transfer && !G_context.tx_info.transaction.is_vote_script &&
        !N_storage.scriptsAllowed) {
        ux_display_transaction_flow[index++] = &ux_display_no_arbitrary_script_step;
        ux_display_transaction_flow[index++] = &ux_display_abort_step;
        ux_display_transaction_flow[index++] = FLOW_END_STEP;
        return;
    }

    ux_display_transaction_flow[index++] = &ux_display_review_step;

    if (G_context.tx_info.transaction.is_vote_script) {
        if (G_context.tx_info.transaction.is_remove_vote) {
            ux_display_transaction_flow[index++] = &ux_display_vote_retract_step;
        } else {
            ux_display_transaction_flow[index++] = &ux_display_vote_to_step;
        }
    } else if (G_context.tx_info.transaction.is_system_asset_transfer) {
        ux_display_transaction_flow[index++] = &ux_display_dst_address_step;
        ux_display_transaction_flow[index++] = &ux_display_token_amount_step;
    }

    ux_display_transaction_flow[index++] = &ux_display_network_step;
    ux_display_transaction_flow[index++] = &ux_display_systemfee_step;
    ux_display_transaction_flow[index++] = &ux_display_networkfee_step;
    ux_display_transaction_flow[index++] = &ux_display_total_fee;
    ux_display_transaction_flow[index++] = &ux_display_validuntilblock_step;

    // special step that won't be shown, but used for runtime displaying
    // dynamics screens when applicable
    ux_display_transaction_flow[index++] = &ux_upper_delimiter;
    // will be used to dynamically display Signers
    ux_display_transaction_flow[index++] = &ux_display_generic;
    // special step that won't be shown, but used for runtime displaying
    // dynamics screens when applicable
    ux_display_transaction_flow[index++] = &ux_lower_delimiter;

    ux_display_transaction_flow[index++] = &ux_display_approve_step;
    ux_display_transaction_flow[index++] = &ux_display_reject_step;
    ux_display_transaction_flow[index++] = FLOW_END_STEP;

#else

    static_items_nb = 0;
    dyn_items_nb = 0;

    // if (!G_context.tx_info.transaction.is_system_asset_transfer && !G_context.tx_info.transaction.is_vote_script &&
    //     !N_storage.scriptsAllowed) {
    //     ux_display_transaction_flow[index++] = &ux_display_no_arbitrary_script_step;
    //     ux_display_transaction_flow[index++] = &ux_display_abort_step;
    //     ux_display_transaction_flow[index++] = FLOW_END_STEP;
    //     return;
    // }

    if (G_context.tx_info.transaction.is_vote_script) {
        if (G_context.tx_info.transaction.is_remove_vote) {
            static_items[static_items_nb].item = "Object";
            static_items[static_items_nb].value = "Retracting vote";
            ++static_items_nb;
        } else {
            static_items[static_items_nb].item = "Object";
            static_items[static_items_nb].value = "Casting vote";
            ++static_items_nb;
        }
    } else if (G_context.tx_info.transaction.is_system_asset_transfer) {
        static_items[static_items_nb].item = "Object";
        static_items[static_items_nb].value = "System asset transfer";
        ++static_items_nb;
        static_items[static_items_nb].item = "Destination addr";
        static_items[static_items_nb].value = g_dst_address;
        ++static_items_nb;
        static_items[static_items_nb].item = "Token amount";
        static_items[static_items_nb].value = g_token_amount;
        ++static_items_nb;
    }

    static_items[static_items_nb].item = "Target network";
    static_items[static_items_nb].value = g_network;
    ++static_items_nb;

    static_items[static_items_nb].item = "System fee";
    static_items[static_items_nb].value = g_system_fee;
    ++static_items_nb;

    static_items[static_items_nb].item = "Network fee";
    static_items[static_items_nb].value = g_network_fee;
    ++static_items_nb;

    static_items[static_items_nb].item = "Total fees";
    static_items[static_items_nb].value = g_total_fees;
    ++static_items_nb;

    static_items[static_items_nb].item = "Valid until height";
    static_items[static_items_nb].value = g_valid_until_block;
    ++static_items_nb;

    for (int i = 0; i < G_context.tx_info.transaction.signers_size; ++i) {
        dyn_items[dyn_items_nb].kind = SIGNER;
        dyn_items[dyn_items_nb].content.as_item_signer.signer_index = i;
        ++dyn_items_nb;
        dyn_items[dyn_items_nb].kind = ACCOUNT;
        dyn_items[dyn_items_nb].content.as_item_account.signer_index = i;
        ++dyn_items_nb;
        dyn_items[dyn_items_nb].kind = SCOPE;
        dyn_items[dyn_items_nb].content.as_item_scope.signer_index = i;
        ++dyn_items_nb;

        signer_t signer = G_context.tx_info.transaction.signers[i];
        for (int j = 0; j < signer.allowed_contracts_size; ++j) {
            dyn_items[dyn_items_nb].kind = CONTRACT;
            dyn_items[dyn_items_nb].content.as_item_contract.signer_index = i;
            dyn_items[dyn_items_nb].content.as_item_contract.contract_index = j;
            ++dyn_items_nb;
        }
        for (int j = 0; j < signer.allowed_groups_size; ++j) {
            dyn_items[dyn_items_nb].kind = GROUP;
            dyn_items[dyn_items_nb].content.as_item_group.signer_index = i;
            dyn_items[dyn_items_nb].content.as_item_group.group_index = j;
            ++dyn_items_nb;
        }
    }

#endif
}

int ui_display_transaction() {
    if (G_context.req_type != CONFIRM_TRANSACTION || G_context.state != STATE_PARSED) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    if (G_context.tx_info.transaction.is_system_asset_transfer) {
        memset(g_dst_address, 0, sizeof(g_dst_address));
        snprintf(g_dst_address, sizeof(g_dst_address), "%s", G_context.tx_info.transaction.dst_address);
        PRINTF("Destination address: %s\n", g_dst_address);

        memset(g_token_amount, 0, sizeof(g_token_amount));
        char token_amount[30] = {0};
        if (!format_fpu64(token_amount,
                          sizeof(token_amount),
                          (uint64_t) G_context.tx_info.transaction.amount,
                          G_context.tx_info.transaction.is_neo ? 0 : 8)) {
            return io_send_sw(SW_DISPLAY_TOKEN_TRANSFER_AMOUNT_FAIL);
        }
        snprintf(g_token_amount,
                 sizeof(g_token_amount),
                 "%s %.*s",
                 G_context.tx_info.transaction.is_neo ? "NEO" : "GAS",
                 sizeof(token_amount),
                 token_amount);
    }

    if (G_context.tx_info.transaction.is_vote_script && !G_context.tx_info.transaction.is_remove_vote) {
        memset(g_vote_to, 0, sizeof(g_vote_to));
        format_hex(G_context.tx_info.transaction.vote_to, ECPOINT_LEN, g_vote_to, sizeof(g_vote_to));
    }

    // We'll try to give more user friendly names for known networks
    if (G_context.network_magic == NETWORK_MAINNET) {
        strcpy(g_network, "MainNet");
    } else if (G_context.network_magic == NETWORK_TESTNET) {
        strcpy(g_network, "TestNet");
    } else {
        snprintf(g_network, sizeof(g_network), "%d", G_context.network_magic);
    }
    PRINTF("Target network: %s\n", g_network);

    // System fee is a value multiplied by 100_000_000 to create 8 decimals stored in an int.
    // It is not allowed to be negative so we can safely cast it to uint64_t
    memset(g_system_fee, 0, sizeof(g_system_fee));
    char system_fee[30] = {0};
    if (!format_fpu64(system_fee, sizeof(system_fee), (uint64_t) G_context.tx_info.transaction.system_fee, 8)) {
        return io_send_sw(SW_DISPLAY_SYSTEM_FEE_FAIL);
    }
    snprintf(g_system_fee, sizeof(g_system_fee), "GAS %.*s", sizeof(system_fee), system_fee);
    PRINTF("System fee: %s GAS\n", system_fee);

    // Network fee is stored in a similar fashion as system fee above
    memset(g_network_fee, 0, sizeof(g_network_fee));
    char network_fee[30] = {0};
    if (!format_fpu64(network_fee, sizeof(network_fee), (uint64_t) G_context.tx_info.transaction.network_fee, 8)) {
        return io_send_sw(SW_DISPLAY_NETWORK_FEE_FAIL);
    }
    snprintf(g_network_fee, sizeof(g_network_fee), "GAS %.*s", sizeof(network_fee), network_fee);
    PRINTF("Network fee: %s GAS\n", network_fee);

    memset(g_total_fees, 0, sizeof(g_total_fees));
    char total_fee[30] = {0};
    // Note that network_fee and system_fee are actually int64 and can't be less than 0 (as guarded by
    // transaction_deserialize())
    if (!format_fpu64(total_fee,
                      sizeof(total_fee),
                      (uint64_t) G_context.tx_info.transaction.network_fee + G_context.tx_info.transaction.system_fee,
                      8)) {
        return io_send_sw(SW_DISPLAY_TOTAL_FEE_FAIL);
    }
    snprintf(g_total_fees, sizeof(g_total_fees), "GAS %.*s", sizeof(total_fee), total_fee);

    snprintf(g_valid_until_block, sizeof(g_valid_until_block), "%d", G_context.tx_info.transaction.valid_until_block);
    PRINTF("Valid until: %s\n", g_valid_until_block);

    reset_signer_display_state();

    // Prepare steps
    create_transaction_flow();

#ifdef HAVE_BAGL
    // start display
    ux_flow_init(0, ux_display_transaction_flow, NULL);
#else
    ui_sign_message_nbgl();
#endif

    return 0;
}

#ifdef HAVE_BAGL
void next_prop() {
    uint8_t *idx = &display_ctx.p_index;
    signer_t signer = G_context.tx_info.transaction.signers[display_ctx.s_index];

    if (*idx < (uint8_t) CONTRACTS) (*idx)++;

    if (signer_property[*idx] == CONTRACTS) {
        // we start at -1
        if (display_ctx.c_index + 1 < signer.allowed_contracts_size) {
            display_ctx.c_index++;
            return;  // let it display the contract
        }
        display_ctx.c_index++;
        (*idx)++;  // advance state to GROUPS
    }
    if (signer_property[*idx] == GROUPS) {
        // we start at -1
        if (display_ctx.g_index + 1 < signer.allowed_groups_size) {
            display_ctx.g_index++;
            return;  // let it display the group
        }
        display_ctx.g_index++;
        (*idx)++;  // advance state to END
    }

    // if we displayed all properties of the current signer
    if (signer_property[*idx] == END) {
        // are there more signers?
        if (display_ctx.s_index + 1 == G_context.tx_info.transaction.signers_size) {
            // no more signers
            return;
        } else {
            // more signers, advance signer index and reset some properties
            display_ctx.s_index++;
            display_ctx.c_index = -1;
            display_ctx.g_index = -1;
            *idx = (uint8_t) START;
            next_prop();
        }
    }
}

void prev_prop() {
    uint8_t *idx = &display_ctx.p_index;
    signer_t signer;

    // from first dynamic screen, go back to first static
    if (display_ctx.s_index == 0 && signer_property[*idx] == INDEX) {
        *idx = (uint8_t) START;
        return;
    }

    // from static screen below lower_delimiter screen, go to last dynamic
    if (signer_property[*idx] == END) {
        (*idx)--;  // reverse to GROUPS
    }

    if (signer_property[*idx] == GROUPS) {
        if (display_ctx.g_index > 0) {
            display_ctx.g_index--;
            return;  // let it display the group
        }
        display_ctx.g_index--;  // make sure we end up at -1 as that is what next_prop() expects
                                // when going forward
        (*idx)--;               // advance state to CONTRACTS
    }

    if (signer_property[*idx] == CONTRACTS) {
        if (display_ctx.c_index > 0) {
            display_ctx.c_index--;
            return;  // let it display the contract
        }
        display_ctx.c_index--;  // make sure we end up at -1 as that is what next_prop() expects
                                // when going forward
        // no need to reverse state to SCOPE, will be done on the next line
    }
    (*idx)--;

    // we've exhausted all properties, check if there are more signers
    if (signer_property[*idx] == START) {
        if (display_ctx.s_index > 0) {
            display_ctx.s_index--;
            signer = G_context.tx_info.transaction.signers[display_ctx.s_index];
            *idx = (uint8_t) END;  // set property index to end
            display_ctx.g_index = signer.allowed_groups_size;
            display_ctx.c_index = signer.allowed_contracts_size;
            prev_prop();
        }
    }
}

bool get_next_data(enum e_direction direction) {
    if (direction == DIRECTION_FORWARD) {
        next_prop();
    } else {
        prev_prop();
    }

    signer_t s = G_context.tx_info.transaction.signers[display_ctx.s_index];
    enum e_signer_state display = signer_property[display_ctx.p_index];

    if (display_ctx.s_index == G_context.tx_info.transaction.signers_size &&
        signer_property[display_ctx.p_index] == END) {
        return false;
    }

    switch (display) {
        case START: {
            return false;
        }
        case INDEX: {
            strcpy(g_title, "Signer");
            uint8_t signers_size = G_context.tx_info.transaction.signers_size;
            snprintf(g_text, sizeof(g_text), "%d of %d", display_ctx.s_index + 1, signers_size);
            return true;
        }
        case ACCOUNT: {
            strcpy(g_title, "Account");
            format_hex(s.account, 20, g_text, sizeof(g_text));
            return true;
        }
        case SCOPE: {
            strcpy(g_title, "Scope");
            int scope_size = parse_scope_name(s.scope);
            snprintf(g_text, sizeof(g_text), "%.*s", scope_size, g_scope);
            return true;
        }
        case CONTRACTS: {
            s = G_context.tx_info.transaction.signers[display_ctx.s_index];
            snprintf(g_title, sizeof(g_title), "Contract %d of %d", display_ctx.c_index + 1, s.allowed_contracts_size);
            format_hex(s.allowed_contracts[display_ctx.c_index], UINT160_LEN, g_text, sizeof(g_text));
            return true;
        }
        case GROUPS: {
            s = G_context.tx_info.transaction.signers[display_ctx.s_index];
            snprintf(g_title, sizeof(g_title), "Group %d of %d", display_ctx.g_index + 1, s.allowed_groups_size);
            format_hex(s.allowed_groups[display_ctx.g_index], ECPOINT_LEN, g_text, sizeof(g_text));
            return true;
        }
        case END: {
            return false;
        }
    }
}
#endif
