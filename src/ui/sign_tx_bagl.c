#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "ux.h"
#include "glyphs.h"

#include "constants.h"
#include "globals.h"
#include "io.h"
#include "sw.h"
#include "action/validate.h"
#include "transaction/transaction_types.h"
#include "common/format.h"
#include "utils.h"
#include "menu.h"
#include "shared_context.h"
#include "sign_tx_common.h"

#ifdef HAVE_BAGL

/**
 * State of the dynamic display flow.
 * Use to keep track of whether we are displaying screens that are inside the
 * UX_FLOW array (dynamic), or outside the array (static).
 */
enum e_state {
    STATIC_SCREEN,
    DYNAMIC_SCREEN,
};

/**
 * Hold state around displaying Signers and their properties
 */
typedef struct display_ctx_s {
    enum e_state current_state;  // screen state
    int8_t s_index;              // track which signer is to be displayed
    uint8_t p_index;             // track which signer property is displayed (see also: e_signer_state)
    int8_t c_index;              // track which signer.contract is to be displayed
    int8_t g_index;              // track which signer.group is to be displayed
} display_ctx_t;

static display_ctx_t display_ctx;

static void reset_signer_display_state() {
    display_ctx.current_state = STATIC_SCREEN;
    display_ctx.s_index = 0;
    display_ctx.g_index = -1;
    display_ctx.c_index = -1;
    display_ctx.p_index = 0;
}

/**
 * Hold current dynamic content around displaying Signers and their properties
 */
static char g_title[64];
static char g_text[64];

enum e_direction { DIRECTION_FORWARD, DIRECTION_BACKWARD };

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

static void next_prop(void) {
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

static void prev_prop(void) {
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

static bool get_next_data(enum e_direction direction) {
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
            format_signer(display_ctx.s_index, g_title, sizeof(g_title), g_text, sizeof(g_text));
            return true;
        }
        case ACCOUNT: {
            format_account(&s, g_title, sizeof(g_title), g_text, sizeof(g_text));
            return true;
        }
        case SCOPE: {
            format_scope(&s, g_title, sizeof(g_title), g_text, sizeof(g_text));
            return true;
        }
        case CONTRACTS: {
            s = G_context.tx_info.transaction.signers[display_ctx.s_index];
            format_contract(&s, display_ctx.c_index, g_title, sizeof(g_title), g_text, sizeof(g_text));
            return true;
        }
        case GROUPS: {
            s = G_context.tx_info.transaction.signers[display_ctx.s_index];
            format_group(&s, display_ctx.g_index, g_title, sizeof(g_title), g_text, sizeof(g_text));
            return true;
        }
        case END: {
            return false;
        }
    }
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
                 .text = G_tx.dst_address,
             });

UX_STEP_NOCB(ux_display_token_amount_step,
             bnnn_paging,
             {
                 .title = "Token amount",
                 .text = G_tx.token_amount,
             });

UX_STEP_NOCB(ux_display_systemfee_step,
             bnnn_paging,
             {
                 .title = "System fee",
                 .text = G_tx.system_fee,
             });

UX_STEP_NOCB(ux_display_network_step,
             bnnn_paging,
             {
                 .title = "Target network",
                 .text = G_tx.network,
             });

UX_STEP_NOCB(ux_display_networkfee_step,
             bnnn_paging,
             {
                 .title = "Network fee",
                 .text = G_tx.network_fee,
             });

UX_STEP_NOCB(ux_display_total_fee,
             bnnn_paging,
             {
                 .title = "Total fees",
                 .text = G_tx.total_fees,
             });

UX_STEP_NOCB(ux_display_validuntilblock_step,
             bnnn_paging,
             {
                 .title = "Valid until height",
                 .text = G_tx.valid_until_block,
             });

#if !defined(TARGET_NANOS)
UX_STEP_NOCB(ux_display_script_step,
             bnnn_paging,
             {
                .title = "Script hash",
                .text = G_tx.script_hash,
             }
);
#endif

UX_STEP_NOCB(
    ux_display_no_arbitrary_script_step,
    bnnn_paging,
    {
        .title = "Error",
        .text = "Arbitrary contract scripts are not allowed. Go to Settings to enable signing of such transactions",
    });

UX_STEP_CB(ux_display_abort_step,
           pb,
           ui_action_validate_transaction(false, true),
           {
               &C_icon_validate_14,
               "Understood, abort..",
           });

UX_STEP_NOCB(ux_display_vote_to_step,
             bnnn_paging,
             {
                 .title = "Casting vote for",
                 .text = G_tx.vote_to,
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
           ui_action_validate_transaction(true, true),
           {
               &C_icon_validate_14,
               "Approve",
           });

// Step with reject button
UX_STEP_CB(ux_display_reject_step,
           pb,
           ui_action_validate_transaction(false, true),
           {
               &C_icon_crossmark,
               "Reject",
           });

static void create_transaction_flow(void) {
    uint8_t index = 0;

    reset_signer_display_state();

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
    #if !defined(TARGET_NANOS)
    if (N_storage.showScriptHash) {
        ux_display_transaction_flow[index++] = &ux_display_script_step;
    }
    #endif

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
}

void start_sign_tx_ui(void) {
    // Prepare steps
    create_transaction_flow();
    // start display
    ux_flow_init(0, ux_display_transaction_flow, NULL);
}

#endif
