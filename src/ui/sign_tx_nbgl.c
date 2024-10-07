#ifdef HAVE_NBGL

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

#include "nbgl_use_case.h"

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

static nbgl_contentTagValueList_t content;
static const char *review_final_long_press_text;
static nbgl_contentTagValue_t current_pair;
static nbgl_contentTagValue_t static_items[MAX_NUM_STEPS + 1];
static dynamic_slot_t dyn_slots[NB_MAX_DISPLAYED_PAIRS_IN_REVIEW];
static uint8_t static_items_nb;
// Reserve space for preparing the dynamic items (signer) display. At the moment 42 elements max
static dynamic_item_t
    dyn_items[MAX_TX_SIGNERS * (3 + MAX_SIGNER_ALLOWED_CONTRACTS * 1 + MAX_SIGNER_ALLOWED_GROUPS * 1)];
static uint8_t dyn_items_nb;
static const char *review_title;

static void create_transaction_flow(void) {
    static_items_nb = 0;
    dyn_items_nb = 0;

    if (G_context.tx_info.transaction.is_vote_script) {
        if (G_context.tx_info.transaction.is_remove_vote) {
            review_final_long_press_text = "Sign transaction to\nretract vote?";
            review_title = "Review transaction to\nretract vote";
        } else {
            review_final_long_press_text = "Sign transaction to\ncast vote?";
            review_title = "Review transaction to\ncast vote";
        }
    } else if (G_context.tx_info.transaction.is_system_asset_transfer) {
        static_items[static_items_nb].item = "To";
        static_items[static_items_nb].value = G_tx.dst_address;
        ++static_items_nb;
        static_items[static_items_nb].item = "Token amount";
        static_items[static_items_nb].value = G_tx.token_amount;
        ++static_items_nb;

        if (G_context.tx_info.transaction.is_neo) {
            review_final_long_press_text = "Sign transaction to\nsend NEO?";
            review_title = "Review transaction to\nsend NEO";
        } else {
            review_final_long_press_text = "Sign transaction to\nsend GAS?";
            review_title = "Review transaction to\nsend GAS";
        }
    } else {
        review_final_long_press_text = "Sign script?";
        review_title = "Review transaction\nto sign script";
    }

    static_items[static_items_nb].item = "Target network";
    static_items[static_items_nb].value = G_tx.network;
    ++static_items_nb;

    static_items[static_items_nb].item = "System fee";
    static_items[static_items_nb].value = G_tx.system_fee;
    ++static_items_nb;

    static_items[static_items_nb].item = "Network fee";
    static_items[static_items_nb].value = G_tx.network_fee;
    ++static_items_nb;

    static_items[static_items_nb].item = "Total fees";
    static_items[static_items_nb].value = G_tx.total_fees;
    ++static_items_nb;

    static_items[static_items_nb].item = "Valid until height";
    static_items[static_items_nb].value = G_tx.valid_until_block;
    ++static_items_nb;

    static_items[static_items_nb].item = "Script hash";
    static_items[static_items_nb].value = G_tx.script_hash;
    ++static_items_nb;

    // dyn_items size is tailored to fit the worst case scenario
    // if the number of element is too big for the array it would have triggered a parsing error
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
}


static void review_final_callback(bool confirmed) {
    if (confirmed) {
        ui_action_validate_transaction(true, false);
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
        ui_action_validate_transaction(false, false);
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main);
    }
}


static void format_tag_value(dynamic_slot_t *slot, const dynamic_item_t *item) {
    signer_t s = G_context.tx_info.transaction.signers[item->content.as_item_scope.signer_index];
    switch (item->kind) {
        case SIGNER:
            format_signer(item->content.as_item_signer.signer_index,
                          slot->title,
                          sizeof(slot->title),
                          slot->text,
                          sizeof(slot->text));
            break;

        case ACCOUNT:
            format_account(&s, slot->title, sizeof(slot->title), slot->text, sizeof(slot->text));
            break;

        case SCOPE:
            format_scope(&s, slot->title, sizeof(slot->title), slot->text, sizeof(slot->text));
            break;

        case CONTRACT:
            format_contract(&s,
                            item->content.as_item_contract.contract_index,
                            slot->title,
                            sizeof(slot->title),
                            slot->text,
                            sizeof(slot->text));
            break;

        case GROUP:
            format_group(&s,
                         item->content.as_item_group.group_index,
                         slot->title,
                         sizeof(slot->title),
                         slot->text,
                         sizeof(slot->text));
            break;
    }
}

// function called by NBGL to get the current_pair indexed by "index"
static nbgl_contentTagValue_t *get_single_action_review_pair(uint8_t index) {
    current_pair.valueIcon = NULL;
    if (index < static_items_nb) {
        // No need to copy to dyn_slots as item and value are pointers to static values
        current_pair.item = static_items[index].item;
        current_pair.value = static_items[index].value;
    } else {
        dynamic_item_t *item = &dyn_items[index - static_items_nb];
        dynamic_slot_t *slot = &dyn_slots[index % ARRAY_COUNT(dyn_slots)];
        format_tag_value(slot, item);
        current_pair.item = slot->title;
        current_pair.value = slot->text;
    }
    return &current_pair;
}

static void arbitrary_script_rejection_callback(bool confirmed) {
    ui_action_validate_transaction(false, false);
    ui_menu_settings(confirmed);
}

void start_sign_tx_ui(void) {
    if (!G_context.tx_info.transaction.is_system_asset_transfer && !G_context.tx_info.transaction.is_vote_script &&
        !N_storage.scriptsAllowed) {
        // TODO: maybe add a mechanism to resume the transaction if the user allows the setting
        nbgl_useCaseChoice(&C_Warning_64px,
                           "Arbitrary contract\nscripts are not allowed.",
                           "Go to the Settings menu to\nenable the signing of such\ntransactions.\n\nThis "
                           "transaction\nwill be rejected.",
                           "Go to Settings menu",
                           "Go to Home screen",
                           arbitrary_script_rejection_callback);
    } else {
        // Prepare steps
        create_transaction_flow();
        // start display
        content.nbMaxLinesForValue = 0;
        content.smallCaseForValue = false;
        content.wrapping = true;
        content.pairs = NULL;  // to indicate that callback should be used
        content.callback = get_single_action_review_pair;
        content.startIndex = 0;
        content.nbPairs = static_items_nb + dyn_items_nb;

        nbgl_useCaseReview(
            TYPE_TRANSACTION,
            &content,
            &C_icon_neo_n3_64x64,
            review_title,
            NULL,
            review_final_long_press_text,
            review_final_callback);
    }
}

#endif
