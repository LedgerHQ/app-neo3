#ifndef APP_NEO3_SHARED_CONTEXT_H
#define APP_NEO3_SHARED_CONTEXT_H

#include "os.h"

#define N_storage (*(volatile internalStorage_t *) PIC(&N_storage_real))

typedef struct internalStorage_t {
    unsigned char scriptsAllowed;
    unsigned char showScriptHash;
    unsigned char signerAccountFormat;
    uint8_t initialized;
} internalStorage_t;

// "Not allowed" + \0
#define SETTING_SCRIPT_ALLOWED_SIZE 12
// "Hide" + \0  (both options have the same size)
#define SETTING_SCRIPT_HASH_SIZE 6
// "Script hash" + \0 (both options have the same size)
#define SETTING_SCRIPT_ALLOWED_SIZE 12

typedef struct settings_strings_t {
    char scriptsAllowed[SETTING_SCRIPT_ALLOWED_SIZE];       // Allowed / Not Allowed
    char showScriptHash[SETTING_SCRIPT_HASH_SIZE];          // Show / Hide
    char signerAccountFormat[SETTING_SCRIPT_ALLOWED_SIZE];  // Script hash / NEO address
} settings_strings_t;

extern settings_strings_t strings;
extern const internalStorage_t N_storage_real;

#endif  // APP_NEO3_SHARED_CONTEXT_H
