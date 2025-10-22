#pragma once

#include <stddef.h>   // size_t
#include <stdint.h>   // int*_t, uint*_t
#include <stdbool.h>  // bool

#include "format.h"

/**
 * Format 64-bit unsigned integer as string with decimals.
 *
 * @param[out] dst
 *   Pointer to output string.
 * @param[in]  dst_len
 *   Length of output string.
 * @param[in]  value
 *   64-bit unsigned integer to format.
 * @param[in]  decimals
 *   Number of digits after decimal separator.
 *
 * @return true if success, false otherwise.
 *
 */
bool format_amount(char *dst, size_t dst_len, const uint64_t value, uint8_t decimals);
