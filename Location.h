#pragma once
/**
 * @file Location.h
 * @brief Standalone WGS-84 location utilities with optional home/origin/terrain hooks.
 *
 * This header removes all dependencies on AP_AHRS/AP_Terrain. Instead, the application
 * may optionally provide:
 *  - a "home" Location via set_home()/clear_home()
 *  - an "origin" Location via set_origin()/clear_origin()
 *  - a terrain query callback via set_terrain_provider()
 *
 * If these are not provided, conversions that rely on them (ABOVE_HOME, ABOVE_ORIGIN,
 * ABOVE_TERRAIN) will fail gracefully and return false.
 */

#include "../Embed_Math/Embed_Math.h"

#define LOCATION_ALT_MAX_M  83000   // max altitude (m) that fits in int32 cm

class Location
{
public:
    // bitfields (compat with ArduPilot storage layout)
    uint8_t relative_alt : 1;    ///< 1 if altitude is relative to home
    uint8_t loiter_ccw   : 1;    ///< 0 if clockwise, 1 if counter-clockwise
    uint8_t terrain_alt  : 1;    ///< altitude is above terrain
    uint8_t origin_alt   : 1;    ///< altitude is above EKF origin
    uint8_t loiter_xtrack: 1;    ///< loiter crosstrack mode

    // note: mission storage uses 24 bits for altitude (~ +/- 83 km)
    int32_t alt;   ///< centimeters
    int32_t lat;   ///< 1e-7 degrees
    int32_t lng;   ///< 1e-7 degrees

    /// altitude reference frame
    enum class AltFrame {
        ABSOLUTE = 0,
        ABOVE_HOME = 1,
        ABOVE_ORIGIN = 2,
        ABOVE_TERRAIN = 3
    };

    // ---- constructors ----
    Location() { zero(); }

    /**
     * @brief Construct from lat/lon (1e-7 deg) and altitude (cm) in a given frame.
     * @note For non-ABSOLUTE frames, conversions later depend on optional hooks (home/origin/terrain).
     */
    Location(int32_t latitude, int32_t longitude, int32_t alt_in_cm, AltFrame frame);

    /**
     * @brief Construct from NEU (cm) offset w.r.t. origin and altitude frame.
     * @note Requires that an origin has been set via set_origin(); otherwise lat/lon remain zero.
     */
    Location(const Vector3f &ekf_offset_neu, AltFrame frame);
    Location(const Vector3d &ekf_offset_neu, AltFrame frame);

    // ---- altitude accessors/converters ----

    void set_alt_cm(int32_t alt_cm, AltFrame frame);
    void set_alt_m(float alt_m, AltFrame frame) { set_alt_cm(int32_t(alt_m*100.0f), frame); }

    /**
     * @brief Get altitude in desired frame (cm).
     * @return false if conversion requires hooks that are not set (home/origin/terrain) or provider fails.
     */
    bool get_alt_cm(AltFrame desired_frame, int32_t &ret_alt_cm) const;

    /// Same as get_alt_cm but returns meters.
    bool get_alt_m(AltFrame desired_frame, float &ret_alt) const {
        int32_t cm;
        if (!get_alt_cm(desired_frame, cm)) return false;
        ret_alt = cm * 0.01f;
        return true;
    }

    /// Current altitude frame of this Location (based on flags).
    AltFrame get_alt_frame() const;

    /// Convert altitude to a new frame (updates this Location). Returns false on failure (see get_alt_cm).
    bool change_alt_frame(AltFrame desired_frame);

    /// Copy altitude and its frame flags from another Location.
    void copy_alt_from(const Location &other) {
        alt = other.alt;
        relative_alt = other.relative_alt;
        terrain_alt  = other.terrain_alt;
        origin_alt   = other.origin_alt;
    }

    // ---- origin-relative vectors ----
    // These require an origin to be set via set_origin(). If not set, they return false.

    template<typename T>
    bool get_vector_xy_from_origin_NE_cm(T &vec_ne) const;     ///< NE (cm) w.r.t. origin

    template<typename T>
    bool get_vector_from_origin_NEU_cm(T &vec_neu) const;      ///< NEU (cm) w.r.t. origin

    template<typename T>
    bool get_vector_from_origin_NEU(T &vec_neu) const {        ///< alias
        return get_vector_from_origin_NEU_cm(vec_neu);
    }

