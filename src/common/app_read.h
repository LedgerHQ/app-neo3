#pragma once

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t
#include "read.h"

/**
 * Read 2 bytes as Big Endian from byte buffer.
 *
 * @param[in] ptr
 *   Pointer to byte buffer.
 * @param[in] offset
 *   Offset in the byte buffer.
 *
 * @return 2 bytes value read from buffer.
 *
 */
int16_t read_s16_be(const uint8_t *ptr, size_t offset);

/**
 * Read 4 bytes as Big Endian from byte buffer.
 *
 * @param[in] ptr
 *   Pointer to byte buffer.
 * @param[in] offset
 *   Offset in the byte buffer.
 *
 * @return 4 bytes value read from buffer.
 *
 */
int32_t read_s32_be(const uint8_t *ptr, size_t offset);

/**
 * Read 8 bytes as Big Endian from byte buffer.
 *
 * @param[in] ptr
 *   Pointer to byte buffer.
 * @param[in] offset
 *   Offset in the byte buffer.
 *
 * @return 8 bytes value read from buffer.
 *
 */
int64_t read_s64_be(const uint8_t *ptr, size_t offset);

/**
 * Read 2 bytes as Little Endian from byte buffer.
 *
 * @param[in] ptr
 *   Pointer to byte buffer.
 * @param[in] offset
 *   Offset in the byte buffer.
 *
 * @return 2 bytes value read from buffer.
 *
 */
int16_t read_s16_le(const uint8_t *ptr, size_t offset);

/**
 * Read 4 bytes as Little Endian from byte buffer.
 *
 * @param[in] ptr
 *   Pointer to byte buffer.
 * @param[in] offset
 *   Offset in the byte buffer.
 *
 * @return 4 bytes value read from buffer.
 *
 */
int32_t read_s32_le(const uint8_t *ptr, size_t offset);

/**
 * Read 8 bytes as Little Endian from byte buffer.
 *
 * @param[in] ptr
 *   Pointer to byte buffer.
 * @param[in] offset
 *   Offset in the byte buffer.
 *
 * @return 8 bytes value read from buffer.
 *
 */
int64_t read_s64_le(const uint8_t *ptr, size_t offset);
