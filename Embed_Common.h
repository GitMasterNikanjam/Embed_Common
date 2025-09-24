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
/**
 * @file      Embed_Common.h
 * @brief     Common portability attributes, small utilities, and helpers used across the library.
 *
 * This header centralizes frequently used attributes (e.g., @ref PACKED, @ref WEAK),
 * compile-time checks, formatting annotations, and a handful of tiny helpers
 * (bounds checks, hex conversion, safe strncpy, etc.). It is dependency-free and
 * suitable for bare-metal and hosted builds alike.
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <new>

/**
 * @def PACKED
 * @brief Pack a structure (no implicit padding). GCC/Clang attribute.
 *
 * Use on POD structs that must match an external binary layout (wire formats,
 * flash records, etc.). Example:
 * @code
 * struct PACKED Record { uint8_t id; uint16_t len; };
 * @endcode
 */
#define PACKED __attribute__((__packed__))

#if !defined(CYGWIN_BUILD)
/**
 * @def WEAK
 * @brief Mark a symbol as weak (may be overridden at link time).
 *
 * Useful for providing default hooks that platforms can override.
 */
#define WEAK __attribute__((__weak__))
#else
// cygwin cannot properly support weak symbols allegedly due to Windows
// executable format limitations
// (see https://www.cygwin.com/faq.html#faq.programming.linker ). fortunately
// we only use weak symbols for tests and some HAL stuff which SITL and
// therefore cygwin does not need to override. in the event that overriding is
// attempted the link will fail with a symbol redefinition error hopefully
// suggesting that an alternate approach is needed.
#define WEAK
#endif

/**
 * @def UNUSED_FUNCTION
 * @brief Mark a function as possibly unused (suppresses -Wunused-function).
 */
#define UNUSED_FUNCTION __attribute__((unused))

/**
 * @def UNUSED_PRIVATE_MEMBER
 * @brief Mark a private member as possibly unused (Clang only; no-op otherwise).
 */
#ifdef __clang__
#define UNUSED_PRIVATE_MEMBER __attribute__((unused))
#else
#define UNUSED_PRIVATE_MEMBER
#endif

/**
 * @def OPTIMIZE
 * @brief Per-function optimization level override (GCC/Clang).
 * @param level A string such as "O0", "O2", etc.
 */
#define OPTIMIZE(level) __attribute__((optimize(level)))

/**
 * @def NOINLINE
 * @brief Force a function to not be inlined (to reduce stack bloat, etc.).
 */
#ifndef NOINLINE
#define NOINLINE __attribute__((noinline))
#endif

/**
 * @def IGNORE_RETURN
 * @brief Explicitly ignore a function result (useful with warn_unused_result).
 * @param x expression whose return value should be ignored.
 */
#define IGNORE_RETURN(x) do {if (x) {}} while(0)

/**
 * @def FMT_PRINTF
 * @brief Apply printf-style format checking to a function.
 * @param a 1-based index of the format string parameter.
 * @param b 1-based index of the first variadic argument.
 */
#define FMT_PRINTF(a,b) __attribute__((format(printf, a, b)))

/**
 * @def FMT_SCANF
 * @brief Apply scanf-style format checking to a function.
 * @param a 1-based index of the format string parameter.
 * @param b 1-based index of the first variadic argument.
 */
#define FMT_SCANF(a,b) __attribute__((format(scanf, a, b)))

/**
 * @def CLASS_NO_COPY
 * @brief Disable copy construction and copy assignment for a class.
 * @param c Class name.
 */
#define CLASS_NO_COPY(c) c(const c &other) = delete; c &operator=(const c&) = delete

#ifdef __has_cpp_attribute
#  if __has_cpp_attribute(fallthrough)
/**
 * @def FALLTHROUGH
 * @brief Portable [[fallthrough]] attribute for switch statements.
 */ 
#    define FALLTHROUGH [[fallthrough]]
#  elif __has_cpp_attribute(gnu::fallthrough)
#    define FALLTHROUGH [[gnu::fallthrough]]
#  endif
#endif
#ifndef FALLTHROUGH
#  define FALLTHROUGH
#endif

#ifdef __GNUC__
/**
 * @def WARN_IF_UNUSED
 * @brief Mark a function's return value as significant; warn if ignored.
 */
 #define WARN_IF_UNUSED __attribute__ ((warn_unused_result))
