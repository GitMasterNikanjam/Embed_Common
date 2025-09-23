/*
 * Standalone Location.cpp (no AP_AHRS/AP_Terrain deps)
 */

#include "Location.h"
#include <cstring>
#include <cfloat>

// --- local helpers for standalone build (lat/lon in 1e-7 degrees) ---
bool check_lat(int32_t lat) { return lat >= -900000000L  && lat <=  900000000L;  }
bool check_lng(int32_t lng) { return lng >= -1800000000L && lng <= 1800000000L; }

// ---- static hook storage ----
static bool        s_home_set   = false;
static Location    s_home{};
static bool        s_origin_set = false;
static Location    s_origin{};
static bool      (*s_terrain_provider)(const Location&, float&) = nullptr;

// a canonical zero-initialized Location for fast memcmp in is_zero()
static const Location definitely_zero{};

// ---- hook APIs ----
void Location::set_home(const Location& home)       { s_home = home; s_home_set = true; }
void Location::clear_home()                         { s_home_set = false; s_home = Location{}; }
bool Location::home_is_set()                        { return s_home_set; }
const Location& Location::home()                    { return s_home; }

void Location::set_origin(const Location& origin)   { s_origin = origin; s_origin_set = true; }
void Location::clear_origin()                       { s_origin_set = false; s_origin = Location{}; }
bool Location::origin_is_set()                      { return s_origin_set; }
const Location& Location::origin()                  { return s_origin; }

void Location::set_terrain_provider(bool (*provider)(const Location&, float&)) {
    s_terrain_provider = provider;
}

// ---- basic helpers ----
bool Location::is_zero(void) const
{
    return std::memcmp(this, &definitely_zero, sizeof(*this)) == 0;
}
void Location::zero(void)
{
    std::memset(this, 0, sizeof(*this));
}

// ---- ctors ----
Location::Location(int32_t latitude, int32_t longitude, int32_t alt_in_cm, AltFrame frame)
{
    zero();
    lat = latitude;
    lng = longitude;
    set_alt_cm(alt_in_cm, frame);
}

Location::Location(const Vector3f &ekf_offset_neu, AltFrame frame)
{
    zero();
    set_alt_cm(int32_t(ekf_offset_neu.z), frame);
    if (s_origin_set) {
        lat = s_origin.lat;
        lng = s_origin.lng;
        offset(ekf_offset_neu.x * 0.01f, ekf_offset_neu.y * 0.01f);
    }
}

Location::Location(const Vector3d &ekf_offset_neu, AltFrame frame)
{
    zero();
    set_alt_cm(int32_t(ekf_offset_neu.z), frame);
    if (s_origin_set) {
        lat = s_origin.lat;
        lng = s_origin.lng;
        offset(ftype(ekf_offset_neu.x * 0.01), ftype(ekf_offset_neu.y * 0.01));
    }
}

// ---- altitude ----
void Location::set_alt_cm(int32_t alt_cm, AltFrame frame)
{
    alt = alt_cm;
    relative_alt = false;
    terrain_alt  = false;
    origin_alt   = false;
    switch (frame) {
    case AltFrame::ABSOLUTE:      break;
    case AltFrame::ABOVE_HOME:    relative_alt = true; break;
    case AltFrame::ABOVE_ORIGIN:  origin_alt   = true; break;
    case AltFrame::ABOVE_TERRAIN: relative_alt = true; terrain_alt = true; break;
    }
}

// converts altitude to new frame
bool Location::change_alt_frame(AltFrame desired_frame)
{
    int32_t new_alt_cm;
    if (!get_alt_cm(desired_frame, new_alt_cm)) {
        return false;
    }
    set_alt_cm(new_alt_cm, desired_frame);
    return true;
}

Location::AltFrame Location::get_alt_frame() const
{
    if (terrain_alt) return AltFrame::ABOVE_TERRAIN;
    if (origin_alt)  return AltFrame::ABOVE_ORIGIN;
    if (relative_alt) return AltFrame::ABOVE_HOME;
    return AltFrame::ABSOLUTE;
}

