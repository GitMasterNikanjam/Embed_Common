/**
 * @file test_location.cpp
 * @brief Minimal standalone tests for Location.h (no GoogleTest).
 *
 * @details
 * This file provides a small test harness (using `CHECK()` macros) to verify a few
 * core behaviors of the standalone Location library:
 *
 *   1) Horizontal distance between two WGS-84 coordinates (`get_distance()`).
 *   2) North/East offset in meters (`offset()`), including latitude/longitude changes.
 *   3) Altitude frame conversion using a provided "home" reference (`get_alt_cm()`).
 *
 * The goal is to validate correctness without any external test framework.
 * You can extend this file with more checks (bearing, sanitize, NE/NED distances, etc.).
 *
 * @section build Build Example
 * @code{.sh}
 * mkdir -p ./bin && g++ -std=c++11 -Wall -Wextra -I.. test_location.cpp ../Location.cpp ../../Embed_Math/Embed_Math.cpp -o ./bin/test_location
 * ./bin/test_location
 * @endcode
 */

#include <iostream>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include "../Location.h"  // Adjust relative path if needed

// -------------------------
// Tiny assertion utilities
// -------------------------
static int g_failures = 0;

/**
 * @brief Simple check macro that prints [OK] or [FAIL].
 */
#define CHECK(cond, msg) \
    do { \
        if (cond) { \
            std::cout << "[ OK ] " << msg << "\n"; \
        } else { \
            std::cerr << "[FAIL] " << msg << "\n"; \
            ++g_failures; \
        } \
    } while (0)

/**
 * @brief Compare two floats with tolerance.
 */
inline bool nearf(float a, float b, float tol) {
    return std::fabs(a - b) <= tol;
}

/**
 * @brief Macro wrapper for approximate float comparison with logging.
 */
#define CHECK_NEAR(val, expect, tol, msg) \
    CHECK(nearf((val), (expect), (tol)), msg << " (got=" << (val) << ", expect=" << (expect) << ", tol=" << (tol) << ")")

/**
 * @brief Pretty-print a Location (lat/lon in degrees, altitude in meters).
 */
static void print_loc(const char* tag, const Location& L)
{
    const double lat_deg = L.lat * 1e-7;
    const double lon_deg = L.lng * 1e-7;
    std::cout << tag << " lat=" << lat_deg << " deg, lon=" << lon_deg
              << " deg, alt=" << (L.alt / 100.0f) << " m\n";
}

/**
 * @brief Main test harness for Location library.
 *
 * Runs a few checks:
 *   - distance between two points
 *   - offset north/east
 *   - altitude conversion relative to home
 */
int main()
{
    std::cout << "==== Location basic tests (standalone) ====\n";

    // 1) Distance between two close landmarks (Sydney Opera House vs Harbour Bridge)
    Location L1(-338570000, 1512150000, 0, Location::AltFrame::ABSOLUTE); // Opera House
    Location L2(-338520000, 1512100000, 0, Location::AltFrame::ABSOLUTE); // Harbour Bridge

    const float d12 = L1.get_distance(L2);
    std::cout << "Distance(L1,L2) = " << d12 << " m\n";
    CHECK_NEAR(d12, 700.0f, 150.0f, "Distance between two nearby points is ~700 m");

    // 2) Offset tests: move north and east from L1
    Location L3 = L1;
    L3.offset(1000.0f, 0.0f); // +1000 m North
    const float dN = L1.get_distance(L3);
    print_loc("After +1000m North:", L3);
    CHECK_NEAR(dN, 1000.0f, 60.0f, "Offset +1000 m North changes distance by ~1000 m");
    CHECK(L3.lat > L1.lat, "Latitude increased after moving North");

    L3.offset(0.0f, 1000.0f); // +1000 m East
    const float dNE = L1.get_distance(L3);
    print_loc("After +1000m East:", L3);
    CHECK(L3.lng > L1.lng, "Longitude increased after moving East");
    CHECK(dNE > dN, "Distance from original point increased after East offset");

    // 3) Altitude test with "home" reference
    Location::clear_home();
    Location home(-338570000, 1512150000, 10000, Location::AltFrame::ABSOLUTE); // 100 m
    Location::set_home(home);

    Location L4(-338570000, 1512150000, 2000, Location::AltFrame::ABOVE_HOME); // 20 m above home
    int32_t alt_abs_cm;
    if (L4.get_alt_cm(Location::AltFrame::ABSOLUTE, alt_abs_cm)) {
        std::cout << "Altitude (ABSOLUTE) = " << alt_abs_cm / 100.0f << " m\n";
        CHECK(alt_abs_cm == 12000, "ABOVE_HOME altitude converted correctly to ABSOLUTE (120 m)");
    } else {
        CHECK(false, "Failed to convert ABOVE_HOME altitude to ABSOLUTE");
    }

    std::cout << "==== Tests complete. Failures: " << g_failures << " ====\n";
    return g_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
