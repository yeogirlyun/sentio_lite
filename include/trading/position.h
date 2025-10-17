#pragma once
#include "core/types.h"

namespace trading {

/**
 * Position - Represents an open trading position
 *
 * Tracks:
 * - Number of shares (positive = long, negative = short)
 * - Entry price
 * - Entry timestamp
 * - Entry bar ID (for synchronization validation)
 * - Unrealized P&L calculations
 */
struct Position {
    int shares;              // Number of shares (can be negative for short)
    Price entry_price;       // Entry price per share
    Timestamp entry_time;    // When position was opened
    uint64_t entry_bar_id;   // Bar ID for entry (for synchronization validation)

    Position() : shares(0), entry_price(0.0), entry_bar_id(0) {}

    Position(int s, Price p, Timestamp t, uint64_t bid = 0)
        : shares(s), entry_price(p), entry_time(t), entry_bar_id(bid) {}

    /**
     * Calculate unrealized P&L in dollars
     */
    double unrealized_pnl(Price current_price) const {
        return shares * (current_price - entry_price);
    }

    /**
     * Calculate unrealized P&L as percentage of initial investment
     */
    double pnl_percentage(Price current_price) const {
        if (entry_price == 0) return 0.0;
        return (current_price - entry_price) / entry_price;
    }

    /**
     * Check if position is long
     */
    bool is_long() const { return shares > 0; }

    /**
     * Check if position is short
     */
    bool is_short() const { return shares < 0; }

    /**
     * Check if position is flat (no shares)
     */
    bool is_flat() const { return shares == 0; }

    /**
     * Get position value at current price
     */
    double market_value(Price current_price) const {
        return shares * current_price;
    }
};

} // namespace trading