    template<typename T>
    bool get_vector_xy_from_origin_NE_m(T &vec_ne) const {     ///< NE (m) w.r.t. origin
        if (!get_vector_xy_from_origin_NE_cm(vec_ne)) return false;
        vec_ne *= 0.01f;
        return true;
    }

    template<typename T>
    bool get_vector_from_origin_NEU_m(T &vec_neu) const {      ///< NEU (m) w.r.t. origin
        if (!get_vector_from_origin_NEU_cm(vec_neu)) return false;
        vec_neu *= 0.01f;
        return true;
    }

    // ---- geometry utilities (standalone) ----

    ftype   get_distance(const Location &loc2) const;           ///< horizontal distance (m)
    Vector2f get_distance_NE(const Location &loc2) const;       ///< NE (m) to loc2
    Vector3f get_distance_NED(const Location &loc2) const;      ///< NED (m), ignoring alt frame
    Vector3f get_distance_NED_alt_frame(const Location &loc2) const; ///< NED (m), converting alt frames if possible

    Vector2d get_distance_NE_double(const Location &loc2) const;
    Vector3d get_distance_NED_double(const Location &loc2) const;

    Vector2p get_distance_NE_postype(const Location &loc2) const;
    Vector3p get_distance_NED_postype(const Location &loc2) const;

    Vector2F get_distance_NE_ftype(const Location &loc2) const;

    static void  offset_latlng(int32_t &lat, int32_t &lng, ftype ofs_north, ftype ofs_east);
    void         offset(ftype ofs_north, ftype ofs_east);
    void         offset(const Vector3p &ofs_ned);               ///< meters; z positive up (NED)
    void         offset_up_cm(int32_t alt_offset_cm) { alt += alt_offset_cm; }
    void         offset_up_m(float alt_offset_m) { alt += int32_t(alt_offset_m * 100.0f); }

    void         offset_bearing(ftype bearing_deg, ftype distance);
    void         offset_bearing_and_pitch(ftype bearing_deg, ftype pitch_deg, ftype distance);

    static ftype longitude_scale(int32_t lat_e7);

    bool is_zero() const;
    void zero();

    ftype   get_bearing(const Location &loc2) const;            ///< radians, [0, 2π)
    int32_t get_bearing_to(const Location &loc2) const { return int32_t(rad_to_cd(get_bearing(loc2)) + 0.5f); }

    bool same_latlon_as(const Location &loc2) const { return lat == loc2.lat && lng == loc2.lng; }
    bool same_alt_as(const Location &loc2) const;                ///< compares after converting frames if possible
    bool same_loc_as(const Location &loc2) const { return same_latlon_as(loc2) && same_alt_as(loc2); }

    bool sanitize(const Location &defaultLoc);                   ///< fix invalid fields using defaults
    bool check_latlng() const;                                   ///< latitude/longitude in valid ranges?

    bool past_interval_finish_line(const Location &p1, const Location &p2) const;
    float line_path_proportion(const Location &p1, const Location &p2) const;

    void linearly_interpolate_alt(const Location &p1, const Location &p2);

    bool initialised() const { return (lat != 0 || lng != 0 || alt != 0); }
    bool alt_is_zero() const { return alt == 0; }

    static int32_t wrap_longitude(int64_t lon);
    static int32_t limit_lattitude(int32_t lat);
    static int32_t diff_longitude(int32_t lon1, int32_t lon2);

    // ---- Hooks to replace AP_AHRS/AP_Terrain ----

    /** Set/clear "home" reference. */
    static void set_home(const Location& home);
    static void clear_home();
    static bool home_is_set();
    static const Location& home();

    /** Set/clear "origin" (EKF origin) reference. */
    static void set_origin(const Location& origin);
    static void clear_origin();
    static bool origin_is_set();
    static const Location& origin();

    /**
     * @brief Set a terrain provider callback.
     * @param provider Function returning terrain height AMSL (meters) at the given Location.
     *                 Signature: bool provider(const Location&, float& height_m)
     *                 Return true on success, false on failure/unavailable.
     */
    static void set_terrain_provider(bool (*provider)(const Location&, float&));

private:
    static constexpr float LOCATION_SCALING_FACTOR     = LATLON_TO_M;
    static constexpr float LOCATION_SCALING_FACTOR_INV = LATLON_TO_M_INV;
};
