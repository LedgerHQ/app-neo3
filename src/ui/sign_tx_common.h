#pragma once

#include "types.h"

// number of steps in create_transaction_flow() for BAGL
#define MAX_NUM_STEPS 14

// uint32 (=max 10 chars) + \0
#define UINT32_STRING_SIZE 11

// Target network the tx in tended for ("MainNet", "TestNet" or uint32 network number for private nets)
#define NETWORK_NAME_MAX_SIZE 11

// ticker + uint64 (=max 20 chars) + \0
#define AMOUNTS_MAX_SIZE 30

// 33 bytes public key as hex + \0
#define VOTE_TO_SIZE (ECPOINT_LEN * 2 + 1)

#define SHA256_SIZE (32 * 2 + 1)

typedef struct global_item_storage_s {
    char dst_address[ADDRESS_LEN + 1];
    char system_fee[AMOUNTS_MAX_SIZE];
    char network_fee[AMOUNTS_MAX_SIZE];
    char total_fees[AMOUNTS_MAX_SIZE];
    char token_amount[AMOUNTS_MAX_SIZE];
    char vote_to[VOTE_TO_SIZE];
    char network[NETWORK_NAME_MAX_SIZE];
    char valid_until_block[UINT32_STRING_SIZE];
    char script_hash[SHA256_SIZE];
} global_item_storage_t;

extern global_item_storage_t G_tx;

void format_signer(uint8_t signer_idx,
                   char *dest_title,
                   size_t dest_title_size,
                   char *dest_text,
                   size_t dest_text_size);

void format_account(const signer_t *s,
                    char *dest_title,
                    size_t dest_title_size,
                    char *dest_text,
                    size_t dest_text_size);

void format_scope(const signer_t *s, char *dest_title, size_t dest_title_size, char *dest_text, size_t dest_text_size);

void format_contract(const signer_t *s,
                     uint8_t contract_index,
                     char *dest_title,
                     size_t dest_title_size,
                     char *dest_text,
                     size_t dest_text_size);

void format_group(const signer_t *s,
                  uint8_t group_index,
                  char *dest_title,
                  size_t dest_title_size,
                  char *dest_text,
                  size_t dest_text_size);

int start_sign_tx(void);

void start_sign_tx_ui(void);
