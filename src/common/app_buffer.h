#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include "buffer.h"

/**
 * Read 1 byte from buffer into int8_t.
 *
 * @param[in,out]  buffer
 *   Pointer to input buffer struct.
 * @param[out]     value
 *   Pointer to 8-bit signed integer read from buffer.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_s8(buffer_t *buffer, int8_t *value);

/**
 * Read 2 bytes from buffer into int16_t.
 *
 * @param[in,out]  buffer
 *   Pointer to input buffer struct.
 * @param[out]     value
 *   Pointer to 16-bit signed integer read from buffer.
 * @param[in]      endianness
 *   Either BE (Big Endian) or LE (Little Endian).
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_s16(buffer_t *buffer, int16_t *value, endianness_t endianness);

/**
 * Read 4 bytes from buffer into int32_t.
 *
 * @param[in,out]  buffer
 *   Pointer to input buffer struct.
 * @param[out]     value
 *   Pointer to 32-bit signed integer read from buffer.
 * @param[in]      endianness
 *   Either BE (Big Endian) or LE (Little Endian).
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_s32(buffer_t *buffer, int32_t *value, endianness_t endianness);

/**
 * Read 8 bytes from buffer into int64_t.
 *
 * @param[in,out]  buffer
 *   Pointer to input buffer struct.
 * @param[out]     value
 *   Pointer to 64-bit signed integer read from buffer.
 * @param[in]      endianness
 *   Either BE (Big Endian) or LE (Little Endian).
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_s64(buffer_t *buffer, int64_t *value, endianness_t endianness);

/**
 * Read Bitcoin-like varint from buffer into uint64_t.
 *
 * @see https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
 *
 * @param[in,out]  buffer
 *   Pointer to input buffer struct.
 * @param[out]     value
 *   Pointer to 64-bit unsigned integer read from buffer.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_uvarint(buffer_t *buffer, uint64_t *value);

