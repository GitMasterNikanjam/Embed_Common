/**
 * @file test_embed_common.cpp
 * @brief Minimal unit-test harness for the Embed_Common library **without** GoogleTest.
 *
 * @details
 * This file implements a lightweight test framework (macros + counters) and a set of
 * unit tests that exercise selected helpers from your Embed_Common library:
 *  - @ref hex_to_uint8 (hex-character to 0..15)
 *  - @ref is_bounded_int32 (range check for 32-bit integers)
 *  - @ref BIT_SET / @ref BIT_CLEAR (bit manipulation)
 *  - @ref strncpy_noterm (bounded copy without forced null-termination)
 *
 * The harness prints failures to stdout/stderr-style via `std::printf`, keeps pass/fail
 * counters, and returns process exit code `0` on success and `1` on failure.
 *
 * ### Build
 * @code{.bash}
 * mkdir -p ./bin && g++ -std=c++11 -Wall -Wextra -I/.. test_embed_common.cpp ../Embed_Common.cpp -o ./bin/test_embed_common
 *
 * ./bin/test_embed_common
 * @endcode
 *
 * @note If your Embed_Common already defines `ARRAY_SIZE`, `BIT_SET`, or `BIT_CLEAR`,
 * the compatibility fallbacks here will be ignored due to include guards.
 *
 * @author
 *   Your Name <you@example.com>
 * @date 2025-09-24
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../Embed_Common.h"   ///< Include your library header (adjust the path if needed)

/** @defgroup Compat Compatibility helpers
 *  @brief Fallbacks when Embed_Common does not export certain utilities.
 *  @{
 */

/**
 * @brief Compile-time array length helper.
 *
 * @tparam T Element type (deduced)
 * @tparam N Array length (deduced)
 * @param[in]  (unnamed) reference to a C-array
 * @return constexpr size_t Number of elements in the array.
 *
 * @note Only defined if `ARRAY_SIZE` is not provided by your library.
 */
#ifndef ARRAY_SIZE
template <typename T, size_t N>
constexpr size_t ARRAY_SIZE(const T (&)[N]) { return N; }
#endif

/**
 * @brief Set bit @p b in variable @p v.
 *
 * @param v Lvalue variable (integral type).
 * @param b Zero-based bit index to set.
 * @note Only defined if `BIT_SET` is not provided by your library.
 */
#ifndef BIT_SET
#define BIT_SET(v, b)   ((v) |= (decltype(v))(1ULL << (b)))
#endif

/**
 * @brief Clear bit @p b in variable @p v.
 *
 * @param v Lvalue variable (integral type).
 * @param b Zero-based bit index to clear.
 * @note Only defined if `BIT_CLEAR` is not provided by your library.
 */
#ifndef BIT_CLEAR
#define BIT_CLEAR(v, b) ((v) &= (decltype(v))~(1ULL << (b)))
#endif
/** @} */ // end of Compat

/** @defgroup TinyTest Minimal test framework
 *  @brief A very small test framework implemented with macros and counters.
 *  @{
 */

/**
 * @brief Global counter of passed checks (assertions).
 */
static int g_pass = 0;

/**
 * @brief Global counter of failed checks (assertions).
 */
static int g_fail = 0;

/**
 * @brief Declare a test function.
 *
 * @param name Identifier of the test function.
 *
 * @note Each test is a `static void` function with no parameters.
 */
#define TEST(name) static void name()

/**
 * @brief Check that an expression is `true`.
 *
 * Increments @ref g_pass on success; otherwise increments @ref g_fail and prints details.
 *
 * @param expr Boolean expression to evaluate.
 */
