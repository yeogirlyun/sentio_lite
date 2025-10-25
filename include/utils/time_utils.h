#pragma once

#include <ctime>
#include <cstdlib>

namespace utils {

/**
 * Calculate minutes from US market open (9:30 ET)
 *
 * @param timestamp_ms Unix timestamp in milliseconds
 * @return Minutes from market open (0-based), or -1 if market is closed
 *
 * Examples:
 *   9:30 AM ET → 0
 *   9:31 AM ET → 1
 *   4:00 PM ET → 390
 *   Weekend/after hours → -1
 */
inline int calculate_minutes_from_open(long long timestamp_ms) {
    // Save current timezone
    char* old_tz = getenv("TZ");

    // Set to Eastern Time
    setenv("TZ", "America/New_York", 1);
    tzset();

    const time_t timestamp_sec = timestamp_ms / 1000;
    std::tm tm_local;
    localtime_r(&timestamp_sec, &tm_local);

    // Restore old timezone
    if (old_tz) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    // Check if weekend (0=Sunday, 6=Saturday)
    if (tm_local.tm_wday == 0 || tm_local.tm_wday == 6) {
        return -1;
    }

    int total_minutes_since_midnight = tm_local.tm_hour * 60 + tm_local.tm_min;

    // Market hours: 9:30 AM to 4:00 PM ET
    const int market_open_minutes = 9 * 60 + 30;   // 570
    const int market_close_minutes = 16 * 60;       // 960

    if (total_minutes_since_midnight < market_open_minutes ||
        total_minutes_since_midnight >= market_close_minutes) {
        return -1; // Market closed
    }

    // Return 0-based minute index (9:30 = 0, 9:31 = 1, etc.)
    return total_minutes_since_midnight - market_open_minutes;
}

/**
 * Convert timestamp to bar index of the day (1-based)
 *
 * @param timestamp_ms Unix timestamp in milliseconds
 * @return Bar index (1-391 for regular trading day), or -1 if market closed
 */
inline int get_bar_index_of_day(long long timestamp_ms) {
    int minute_index = calculate_minutes_from_open(timestamp_ms);
    if (minute_index == -1) {
        return -1; // Market closed
    }
    // Convert to 1-based index (bar 1, bar 2, ..., bar 391)
    return minute_index + 1;
}

} // namespace utils
