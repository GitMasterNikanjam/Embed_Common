/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 *   AP_Common.cpp - common utility functions (standalone, no AP_HAL)
 */

#include "Embed_Common.h"

#include <cstdint>
#include <cstring>   // memcpy
#include <cstdlib>   // realloc, free

// Ensure we are compiling with 32-bit float (expected on embedded/desktop)
static_assert(sizeof(float) == 4, "AP_Common expects 32-bit float");

// Local safe strnlen fallback (avoids POSIX dependency on some toolchains)
static size_t apc_strnlen(const char *s, size_t maxlen)
{
    if (!s) return 0;
    size_t i = 0;
    for (; i < maxlen && s[i] != '\0'; ++i) { /* scan */ }
    return i;
}

/*
  Return true if value is between lower and upper bound inclusive.
  False otherwise.
*/
bool is_bounded_int32(int32_t value, int32_t lower_bound, int32_t upper_bound)
{
    if ((lower_bound <= upper_bound) &&
        (value >= lower_bound) && (value <= upper_bound)) {
        return true;
    }
    return false;
}

/**
 * return the numeric value of an ascii hex character
 *
 * @param [in]  a   Hex character (0-9, A-F, a-f)
 * @param [out] res uint8 value
 * @retval true  Conversion OK
 * @retval false Input value error
 */
bool hex_to_uint8(uint8_t a, uint8_t &res)
{
    uint8_t nibble_low  = a & 0xf;

    switch (a & 0xf0) {
    case 0x30:  // '0'..'9'
        if (nibble_low > 9) {
            return false;
        }
        res = nibble_low;
        break;
    case 0x40:  // 'A'..'F'
    case 0x60:  // 'a'..'f'
        if (nibble_low == 0 || nibble_low > 6) {
            return false;
        }
        res = nibble_low + 9;
        break;
    default:
        return false;
    }
    return true;
}

/*
  strncpy without the warning for not leaving room for nul termination
  Returns the source length (not including any terminator), like POSIX strncpy behavior.
 */
size_t strncpy_noterm(char *dest, const char *src, size_t n)
{
    const size_t len = apc_strnlen(src, n);
    size_t to_copy = len;
    // include terminating NUL if there is room
    if (to_copy < n) {
        to_copy++; // copy NUL too
    }
    if (to_copy > 0) {
        std::memcpy(dest, src, to_copy);
    }
    return len;
}

/**
 * return the numeric value of an ascii hex character
 *
 * @param[in] a Hexadecimal character
 * @return binary value in [0,15]; if invalid returns 255
 */
uint8_t char_to_hex(char a)
{
    if (a >= 'A' && a <= 'F') {
        return static_cast<uint8_t>(a - 'A' + 10);
    } else if (a >= 'a' && a <= 'f') {
        return static_cast<uint8_t>(a - 'a' + 10);
    } else if (a >= '0' && a <= '9') {
        return static_cast<uint8_t>(a - '0');
    }
    return 255;
}

/**
 * Default memory reallocator used when no platform override is provided.
 * This satisfies the weak declaration in AP_Common.h for standalone builds.
 *
 * Semantics:
 *  - If new_size == 0: frees ptr and returns nullptr.
 *  - If ptr == nullptr: behaves like malloc(new_size).
 *  - Else: behaves like realloc(ptr, new_size).
 */
void * mem_realloc(void *ptr, size_t old_size, size_t new_size)
{
    (void)old_size; // not required by the default implementation
    if (new_size == 0) {
        std::free(ptr);
        return nullptr;
    }
    void *np = std::realloc(ptr, new_size);
    // If realloc fails, the original block remains valid; return nullptr.
    return np;
}
