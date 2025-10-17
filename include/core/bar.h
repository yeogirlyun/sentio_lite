#pragma once
#include "types.h"
#include <string>

namespace trading {

struct Bar {
    // Immutable, globally unique identifier for this bar
    // Generated from timestamp and symbol at load time
    // Layout: [16 bits symbol hash][48 bits timestamp_ms]
    uint64_t bar_id = 0;

    Timestamp timestamp;
    Price open;
    Price high;
    Price low;
    Price close;
    Volume volume;

    // Symbol name for bar_id generation and validation
    std::string symbol;

    Bar() = default;
    Bar(Timestamp ts, Price o, Price h, Price l, Price c, Volume v, const std::string& sym = "")
        : timestamp(ts), open(o), high(h), low(l), close(c), volume(v), symbol(sym) {}

    // Convenience constructor with timestamp_ms
    Bar(int64_t ts_ms, Price o, Price h, Price l, Price c, Volume v, const std::string& sym = "")
        : timestamp(from_timestamp_ms(ts_ms))
        , open(o), high(h), low(l), close(c), volume(v), symbol(sym) {}
};

} // namespace trading