#else
 #define WARN_IF_UNUSED
#endif

/**
 * @def NORETURN
 * @brief Mark a function as not returning (e.g., panic/abort).
 */
#define NORETURN __attribute__ ((noreturn))

/**
 * @def DEFINE_BYTE_ARRAY_METHODS
 * @brief Inject [] access treating the object as a byte array.
 *
 * Adds const and non-const operator[] that reinterpret the object as bytes.
 * Use only on trivially copyable types.
 */
#define DEFINE_BYTE_ARRAY_METHODS                                                                   \
    inline uint8_t &operator[](size_t i) { return reinterpret_cast<uint8_t *>(this)[i]; }           \
    inline uint8_t operator[](size_t i) const { return reinterpret_cast<const uint8_t *>(this)[i]; }

/**
 * @def BIT_IS_SET
 * @brief Test whether bit @p bitnumber is set in @p value (0-based).
 */
#define BIT_IS_SET(value, bitnumber) (((value) & (1U<<(bitnumber))) != 0)

/**
 * @def BIT_IS_SET_64
 * @brief 64-bit variant of @ref BIT_IS_SET.
 */
#define BIT_IS_SET_64(value, bitnumber) (((value) & (uint64_t(1U)<<(bitnumber))) != 0)

/**
 * @def LOWBYTE
 * @brief Extract low byte from a 16-bit integer.
 */
#define LOWBYTE(i) ((uint8_t)(i))

/**
 * @def HIGHBYTE
 * @brief Extract high byte from a 16-bit integer.
 */
#define HIGHBYTE(i) ((uint8_t)(((uint16_t)(i))>>8))

/**
 * @def ARRAY_SIZE
 * @brief Return number of elements in a C array.
 */
#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(_arr[0]))

/**
 * @def UINT16_VALUE
 * @brief Compose a 16-bit value from two bytes.
 * @param hbyte High byte.
 * @param lbyte Low byte.
 */
#define UINT16_VALUE(hbyte, lbyte) (static_cast<uint16_t>(((hbyte)<<8)|(lbyte)))

/**
 * @def UINT32_VALUE
 * @brief Compose a 32-bit value from four bytes (b3 is MSB).
 */
#define UINT32_VALUE(b3, b2, b1, b0) (static_cast<uint32_t>(((b3)<<24)|((b2)<<16)|((b1)<<8)|(b0)))

/**
 * @def _UNUSED_RESULT
 * @brief Internal: evaluate an expression annotated with warn_unused_result without a warning.
 * @param uniq_ Unique token used as a local variable name.
 * @param expr_ Expression to evaluate and ignore.
 */
#define _UNUSED_RESULT(uniq_, expr_)                      \
    do {                                                  \
        decltype(expr_) uniq_ __attribute__((unused));    \
        uniq_ = expr_;                                    \
    } while (0)

/**
 * @def UNUSED_RESULT
 * @brief Evaluate an expression annotated with warn_unused_result and ignore its value.
 * @param expr_ Expression to evaluate and ignore.
 */
#define UNUSED_RESULT(expr_) _UNUSED_RESULT(__unique_name_##__COUNTER__, expr_)

// @}

/**
 * @def STR_VALUE
 * @brief Convert a macro token to a string literal.
 *
 * @code
 * printf("%s", STR_VALUE(EINVAL)); // prints "EINVAL"
 * @endcode
 */
#define STR_VALUE(x) #x

/**
 * @brief Assert at compile time that a type has an exact storage size.
 *
 * Use @ref ASSERT_STORAGE_SIZE for convenience; the templates ensure the
 * diagnostic contains the expected vs. actual size.
 *
 * @tparam s      The type being checked.
 * @tparam s_size Size deduced as template arg (impl detail).
 * @tparam t      Expected size in bytes.
 */
template<typename s, size_t s_size, size_t t> struct _assert_storage_size {
    static_assert(s_size == t, "wrong size");
};

/**
 * @brief Wrapper that instantiates @_assert_storage_size with sizeof(s).
 * @tparam s Type to check.
 * @tparam t Expected size in bytes.
 */
template<typename s, size_t t> struct assert_storage_size {
    _assert_storage_size<s, sizeof(s), t> _member;
};

