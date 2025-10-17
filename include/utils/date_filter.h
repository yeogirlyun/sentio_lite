#pragma once
#include "core/types.h"
#include "core/bar.h"
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace trading {

/**
 * Date utilities for filtering market data
 */
class DateFilter {
public:
    /**
     * Parse date string in format YYYY-MM-DD
     * @param date_str Date string (e.g., "2024-01-15")
     * @return Timestamp
     */
    static Timestamp parse_date(const std::string& date_str) {
        std::tm tm = {};
        std::istringstream ss(date_str);
        ss >> std::get_time(&tm, "%Y-%m-%d");

        if (ss.fail()) {
            throw std::runtime_error("Invalid date format: " + date_str +
                                   " (expected YYYY-MM-DD)");
        }

        auto time_c = std::mktime(&tm);
        return std::chrono::system_clock::from_time_t(time_c);
    }

    /**
     * Filter bars by date range
     * @param bars Input bars
     * @param start_date Start date (YYYY-MM-DD) or empty for no filter
     * @param end_date End date (YYYY-MM-DD) or empty for no filter
     * @return Filtered bars
     */
    static std::vector<Bar> filter(const std::vector<Bar>& bars,
                                   const std::string& start_date,
                                   const std::string& end_date) {
        if (start_date.empty() && end_date.empty()) {
            return bars;  // No filtering
        }

        Timestamp start_ts = start_date.empty()
            ? Timestamp::min()
            : parse_date(start_date);

        Timestamp end_ts = end_date.empty()
            ? Timestamp::max()
            : parse_date(end_date) + std::chrono::hours(24);  // Include end day

        std::vector<Bar> filtered;
        filtered.reserve(bars.size());

        for (const auto& bar : bars) {
            if (bar.timestamp >= start_ts && bar.timestamp < end_ts) {
                filtered.push_back(bar);
            }
        }

        return filtered;
    }

    /**
     * Format timestamp as YYYY-MM-DD
     */
    static std::string format_date(Timestamp ts) {
        auto time_c = std::chrono::system_clock::to_time_t(ts);
        std::tm tm;
        localtime_r(&time_c, &tm);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d");
        return oss.str();
    }

    /**
     * Format timestamp as YYYY-MM-DD HH:MM:SS
     */
    static std::string format_datetime(Timestamp ts) {
        auto time_c = std::chrono::system_clock::to_time_t(ts);
        std::tm tm;
        localtime_r(&time_c, &tm);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace trading
