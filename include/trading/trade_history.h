#pragma once
#include "utils/circular_buffer.h"
#include "core/types.h"

namespace trading {

/**
 * Trade Record - Stores completed trade information
 */
struct TradeRecord {
    double pnl;          // Profit/Loss in dollars
    double pnl_pct;      // P&L as percentage
    Timestamp entry_time;
    Timestamp exit_time;
    Symbol symbol;
    int shares;
    Price entry_price;
    Price exit_price;
    uint64_t entry_bar_id;  // Bar ID for entry (for synchronization validation)
    uint64_t exit_bar_id;   // Bar ID for exit (for synchronization validation)
    size_t exit_bar_index;  // Bar index when trade exited (for filtering test-day trades)

    TradeRecord()
        : pnl(0.0), pnl_pct(0.0), shares(0), entry_price(0.0), exit_price(0.0),
          entry_bar_id(0), exit_bar_id(0), exit_bar_index(0) {}

    TradeRecord(double p, double pct)
        : pnl(p), pnl_pct(pct), shares(0), entry_price(0.0), exit_price(0.0),
          entry_bar_id(0), exit_bar_id(0), exit_bar_index(0) {}

    TradeRecord(double p, double pct, Timestamp entry, Timestamp exit,
                const Symbol& sym, int sh, Price entry_pr, Price exit_pr,
                uint64_t entry_bid = 0, uint64_t exit_bid = 0, size_t exit_idx = 0)
        : pnl(p), pnl_pct(pct), entry_time(entry), exit_time(exit),
          symbol(sym), shares(sh), entry_price(entry_pr), exit_price(exit_pr),
          entry_bar_id(entry_bid), exit_bar_id(exit_bid), exit_bar_index(exit_idx) {}

    bool is_win() const { return pnl > 0; }
    bool is_loss() const { return pnl < 0; }
};

/**
 * Trade History - Circular buffer of recent trades
 * Used for adaptive position sizing and performance tracking
 */
using TradeHistory = CircularBuffer<TradeRecord>;

} // namespace trading
