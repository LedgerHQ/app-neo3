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

global_item_storage_t G_tx;

void format_signer(uint8_t signer_idx,
                   char *dest_title,
                   size_t dest_title_size,
                   char *dest_text,
                   size_t dest_text_size) {
    strlcpy(dest_title, "Signer", dest_title_size);
    snprintf(dest_text, dest_text_size, "%d of %d", signer_idx + 1, G_context.tx_info.transaction.signers_size);
}

void format_account(const signer_t *s,
                    char *dest_title,
                    size_t dest_title_size,
                    char *dest_text,
                    size_t dest_text_size) {
    strlcpy(dest_title, "Account", dest_title_size);
    format_hex(s->account, 20, dest_text, dest_text_size);
}

static void strlcat_with_comma(char *dest, const char *text, size_t dest_size, bool *is_first) {
    if (!*is_first) {
        strlcat(dest, ",", dest_size);
    }
    strlcat(dest, text, dest_size);
    *is_first = false;
}

void format_scope(const signer_t *s, char *dest_title, size_t dest_title_size, char *dest_text, size_t dest_text_size) {
    strlcpy(dest_title, "Scope", dest_title_size);
    if (s->scope == NONE) {
        strlcpy(dest_text, "None", dest_text_size);
    } else if (s->scope == GLOBAL) {
        strlcpy(dest_text, "Global", dest_text_size);
    } else {
        bool is_first = true;
        strlcpy(dest_text, "By ", dest_text_size);
        if (s->scope & CALLED_BY_ENTRY) {
            strlcat_with_comma(dest_text, "Entry", dest_text_size, &is_first);
        }
        if (s->scope & CUSTOM_CONTRACTS) {
            strlcat_with_comma(dest_text, "Contracts", dest_text_size, &is_first);
        }
        if (s->scope & CUSTOM_GROUPS) {
            strlcat_with_comma(dest_text, "Groups", dest_text_size, &is_first);
        }
    }
}

void format_contract(const signer_t *s,
                     uint8_t contract_index,
                     char *dest_title,
                     size_t dest_title_size,
                     char *dest_text,
                     size_t dest_text_size) {
    snprintf(dest_title, dest_title_size, "Contract %d of %d", contract_index + 1, s->allowed_contracts_size);
    format_hex(s->allowed_contracts[contract_index], UINT160_LEN, dest_text, dest_text_size);
}

void format_group(const signer_t *s,
                  uint8_t group_index,
                  char *dest_title,
                  size_t dest_title_size,
                  char *dest_text,
                  size_t dest_text_size) {
    snprintf(dest_title, dest_title_size, "Group %d of %d", group_index + 1, s->allowed_groups_size);
    format_hex(s->allowed_groups[group_index], ECPOINT_LEN, dest_text, dest_text_size);
}

int start_sign_tx(void) {
    if (G_context.tx_info.transaction.is_system_asset_transfer) {
        memset(G_tx.dst_address, 0, sizeof(G_tx.dst_address));
        snprintf(G_tx.dst_address, sizeof(G_tx.dst_address), "%s", G_context.tx_info.transaction.dst_address);
        PRINTF("Destination address: %s\n", G_tx.dst_address);

        memset(G_tx.token_amount, 0, sizeof(G_tx.token_amount));
        char token_amount[sizeof(G_tx.token_amount)] = {0};
        if (!format_fpu64(token_amount,
                          sizeof(token_amount),
                          (uint64_t) G_context.tx_info.transaction.amount,
                          G_context.tx_info.transaction.is_neo ? 0 : 8)) {
            return io_send_sw(SW_DISPLAY_TOKEN_TRANSFER_AMOUNT_FAIL);
        }
        snprintf(G_tx.token_amount,
                 sizeof(G_tx.token_amount),
                 "%s %.*s",
                 G_context.tx_info.transaction.is_neo ? "NEO" : "GAS",
                 sizeof(token_amount),
                 token_amount);
    }

    if (G_context.tx_info.transaction.is_vote_script && !G_context.tx_info.transaction.is_remove_vote) {
        memset(G_tx.vote_to, 0, sizeof(G_tx.vote_to));
        format_hex(G_context.tx_info.transaction.vote_to, ECPOINT_LEN, G_tx.vote_to, sizeof(G_tx.vote_to));
    }

    // We'll try to give more user friendly names for known networks
    if (G_context.network_magic == NETWORK_MAINNET) {
        strcpy(G_tx.network, "MainNet");
    } else if (G_context.network_magic == NETWORK_TESTNET) {
        strcpy(G_tx.network, "TestNet");
    } else {
        snprintf(G_tx.network, sizeof(G_tx.network), "%d", G_context.network_magic);
    }
    PRINTF("Target network: %s\n", G_tx.network);

    // System fee is a value multiplied by 100_000_000 to create 8 decimals stored in an int.
    // It is not allowed to be negative so we can safely cast it to uint64_t
    memset(G_tx.system_fee, 0, sizeof(G_tx.system_fee));
    char system_fee[sizeof(G_tx.system_fee)] = {0};
    if (!format_fpu64(system_fee, sizeof(system_fee), (uint64_t) G_context.tx_info.transaction.system_fee, 8)) {
        return io_send_sw(SW_DISPLAY_SYSTEM_FEE_FAIL);
    }
    snprintf(G_tx.system_fee, sizeof(G_tx.system_fee), "GAS %.*s", sizeof(system_fee), system_fee);
    PRINTF("System fee: %s GAS\n", system_fee);

    // Network fee is stored in a similar fashion as system fee above
    memset(G_tx.network_fee, 0, sizeof(G_tx.network_fee));
    char network_fee[sizeof(G_tx.network_fee)] = {0};
    if (!format_fpu64(network_fee, sizeof(network_fee), (uint64_t) G_context.tx_info.transaction.network_fee, 8)) {
        return io_send_sw(SW_DISPLAY_NETWORK_FEE_FAIL);
    }
    snprintf(G_tx.network_fee, sizeof(G_tx.network_fee), "GAS %.*s", sizeof(network_fee), network_fee);
    PRINTF("Network fee: %s GAS\n", network_fee);

    memset(G_tx.total_fees, 0, sizeof(G_tx.total_fees));
    char total_fee[sizeof(G_tx.total_fees)] = {0};
    // Note that network_fee and system_fee are actually int64 and can't be less than 0 (as guarded by
    // transaction_deserialize())
    if (!format_fpu64(total_fee,
                      sizeof(total_fee),
                      (uint64_t) G_context.tx_info.transaction.network_fee + G_context.tx_info.transaction.system_fee,
                      8)) {
        return io_send_sw(SW_DISPLAY_TOTAL_FEE_FAIL);
    }
    snprintf(G_tx.total_fees, sizeof(G_tx.total_fees), "GAS %.*s", sizeof(total_fee), total_fee);

    snprintf(G_tx.valid_until_block,
             sizeof(G_tx.valid_until_block),
             "%d",
             G_context.tx_info.transaction.valid_until_block);
    PRINTF("Valid until: %s\n", G_tx.valid_until_block);

    #if !defined(TARGET_NANOS)
    memset(G_tx.script_hash, 0, sizeof(G_tx.script_hash));
    if(format_hex(G_context.tx_info.script_hash, 32, G_tx.script_hash, sizeof(G_tx.script_hash)) == -1) {
        return io_send_sw(SW_DISPLAY_SCRIPT_HASH_FAIL);
    }
    PRINTF("Script hash: %s\n", G_tx.script_hash);
    #endif
    start_sign_tx_ui();

    return 0;
}