#define CHECK_TRUE(expr) \
    do { if (expr) { g_pass++; } \
         else { g_fail++; std::printf("[FAIL] %s:%d  %s\n", __FILE__, __LINE__, #expr); } } while (0)

/**
 * @brief Check that an expression is `false`.
 *
 * @param expr Boolean expression to evaluate.
 * @see CHECK_TRUE
 */
#define CHECK_FALSE(expr) CHECK_TRUE(!(expr))

/**
 * @brief Check that two arithmetic values compare equal with `==`.
 *
 * @param a Left-hand value (evaluated once).
 * @param b Right-hand value (evaluated once).
 *
 * @note Values are printed as signed 64-bit integers for diagnostics; for large unsigned
 * values this may render negative numbers, which is acceptable for debugging here.
 */
#define CHECK_EQ(a,b) \
    do { auto _va = (a); auto _vb = (b); \
         if (_va == _vb) { g_pass++; } \
         else { g_fail++; std::printf("[FAIL] %s:%d  %s == %s  (got %lld, exp %lld)\n", \
                    __FILE__, __LINE__, #a, #b, (long long)_va, (long long)_vb); } } while (0)

/**
 * @brief Check that two C-strings are equal (`std::strcmp == 0`).
 *
 * @param a Pointer to NUL-terminated string.
 * @param b Pointer to NUL-terminated string.
 */
#define CHECK_STREQ(a,b) \
    do { if (std::strcmp((a),(b))==0) { g_pass++; } \
         else { g_fail++; std::printf("[FAIL] %s:%d  \"%s\" == \"%s\"  (got \"%s\")\n", \
                    __FILE__, __LINE__, #a, #b, (a)); } } while (0)

/**
 * @brief Check that two C-strings are not equal (`std::strcmp != 0`).
 *
 * @param a Pointer to NUL-terminated string.
 * @param b Pointer to NUL-terminated string.
 */
#define CHECK_STRNE(a,b) \
    do { if (std::strcmp((a),(b))!=0) { g_pass++; } \
         else { g_fail++; std::printf("[FAIL] %s:%d  \"%s\" != \"%s\"\n", \
                    __FILE__, __LINE__, #a, #b); } } while (0)
/** @} */ // end of TinyTest

/** @defgroup Tests Individual test cases
 *  @brief Unit tests for Embed_Common utilities.
 *  @{
 */

/**
 * @test Validate @ref hex_to_uint8 behavior for valid hex characters
 *       ('0'..'9', 'A'..'F', 'a'..'f') and several invalid ones.
 *
 * @details
 * For valid inputs the function must return `true` and place decoded nibble in `res`.
 * For invalid inputs it must return `false` and may leave `res` unchanged.
 */
TEST(Test_HexToUint8)
{
    uint8_t res;

    // '0'..'9'
    for (uint8_t ch = '0', exp = 0; ch <= '9'; ch++, exp++) {
        CHECK_TRUE(hex_to_uint8(ch, res));
        CHECK_EQ(res, exp);
    }

    // 'A'..'F'
    for (uint8_t ch = 'A', exp = 10; ch <= 'F'; ch++, exp++) {
        CHECK_TRUE(hex_to_uint8(ch, res));
        CHECK_EQ(res, exp);
    }

    // 'a'..'f'
    for (uint8_t ch = 'a', exp = 10; ch <= 'f'; ch++, exp++) {
        CHECK_TRUE(hex_to_uint8(ch, res));
        CHECK_EQ(res, exp);
    }

    // Invalid samples
    CHECK_FALSE(hex_to_uint8('G', res));
    CHECK_FALSE(hex_to_uint8('g', res));
    CHECK_FALSE(hex_to_uint8(';', res));
    CHECK_FALSE(hex_to_uint8('/', res));
    CHECK_FALSE(hex_to_uint8('@', res));
    CHECK_FALSE(hex_to_uint8('`', res));
}

/**
 * @test Validate @ref is_bounded_int32 for inclusive ranges on edge and out-of-range values.
 *
 * @details
 * Assumes `is_bounded_int32(val, low, high)` returns `true` iff `low <= val && val <= high`.
 */
TEST(Test_BoundedInt32)
{
    CHECK_TRUE (is_bounded_int32( 1,  0,  2));   // inside range
    CHECK_FALSE(is_bounded_int32( 3,  0,  2));   // above high
    CHECK_FALSE(is_bounded_int32(-1,  0,  2));   // below low
    CHECK_TRUE (is_bounded_int32( 0, -1,  2));   // at low
    CHECK_TRUE (is_bounded_int32(-1,-2,  0));    // at high
}

/**
 * @test Validate bit set/clear helpers across several integer widths.
 *
 * @details
 * Starts from value 128 (bit 7 set), sets bit 3 (adds 8), then clears bit 7 (removes 128),
 * expecting the final value to be 8 on each type.
 */
TEST(Test_BitSetClear)
{
    {
        uint16_t v = 128;
        BIT_SET(v, 3);      // set bit 3 -> 128 + 8 = 136
        CHECK_EQ(v, 136u);
        BIT_CLEAR(v, 7);    // clear bit 7 -> remove 128 -> 8
        CHECK_EQ(v, 8u);
    }
    {
        uint32_t v = 128;
        BIT_SET(v, 3);
        CHECK_EQ(v, 136u);
        BIT_CLEAR(v, 7);
        CHECK_EQ(v, 8u);
    }
    {
        unsigned long v = 128;
        BIT_SET(v, 3);
        CHECK_EQ(v, 136u);
        BIT_CLEAR(v, 7);
        CHECK_EQ(v, 8u);
    }
}

/**
 * @test Validate @ref strncpy_noterm behavior.
 *
 * @details
 * Copies a fixed number of bytes from source to destination without forcing a terminating
 * NUL in the final position (the caller provides a zero-initialized buffer to ensure
 * NUL-termination). Also tests the "exact copy" case where the entire source fits.
 */
TEST(Test_StrncpyNoTerm)
{
    const char src[] = "This is Embed_Common";
    char dest[ARRAY_SIZE(src) - 5] = {}; // smaller buffer; pre-zeroed for safety

    // Copy 12 chars. Function should not overrun and not force-terminate;
    // our pre-zeroed dest guarantees termination.
    strncpy_noterm(dest, src, 12);
    CHECK_STRNE(dest, src);
    CHECK_STREQ(dest, "This is Embe");

    // Exact copy case: shorter source fits; copy should match exactly.
    const char src2[] = "EmbedCommon";
    char dest2[ARRAY_SIZE(src) - 5] = {};
    strncpy_noterm(dest2, src2, 12);
    CHECK_STREQ(dest2, src2);
}
/** @} */ // end of Tests

/**
 * @brief Program entry point for the minimal test runner.
 *
 * @return `0` if all checks passed; `1` if any check failed.
 */
int main()
{
    std::printf("Running Embed_Common tests (no gtest)...\n");

    Test_HexToUint8();
    Test_BoundedInt32();
    Test_BitSetClear();
    Test_StrncpyNoTerm();

    std::printf("\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