// Must not change ret_alt_cm unless true is returned!
bool Location::get_alt_cm(AltFrame desired_frame, int32_t &ret_alt_cm) const
{
    const AltFrame frame = get_alt_frame();
    if (desired_frame == frame) { ret_alt_cm = alt; return true; }

    // If terrain is involved, get terrain height (meters) via provider
    float terr_m = 0.0f;
    if (frame == AltFrame::ABOVE_TERRAIN || desired_frame == AltFrame::ABOVE_TERRAIN) {
        if (!s_terrain_provider) return false;
        if (!s_terrain_provider(*this, terr_m)) return false;
    }

    // Convert from source frame to absolute (cm)
    int32_t alt_abs_cm = 0;
    switch (frame) {
    case AltFrame::ABSOLUTE:
        alt_abs_cm = alt;
        break;
    case AltFrame::ABOVE_HOME:
        if (!s_home_set) return false;
        alt_abs_cm = alt + s_home.alt;
        break;
    case AltFrame::ABOVE_ORIGIN:
        if (!s_origin_set) return false;
        alt_abs_cm = alt + s_origin.alt;
        break;
    case AltFrame::ABOVE_TERRAIN:
        alt_abs_cm = alt + int32_t(terr_m * 100.0f);
        break;
    }

    // Convert absolute to desired frame
    switch (desired_frame) {
    case AltFrame::ABSOLUTE:
        ret_alt_cm = alt_abs_cm;
        return true;
    case AltFrame::ABOVE_HOME:
        if (!s_home_set) return false;
        ret_alt_cm = alt_abs_cm - s_home.alt;
        return true;
    case AltFrame::ABOVE_ORIGIN:
        if (!s_origin_set) return false;
        ret_alt_cm = alt_abs_cm - s_origin.alt;
        return true;
    case AltFrame::ABOVE_TERRAIN:
        ret_alt_cm = alt_abs_cm - int32_t(terr_m * 100.0f);
        return true;
    }
    return false; // not reached
}

// ---- origin-relative vectors ----
template<typename T>
bool Location::get_vector_xy_from_origin_NE_cm(T &vec_ne) const
{
    if (!s_origin_set) return false;
    vec_ne.x = (lat - s_origin.lat) * LATLON_TO_CM;
    vec_ne.y = diff_longitude(lng, s_origin.lng) * LATLON_TO_CM *
               longitude_scale((lat + s_origin.lat) / 2);
    return true;
}
template<typename T>
bool Location::get_vector_from_origin_NEU_cm(T &vec_neu) const
{
    int32_t alt_above_origin_cm = 0;
    if (!get_alt_cm(AltFrame::ABOVE_ORIGIN, alt_above_origin_cm)) return false;
    if (!get_vector_xy_from_origin_NE_cm(vec_neu.xy()))          return false;
    vec_neu.z = alt_above_origin_cm;
    return true;
}

// ---- distances / geometry ----
ftype Location::get_distance(const Location &loc2) const
{
    const ftype dlat = ftype(loc2.lat - lat);
    const ftype dlng = ftype(diff_longitude(loc2.lng, lng)) *
                       longitude_scale((lat + loc2.lat)/2);
    return norm(dlat, dlng) * LOCATION_SCALING_FACTOR;
}

Vector2f Location::get_distance_NE(const Location &loc2) const
{
    return Vector2f(
        (loc2.lat - lat) * LOCATION_SCALING_FACTOR,
        diff_longitude(loc2.lng, lng) * LOCATION_SCALING_FACTOR *
        longitude_scale((loc2.lat + lat)/2)
    );
}

Vector3f Location::get_distance_NED(const Location &loc2) const
{
    return Vector3f(
        (loc2.lat - lat) * LOCATION_SCALING_FACTOR,
        diff_longitude(loc2.lng, lng) * LOCATION_SCALING_FACTOR *
        longitude_scale((lat + loc2.lat)/2),
        (alt - loc2.alt) * 0.01f
    );
}

