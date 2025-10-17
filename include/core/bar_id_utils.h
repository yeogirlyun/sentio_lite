#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace trading {

/**
 * Generate a stable 64-bit bar identifier from timestamp and symbol
 *
 * Layout: [16 bits symbol hash][48 bits timestamp_ms]
 *
 * This ensures:
 * - Same timestamp + symbol = same bar_id (deterministic)
 * - Different symbols at same time = different bar_ids
 * - bar_id encodes both temporal and symbol information
 *
 * CRITICAL: This is used for synchronization across symbols.
 * All bars with the same timestamp_ms (but different symbols) will have
 * the same temporal component but different symbol hashes.
 *
 * @param timestamp_ms Milliseconds since Unix epoch
 * @param symbol Symbol name (e.g., "TQQQ", "SQQQ")
 * @return Unique 64-bit bar identifier
 */
inline uint64_t generate_bar_id(int64_t timestamp_ms, const std::string& symbol) {
    // Lower 48 bits: timestamp (supports up to year 10889)
    uint64_t timestamp_part = static_cast<uint64_t>(timestamp_ms) & 0xFFFFFFFFFFFFULL;

    // Upper 16 bits: symbol hash (65536 possible values)
    uint32_t symbol_hash = static_cast<uint32_t>(std::hash<std::string>{}(symbol));
    uint64_t symbol_part = (static_cast<uint64_t>(symbol_hash) & 0xFFFFULL) << 48;

    return timestamp_part | symbol_part;
}

/**
 * Extract timestamp (milliseconds) from bar_id
 *
 * @param bar_id The bar identifier
 * @return Timestamp in milliseconds since Unix epoch
 */
inline int64_t extract_timestamp_ms(uint64_t bar_id) {
    return static_cast<int64_t>(bar_id & 0xFFFFFFFFFFFFULL);
}

/**
 * Extract symbol hash from bar_id
 *
 * @param bar_id The bar identifier
 * @return 16-bit symbol hash
 */
inline uint16_t extract_symbol_hash(uint64_t bar_id) {
    return static_cast<uint16_t>((bar_id >> 48) & 0xFFFFULL);
}

/**
 * Check if two bar_ids represent the same timestamp (across different symbols)
 *
 * @param bar_id1 First bar identifier
 * @param bar_id2 Second bar identifier
 * @return true if timestamps match
 */
inline bool same_timestamp(uint64_t bar_id1, uint64_t bar_id2) {
    return extract_timestamp_ms(bar_id1) == extract_timestamp_ms(bar_id2);
}

} // namespace trading
