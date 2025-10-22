/*****************************************************************************
 *   (c) 2020 Ledger SAS.
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

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include <string.h>   // memcpy

#include "app_buffer.h"
#include "app_read.h"
#include "varint.h"

bool buffer_read_s8(buffer_t *buffer, int8_t *value) {
    if (!buffer_can_read(buffer, 1)) {
        *value = 0;

        return false;
    }

    *value = (int8_t) buffer->ptr[buffer->offset];
    buffer_seek_cur(buffer, 1);

    return true;
}

bool buffer_read_s16(buffer_t *buffer, int16_t *value, endianness_t endianness) {
    if (!buffer_can_read(buffer, 2)) {
        *value = 0;

        return false;
    }

    *value = ((endianness == BE) ? read_s16_be(buffer->ptr, buffer->offset) : read_s16_le(buffer->ptr, buffer->offset));

    buffer_seek_cur(buffer, 2);

    return true;
}

bool buffer_read_s32(buffer_t *buffer, int32_t *value, endianness_t endianness) {
    if (!buffer_can_read(buffer, 4)) {
        *value = 0;

        return false;
    }

    *value = ((endianness == BE) ? read_s32_be(buffer->ptr, buffer->offset) : read_s32_le(buffer->ptr, buffer->offset));

    buffer_seek_cur(buffer, 4);

    return true;
}

bool buffer_read_s64(buffer_t *buffer, int64_t *value, endianness_t endianness) {
    if (!buffer_can_read(buffer, 8)) {
        *value = 0;

        return false;
    }

    *value = ((endianness == BE) ? read_s64_be(buffer->ptr, buffer->offset) : read_s64_le(buffer->ptr, buffer->offset));

    buffer_seek_cur(buffer, 8);

    return true;
}

bool buffer_read_uvarint(buffer_t *buffer, uint64_t *value) {
    int length = varint_read(buffer->ptr + buffer->offset, buffer->size - buffer->offset, value);

    if (length < 0) {
        *value = 0;

        return false;
    }

    buffer_seek_cur(buffer, (size_t) length);

    return true;
}
