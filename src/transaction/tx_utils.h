#ifndef APP_NEO3_TX_UTILS_H
#define APP_NEO3_TX_UTILS_H

#include "app_buffer.h"
#include "types.h"

void try_parse_transfer_script(buffer_t *script, transaction_t *tx);

void try_parse_vote_script(buffer_t *script, transaction_t *tx);
#endif
