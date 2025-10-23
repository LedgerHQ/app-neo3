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

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t
#include "app_read.h"

int16_t read_s16_be(const uint8_t *ptr, size_t offset) {
    return (int16_t) read_u16_be(ptr, offset);
}

int32_t read_s32_be(const uint8_t *ptr, size_t offset) {
    return (int32_t) read_u32_be(ptr, offset);
}

int64_t read_s64_be(const uint8_t *ptr, size_t offset) {
    return (int64_t) read_u64_be(ptr, offset);
}

int16_t read_s16_le(const uint8_t *ptr, size_t offset) {
    return (int64_t) read_u16_le(ptr, offset);
}

int32_t read_s32_le(const uint8_t *ptr, size_t offset) {
    return (int32_t) read_u32_le(ptr, offset);
}

int64_t read_s64_le(const uint8_t *ptr, size_t offset) {
    return (int64_t) read_u64_le(ptr, offset);
}