#define ASSERT_STORAGE_SIZE_JOIN( name, line ) ASSERT_STORAGE_SIZE_DO_JOIN( name, line )
#define ASSERT_STORAGE_SIZE_DO_JOIN( name, line )  name ## line

/**
 * @def ASSERT_STORAGE_SIZE
 * @brief Compile-time assertion that @p structure has size @p size.
 *
 * @code
 * ASSERT_STORAGE_SIZE(MyPackedStruct, 12);
 * @endcode
 */
#define ASSERT_STORAGE_SIZE(structure, size) \
    static_assert(sizeof(structure) == (size), "ASSERT_STORAGE_SIZE failed for " #structure)

////////////////////////////////////////////////////////////////////////////////
/**
 * @name Conversions & small utilities
 * @brief Lightweight helpers used widely across the codebase.
 * @{
 */

/**
 * @brief Check if an int32 value lies within [lower_bound, upper_bound].
 * @param value        Value to test.
 * @param lower_bound  Inclusive lower bound.
 * @param upper_bound  Inclusive upper bound.
 * @return true if @p value is within range; false otherwise.
 */
bool is_bounded_int32(int32_t value, int32_t lower_bound, int32_t upper_bound);

/**
 * @brief Convert an ASCII hex character to its 0..15 numeric value.
 * @param a   Hex digit ('0'..'9','A'..'F','a'..'f').
 * @param res Output nibble (0..15) on success.
 * @return true on success; false if @p a is not a hex digit.
 */
bool hex_to_uint8(uint8_t a, uint8_t &res);  // return the uint8 value of an ascii hex character

/**
 * @brief strncpy-like copy that does not warn about missing NUL and reports source length.
 *
 * Copies up to @p n bytes from @p src to @p dest, including a terminating NUL if
 * there is room. Returns the source length (not counting any terminator), which
 * matches the common POSIX behavior for bounded copies.
 *
 * @param dest Destination buffer.
 * @param src  Source C string.
 * @param n    Maximum number of bytes to write into @p dest.
 * @return Length of @p src (excluding any terminator).
 */
size_t strncpy_noterm(char *dest, const char *src, size_t n);

/**
 * @brief Convert a single ASCII hex character to its numeric value.
 * @param a Character to convert.
 * @return 0..15 on success; 255 if @p a is not a valid hex digit.
 */
uint8_t char_to_hex(char a);

/**
 * @brief Set a single bit in an integral value (0-based index).
 * @tparam T Integral type of @p value.
 * @param value     Lvalue to modify.
 * @param bitnumber Bit index to set (0 == LSB).
 */
template <typename T> void BIT_SET (T& value, uint8_t bitnumber) noexcept {
     static_assert(std::is_integral<T>::value, "Integral required.");
     ((value) |= ((T)(1U) << (bitnumber)));
 }

/**
 * @brief Clear a single bit in an integral value (0-based index).
 * @tparam T Integral type of @p value.
 * @param value     Lvalue to modify.
 * @param bitnumber Bit index to clear (0 == LSB).
 */
template <typename T> void BIT_CLEAR (T& value, uint8_t bitnumber) noexcept {
     static_assert(std::is_integral<T>::value, "Integral required.");
     ((value) &= ~((T)(1U) << (bitnumber)));
 }

/**
 * @def NEW_NOTHROW
 * @brief Convenience alias for placement-new with std::nothrow.
 */
#ifndef NEW_NOTHROW
#define NEW_NOTHROW new(std::nothrow)
#endif

/**
 * @brief Platform-overridable memory reallocator.
 *
 * Default implementation (see the corresponding .cpp) wraps @c std::realloc / @c std::free.
 * Platforms may override this (symbol is marked @ref WEAK) to route allocations
 * to a custom heap.
 *
 * Semantics:
 *  - If @p new_size == 0: frees @p ptr and returns nullptr.
 *  - If @p ptr == nullptr: behaves like @c malloc(new_size).
 *  - Else: behaves like @c realloc(ptr, new_size).
 *
 * @param ptr       Existing block pointer (or nullptr).
 * @param old_size  Previous size in bytes (may be ignored by default impl).
 * @param new_size  Requested size in bytes.
 * @return New block pointer, or nullptr on failure (or when @p new_size == 0).
 */
void * WEAK mem_realloc(void *ptr, size_t old_size, size_t new_size);

/** @} */ // end of Conversions & small utilities