Vector3p Location::get_distance_NED_postype(const Location &loc2) const
{
    return Vector3p(
        (loc2.lat - lat) * LOCATION_SCALING_FACTOR,
        diff_longitude(loc2.lng, lng) * LOCATION_SCALING_FACTOR *
        longitude_scale((lat + loc2.lat)/2),
        (alt - loc2.alt) * 0.01f
    );
}

Vector3d Location::get_distance_NED_double(const Location &loc2) const
{
    return Vector3d(
        (loc2.lat - lat) * double(LOCATION_SCALING_FACTOR),
        diff_longitude(loc2.lng, lng) * LOCATION_SCALING_FACTOR *
        longitude_scale((lat + loc2.lat)/2),
        (alt - loc2.alt) * 0.01
    );
}

Vector3f Location::get_distance_NED_alt_frame(const Location &loc2) const
{
    int32_t alt1 = 0, alt2 = 0;
    if (!get_alt_cm(AltFrame::ABSOLUTE, alt1) ||
        !loc2.get_alt_cm(AltFrame::ABSOLUTE, alt2)) {
        alt1 = 0; alt2 = 0; // if unavailable, ignore vertical
    }
    return Vector3f(
        (loc2.lat - lat) * LOCATION_SCALING_FACTOR,
        diff_longitude(loc2.lng, lng) * LOCATION_SCALING_FACTOR *
        longitude_scale((loc2.lat + lat)/2),
        (alt1 - alt2) * 0.01f
    );
}

Vector2d Location::get_distance_NE_double(const Location &loc2) const
{
    return Vector2d(
        (loc2.lat - lat) * double(LOCATION_SCALING_FACTOR),
        diff_longitude(loc2.lng, lng) * double(LOCATION_SCALING_FACTOR) *
        longitude_scale((lat + loc2.lat)/2)
    );
}

Vector2p Location::get_distance_NE_postype(const Location &loc2) const
{
    return Vector2p(
        (loc2.lat - lat) * ftype(LOCATION_SCALING_FACTOR),
        diff_longitude(loc2.lng, lng) * ftype(LOCATION_SCALING_FACTOR) *
        longitude_scale((lat + loc2.lat)/2)
    );
}

Vector2F Location::get_distance_NE_ftype(const Location &loc2) const
{
    return Vector2F(
        (loc2.lat - lat) * ftype(LOCATION_SCALING_FACTOR),
        diff_longitude(loc2.lng, lng) * ftype(LOCATION_SCALING_FACTOR) *
        longitude_scale((lat + loc2.lat)/2)
    );
}

// ---- offsets ----
void Location::offset_latlng(int32_t &lat_io, int32_t &lng_io, ftype ofs_north, ftype ofs_east)
{
    const int32_t dlat = ofs_north * LOCATION_SCALING_FACTOR_INV;
    const int64_t dlng = (ofs_east * LOCATION_SCALING_FACTOR_INV) /
                         longitude_scale(lat_io + dlat/2);
    lat_io += dlat;
    lat_io  = limit_lattitude(lat_io);
    lng_io  = wrap_longitude(dlng + lng_io);
}

void Location::offset(ftype ofs_north, ftype ofs_east)
{
    offset_latlng(lat, lng, ofs_north, ofs_east);
}

void Location::offset(const Vector3p &ofs_ned)
{
    offset_latlng(lat, lng, ofs_ned.x, ofs_ned.y);
    alt += int32_t(-ofs_ned.z * 100.0f); // NED z is down; alt positive up(cm)
}

void Location::offset_bearing(ftype bearing_deg, ftype distance)
{
    const ftype ofs_north = cosF(radians(bearing_deg)) * distance;
    const ftype ofs_east  = sinF(radians(bearing_deg)) * distance;
    offset(ofs_north, ofs_east);
}

