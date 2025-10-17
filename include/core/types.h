#pragma once
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdint>

namespace trading {

using Symbol = std::string;
using Price = double;
using Volume = int64_t;
using Timestamp = std::chrono::system_clock::time_point;

// Convert timestamp to milliseconds since epoch
inline int64_t to_timestamp_ms(Timestamp ts) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        ts.time_since_epoch()
    ).count();
}

// Convert milliseconds to Timestamp
inline Timestamp from_timestamp_ms(int64_t ms) {
    return Timestamp(std::chrono::milliseconds(ms));
}

} // namespace trading