void Location::offset_bearing_and_pitch(ftype bearing_deg, ftype pitch_deg, ftype distance)
{
    const ftype cP = cosF(radians(pitch_deg));
    const ftype ofs_north =  cP * cosF(radians(bearing_deg)) * distance;
    const ftype ofs_east  =  cP * sinF(radians(bearing_deg)) * distance;
    offset(ofs_north, ofs_east);
    const int32_t dalt =  sinF(radians(pitch_deg)) * distance * 100.0f;
    alt += dalt;
}

ftype Location::longitude_scale(int32_t lat_e7)
{
    ftype scale = cosF(lat_e7 * (1.0e-7f * DEG_TO_RAD));
    return MAX(scale, 0.01f);
}

// ---- misc ----
bool Location::sanitize(const Location &defaultLoc)
{
    bool changed = false;

    if (lat == 0 && lng == 0) { lat = defaultLoc.lat; lng = defaultLoc.lng; changed = true; }

    if (alt == 0 && relative_alt) {
        int32_t alt_cm;
        if (defaultLoc.get_alt_cm(get_alt_frame(), alt_cm)) {
            alt = alt_cm;
            changed = true;
        }
    }

    if (!check_latlng()) {
        lat = defaultLoc.lat;
        lng = defaultLoc.lng;
        changed = true;
    }
    return changed;
}

ftype Location::get_bearing(const Location &loc2) const
{
    const int32_t off_x = diff_longitude(loc2.lng, lng);
    const int32_t off_y = (loc2.lat - lat) / loc2.longitude_scale((lat + loc2.lat)/2);
    ftype bearing = (M_PI*0.5f) + atan2F(-off_y, off_x);
    if (bearing < 0) bearing += 2.0f * M_PI;
    return bearing;
}

bool Location::same_alt_as(const Location &loc2) const
{
    if (this->get_alt_frame() == loc2.get_alt_frame()) {
        return this->alt == loc2.alt;
    }
    ftype diff_m;
    int32_t a1, a2;
    if (!get_alt_cm(AltFrame::ABSOLUTE, a1) || !loc2.get_alt_cm(AltFrame::ABSOLUTE, a2)) {
        return false;
    }
    diff_m = (a1 - a2) * 0.01f;
    return fabsF(diff_m) < FLT_EPSILON;
}

bool Location::check_latlng() const
{
    return check_lat(lat) && check_lng(lng);
}

bool Location::past_interval_finish_line(const Location &p1, const Location &p2) const
{
    return line_path_proportion(p1, p2) >= 1.0f;
}

float Location::line_path_proportion(const Location &p1, const Location &p2) const
{
    const Vector2f v12 = p1.get_distance_NE(p2);
    const Vector2f v1s = p1.get_distance_NE(*this);
    const ftype dsq = sq(v12.x) + sq(v12.y);
    if (dsq < 0.001f) return 1.0f;
    return (v12 * v1s) / dsq;
}

int32_t Location::wrap_longitude(int64_t lon)
{
    if (lon > 1800000000LL) {
        lon = int32_t(lon - 3600000000LL);
    } else if (lon < -1800000000LL) {
        lon = int32_t(lon + 3600000000LL);
    }
    return int32_t(lon);
}

int32_t Location::diff_longitude(int32_t lon1, int32_t lon2)
{
    if ((lon1 & 0x80000000) == (lon2 & 0x80000000)) {
        return lon1 - lon2;
    }
    int64_t dlon = int64_t(lon1) - int64_t(lon2);
    if (dlon >  1800000000LL) dlon -= 3600000000LL;
    if (dlon < -1800000000LL) dlon += 3600000000LL;
    return int32_t(dlon);
}

int32_t Location::limit_lattitude(int32_t lat_e7)
{
    if (lat_e7 >  900000000L)  { lat_e7 = 1800000000LL - lat_e7; }
    else if (lat_e7 < -900000000L) { lat_e7 = -(1800000000LL + lat_e7); }
    return lat_e7;
}

void Location::linearly_interpolate_alt(const Location &p1, const Location &p2)
{
    const float t = constrain_float(line_path_proportion(p1, p2), 0.0f, 1.0f);
    set_alt_cm(p1.alt + int32_t((p2.alt - p1.alt) * t), p2.get_alt_frame());
}
