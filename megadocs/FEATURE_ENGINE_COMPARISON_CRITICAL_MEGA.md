# FEATURE_ENGINE_COMPARISON_CRITICAL - Complete Analysis

**Generated**: 2025-10-16 13:51:41
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: /Volumes/ExternalSSD/Dev/C++/online_trader/FEATURE_ENGINE_COMPARISON_CRITICAL.md
**Total Files**: 1

---

## 📋 **TABLE OF CONTENTS**

1. [src/features/unified_feature_engine.cpp](#file-1)

---

## 📄 **FILE 1 of 1**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`
- **Size**: 610 lines
- **Modified**: 2025-10-16 13:14:00
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "features/unified_feature_engine.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

// OpenSSL for SHA1 hashing (already in dependencies)
#include <openssl/sha.h>

namespace sentio {
namespace features {

// =============================================================================
// SHA1 Hash Utility
// =============================================================================

std::string sha1_hex(const std::string& s) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(s.data()), s.size(), hash);

    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (unsigned char c : hash) {
        os << std::setw(2) << static_cast<int>(c);
    }
    return os.str();
}

// =============================================================================
// UnifiedFeatureEngineV2 Implementation
// =============================================================================

UnifiedFeatureEngine::UnifiedFeatureEngine(EngineConfig cfg)
    : cfg_(cfg),
      rsi14_(cfg.rsi14),
      rsi21_(cfg.rsi21),
      atr14_(cfg.atr14),
      bb20_(cfg.bb20, cfg.bb_k),
      stoch14_(cfg.stoch14),
      will14_(cfg.will14),
      macd_(),  // Uses default periods 12/26/9
      roc5_(cfg.roc5),
      roc10_(cfg.roc10),
      roc20_(cfg.roc20),
      cci20_(cfg.cci20),
      don20_(cfg.don20),
      keltner_(cfg.keltner_ema, cfg.keltner_atr, cfg.keltner_mult),
      obv_(),
      vwap_(),
      ema10_(cfg.ema10),
      ema20_(cfg.ema20),
      ema50_(cfg.ema50),
      sma10_ring_(cfg.sma10),
      sma20_ring_(cfg.sma20),
      sma50_ring_(cfg.sma50),
      scaler_(cfg.robust ? Scaler::Type::ROBUST : Scaler::Type::STANDARD)
{
    build_schema_();
    feats_.assign(schema_.names.size(), std::numeric_limits<double>::quiet_NaN());
}

void UnifiedFeatureEngine::build_schema_() {
    std::vector<std::string> n;

    // ==========================================================================
    // Time features (cyclical encoding for intraday patterns)
    // ==========================================================================
    if (cfg_.time) {
        n.push_back("time.hour_sin");
        n.push_back("time.hour_cos");
        n.push_back("time.minute_sin");
        n.push_back("time.minute_cos");
        n.push_back("time.dow_sin");
        n.push_back("time.dow_cos");
        n.push_back("time.dom_sin");
        n.push_back("time.dom_cos");
    }

    // ==========================================================================
    // Core price/volume features (NORMALIZED - always included)
    // ==========================================================================
    n.push_back("price.range_ratio");      // (high - low) / close
    n.push_back("price.body_ratio");       // (close - open) / close
    n.push_back("price.upper_wick_ratio"); // (high - close) / close
    n.push_back("price.lower_wick_ratio"); // (close - low) / close
    n.push_back("price.return_1");         // 1-bar return
    n.push_back("volume.change_ratio");    // volume change vs previous

    // ==========================================================================
    // Moving Averages (DEVIATION RATIOS - always included for baseline)
    // ==========================================================================
    n.push_back("sma10_dev");       // (close - sma10) / sma10
    n.push_back("sma20_dev");       // (close - sma20) / sma20
    n.push_back("sma50_dev");       // (close - sma50) / sma50
    n.push_back("ema10_dev");       // (close - ema10) / ema10
    n.push_back("ema20_dev");       // (close - ema20) / ema20
    n.push_back("ema50_dev");       // (close - ema50) / ema50
    n.push_back("price_vs_sma20");  // (close - sma20) / sma20 (duplicate for compatibility)
    n.push_back("price_vs_ema20");  // (close - ema20) / ema20 (duplicate for compatibility)

    // ==========================================================================
    // Volatility Features (NORMALIZED)
    // ==========================================================================
    if (cfg_.volatility) {
        n.push_back("atr14_pct");              // ATR / close
        n.push_back("bb20.mean_dev");          // (close - bb_mean) / close
        n.push_back("bb20.sd_pct");            // bb_sd / close
        n.push_back("bb20.upper_dev");         // (close - bb_upper) / close
        n.push_back("bb20.lower_dev");         // (close - bb_lower) / close
        n.push_back("bb20.percent_b");         // Already ratio
        n.push_back("bb20.bandwidth");         // Already ratio
        n.push_back("keltner.middle_dev");     // (close - keltner_mid) / close
        n.push_back("keltner.upper_dev");      // (close - keltner_up) / close
        n.push_back("keltner.lower_dev");      // (close - keltner_dn) / close
    }

    // ==========================================================================
    // Momentum Features
    // ==========================================================================
    if (cfg_.momentum) {
        n.push_back("rsi14");
        n.push_back("rsi21");
        n.push_back("stoch14.k");
        n.push_back("stoch14.d");
        n.push_back("stoch14.slow");
        n.push_back("will14");
        n.push_back("macd.line");
        n.push_back("macd.signal");
        n.push_back("macd.hist");
        n.push_back("roc5");
        n.push_back("roc10");
        n.push_back("roc20");
        n.push_back("cci20");
    }

    // ==========================================================================
    // Volume Features (NORMALIZED)
    // ==========================================================================
    if (cfg_.volume) {
        n.push_back("obv_scaled");       // OBV / (close * 1M)
        n.push_back("vwap_dist");        // (close - vwap) / vwap
    }

    // ==========================================================================
    // Donchian Channels (NORMALIZED as deviations)
    // ==========================================================================
    n.push_back("don20.up_dev");         // (close - don_up) / close
    n.push_back("don20.mid_dev");        // (close - don_mid) / close
    n.push_back("don20.dn_dev");         // (close - don_dn) / close
    n.push_back("don20.position");       // Already ratio  // (close - dn) / (up - dn)

    // ==========================================================================
    // Candlestick Pattern Features (from v1.0)
    // ==========================================================================
    if (cfg_.patterns) {
        n.push_back("pattern.doji");           // Body < 10% of range
        n.push_back("pattern.hammer");         // Lower shadow > 2x body
        n.push_back("pattern.shooting_star");  // Upper shadow > 2x body
        n.push_back("pattern.engulfing_bull"); // Bullish engulfing
        n.push_back("pattern.engulfing_bear"); // Bearish engulfing
    }

    // ==========================================================================
    // Finalize schema and compute hash
    // ==========================================================================
    schema_.names = std::move(n);

    // Concatenate names and critical config for hash
    std::ostringstream cat;
    for (const auto& name : schema_.names) {
        cat << name << "\n";
    }
    cat << "cfg:"
        << cfg_.rsi14 << ","
        << cfg_.bb20 << ","
        << cfg_.bb_k << ","
        << cfg_.macd_fast << ","
        << cfg_.macd_slow << ","
        << cfg_.macd_sig;

    schema_.sha1_hash = sha1_hex(cat.str());
}

bool UnifiedFeatureEngine::update(const Bar& b) {
    // ==========================================================================
    // Update all indicators (O(1) incremental)
    // ==========================================================================

    // Volatility
    atr14_.update(b.high, b.low, b.close);
    bb20_.update(b.close);
    keltner_.update(b.high, b.low, b.close);

    // Momentum
    rsi14_.update(b.close);
    rsi21_.update(b.close);
    stoch14_.update(b.high, b.low, b.close);
    will14_.update(b.high, b.low, b.close);
    macd_.update(b.close);
    roc5_.update(b.close);
    roc10_.update(b.close);
    roc20_.update(b.close);
    cci20_.update(b.high, b.low, b.close);

    // Channels
    don20_.update(b.high, b.low);

    // Volume
    obv_.update(b.close, b.volume);
    vwap_.update(b.close, b.volume);

    // Moving averages
    ema10_.update(b.close);
    ema20_.update(b.close);
    ema50_.update(b.close);
    sma10_ring_.push(b.close);
    sma20_ring_.push(b.close);
    sma50_ring_.push(b.close);

    // Store previous close and volume BEFORE updating (for 1-bar return calculation)
    prevPrevClose_ = prevClose_;
    prevPrevVolume_ = prevVolume_;

    // Calculate and store 1-bar return for volatility calculation
    if (!std::isnan(prevClose_) && prevClose_ > 0.0) {
        double bar_return = (b.close - prevClose_) / prevClose_;
        recent_returns_.push_back(bar_return);

        // Keep only last MAX_RETURNS_HISTORY returns
        if (recent_returns_.size() > MAX_RETURNS_HISTORY) {
            recent_returns_.pop_front();
        }
    }

    // Store current bar values for derived features
    prevTimestamp_ = b.timestamp_ms;
    prevClose_ = b.close;
    prevOpen_ = b.open;
    prevHigh_ = b.high;
    prevLow_ = b.low;
    prevVolume_ = b.volume;

    // Recompute feature vector
    recompute_vector_();

    seeded_ = true;
    ++bar_count_;
    return true;
}

void UnifiedFeatureEngine::recompute_vector_() {
    size_t k = 0;

    // ==========================================================================
    // Time features (cyclical encoding from v1.0)
    // ==========================================================================
    if (cfg_.time && prevTimestamp_ > 0) {
        time_t timestamp = prevTimestamp_ / 1000;
        struct tm* time_info = gmtime(&timestamp);

        if (time_info) {
            double hour = time_info->tm_hour;
            double minute = time_info->tm_min;
            double day_of_week = time_info->tm_wday;     // 0-6 (Sunday=0)
            double day_of_month = time_info->tm_mday;    // 1-31

            // Cyclical encoding (sine/cosine to preserve continuity)
            feats_[k++] = std::sin(2.0 * M_PI * hour / 24.0);           // hour_sin
            feats_[k++] = std::cos(2.0 * M_PI * hour / 24.0);           // hour_cos
            feats_[k++] = std::sin(2.0 * M_PI * minute / 60.0);         // minute_sin
            feats_[k++] = std::cos(2.0 * M_PI * minute / 60.0);         // minute_cos
            feats_[k++] = std::sin(2.0 * M_PI * day_of_week / 7.0);     // dow_sin
            feats_[k++] = std::cos(2.0 * M_PI * day_of_week / 7.0);     // dow_cos
            feats_[k++] = std::sin(2.0 * M_PI * day_of_month / 31.0);   // dom_sin
            feats_[k++] = std::cos(2.0 * M_PI * day_of_month / 31.0);   // dom_cos
        } else {
            // If time parsing fails, fill with NaN
            for (int i = 0; i < 8; ++i) {
                feats_[k++] = std::numeric_limits<double>::quiet_NaN();
            }
        }
    }

    // ==========================================================================
    // Core price/volume (NORMALIZED RATIOS - not raw prices!)
    // ==========================================================================
    // Range/Close ratio (typical: 0.01-0.05 for 1-5% range)
    double range = prevHigh_ - prevLow_;
    feats_[k++] = (prevClose_ != 0) ? range / prevClose_ : 0.0;

    // Body/Close ratio (typical: -0.02 to +0.02 for 2% moves)
    feats_[k++] = (prevClose_ != 0) ? (prevClose_ - prevOpen_) / prevClose_ : 0.0;

    // High/Close ratio (upper wick strength)
    feats_[k++] = (prevClose_ != 0) ? (prevHigh_ - prevClose_) / prevClose_ : 0.0;

    // Low/Close ratio (lower wick strength)
    feats_[k++] = (prevClose_ != 0) ? (prevClose_ - prevLow_) / prevClose_ : 0.0;

    // 1-bar return (already normalized - KEEP)
    feats_[k++] = safe_return(prevClose_, prevPrevClose_);

    // Volume change ratio (typical: -0.5 to +2.0)
    feats_[k++] = (!std::isnan(prevPrevVolume_) && prevPrevVolume_ > 0)
                  ? (prevVolume_ / prevPrevVolume_) - 1.0
                  : 0.0;

    // ==========================================================================
    // Moving Averages (DEVIATION RATIOS - not raw MA values!)
    // ==========================================================================
    double sma10 = sma10_ring_.full() ? sma10_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double sma20 = sma20_ring_.full() ? sma20_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double sma50 = sma50_ring_.full() ? sma50_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double ema10 = ema10_.get_value();
    double ema20 = ema20_.get_value();
    double ema50 = ema50_.get_value();

    // Price deviation from MAs (typical: -0.05 to +0.05 for 5% deviation)
    feats_[k++] = (!std::isnan(sma10) && sma10 != 0) ? (prevClose_ - sma10) / sma10 : 0.0;
    feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : 0.0;
    feats_[k++] = (!std::isnan(sma50) && sma50 != 0) ? (prevClose_ - sma50) / sma50 : 0.0;
    feats_[k++] = (!std::isnan(ema10) && ema10 != 0) ? (prevClose_ - ema10) / ema10 : 0.0;
    feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : 0.0;
    feats_[k++] = (!std::isnan(ema50) && ema50 != 0) ? (prevClose_ - ema50) / ema50 : 0.0;

    // Additional MA cross ratios (already using deviations - KEEP)
    feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : std::numeric_limits<double>::quiet_NaN();

    // ==========================================================================
    // Volatility (NORMALIZED)
    // ==========================================================================
    if (cfg_.volatility) {
        // ATR as percentage of price (typical: 0.01-0.05)
        feats_[k++] = (prevClose_ != 0 && !std::isnan(atr14_.value)) ? atr14_.value / prevClose_ : std::numeric_limits<double>::quiet_NaN();

        // Debug BB NaN issue - check Welford stats when BB produces NaN
        if (bar_count_ > 100 && std::isnan(bb20_.sd)) {
            static int late_nan_count = 0;
            if (late_nan_count < 10) {
                std::cerr << "[FeatureEngine CRITICAL] BB.sd is NaN!"
                          << " bar_count=" << bar_count_
                          << ", bb20_.win.size=" << bb20_.win.size()
                          << ", bb20_.win.capacity=" << bb20_.win.capacity()
                          << ", bb20_.win.full=" << bb20_.win.full()
                          << ", bb20_.win.welford_n=" << bb20_.win.welford_n()
                          << ", bb20_.win.welford_m2=" << bb20_.win.welford_m2()
                          << ", bb20_.win.variance=" << bb20_.win.variance()
                          << ", bb20_.is_ready=" << bb20_.is_ready()
                          << ", bb20_.mean=" << bb20_.mean
                          << ", bb20_.sd=" << bb20_.sd
                          << ", prevClose=" << prevClose_ << std::endl;
                late_nan_count++;
            }
        }

        size_t bb_start_idx = k;
        // Normalize BB bands as deviations from current price
        feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.mean)) ? (prevClose_ - bb20_.mean) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.sd)) ? bb20_.sd / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.upper)) ? (prevClose_ - bb20_.upper) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(bb20_.lower)) ? (prevClose_ - bb20_.lower) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = bb20_.percent_b;  // Already a ratio (0-1)
        feats_[k++] = bb20_.bandwidth;  // Already a ratio

        // Debug: Check if any BB features are NaN after assignment
        if (bar_count_ > 100) {
            static int bb_assign_debug = 0;
            if (bb_assign_debug < 3) {
                std::cerr << "[FeatureEngine] BB features assigned at indices " << bb_start_idx << "-" << (k-1)
                          << ", bb20_.mean=" << bb20_.mean
                          << ", bb20_.sd=" << bb20_.sd
                          << ", feats_[" << bb_start_idx << "]=" << feats_[bb_start_idx]
                          << ", feats_[" << (bb_start_idx+1) << "]=" << feats_[bb_start_idx+1]
                          << std::endl;
                bb_assign_debug++;
            }
        }
        // Normalize Keltner channels as deviations from current price
        feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.middle)) ? (prevClose_ - keltner_.middle) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.upper)) ? (prevClose_ - keltner_.upper) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = (prevClose_ != 0 && !std::isnan(keltner_.lower)) ? (prevClose_ - keltner_.lower) / prevClose_ : std::numeric_limits<double>::quiet_NaN();
    }

    // ==========================================================================
    // Momentum
    // ==========================================================================
    if (cfg_.momentum) {
        feats_[k++] = rsi14_.value;
        feats_[k++] = rsi21_.value;
        feats_[k++] = stoch14_.k;
        feats_[k++] = stoch14_.d;
        feats_[k++] = stoch14_.slow;
        feats_[k++] = will14_.r;
        feats_[k++] = macd_.macd;
        feats_[k++] = macd_.signal;
        feats_[k++] = macd_.hist;
        feats_[k++] = roc5_.value;
        feats_[k++] = roc10_.value;
        feats_[k++] = roc20_.value;
        feats_[k++] = cci20_.value;
    }

    // ==========================================================================
    // Volume
    // ==========================================================================
    if (cfg_.volume) {
        // OBV scaled by (price * 1M) to normalize magnitude (typical: -0.01 to +0.01)
        feats_[k++] = (prevClose_ != 0 && !std::isnan(obv_.value))
                      ? obv_.value / (prevClose_ * 1000000.0)
                      : std::numeric_limits<double>::quiet_NaN();

        // VWAP distance as ratio (typical: -0.02 to +0.02 for 2% deviation)
        double vwap_dist = (!std::isnan(vwap_.value) && vwap_.value != 0)
                           ? (prevClose_ - vwap_.value) / vwap_.value
                           : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = vwap_dist;
    }

    // ==========================================================================
    // Donchian
    // ==========================================================================
    // Donchian bands as price deviations (typical: -0.05 to +0.05 for 5% from extremes)
    feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.up))
                  ? (prevClose_ - don20_.up) / prevClose_
                  : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.mid))
                  ? (prevClose_ - don20_.mid) / prevClose_
                  : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = (prevClose_ != 0 && !std::isnan(don20_.dn))
                  ? (prevClose_ - don20_.dn) / prevClose_
                  : std::numeric_limits<double>::quiet_NaN();

    // Donchian position: (close - dn) / (up - dn) - already normalized ratio (0 to 1)
    double don_range = don20_.up - don20_.dn;
    double don_pos = (don_range != 0 && !std::isnan(don20_.up) && !std::isnan(don20_.dn))
                     ? (prevClose_ - don20_.dn) / don_range
                     : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = don_pos;

    // ==========================================================================
    // Candlestick Pattern Features (from v1.0)
    // ==========================================================================
    if (cfg_.patterns) {
        double range = prevHigh_ - prevLow_;
        double body = std::abs(prevClose_ - prevOpen_);
        double upper_shadow = prevHigh_ - std::max(prevOpen_, prevClose_);
        double lower_shadow = std::min(prevOpen_, prevClose_) - prevLow_;

        // Doji: body < 10% of range
        bool is_doji = (range > 0) && (body / range < 0.1);
        feats_[k++] = is_doji ? 1.0 : 0.0;

        // Hammer: lower shadow > 2x body, upper shadow < body
        bool is_hammer = (lower_shadow > 2.0 * body) && (upper_shadow < body);
        feats_[k++] = is_hammer ? 1.0 : 0.0;

        // Shooting star: upper shadow > 2x body, lower shadow < body
        bool is_shooting_star = (upper_shadow > 2.0 * body) && (lower_shadow < body);
        feats_[k++] = is_shooting_star ? 1.0 : 0.0;

        // Engulfing patterns require previous bar - use prevPrevClose_
        bool engulfing_bull = false;
        bool engulfing_bear = false;
        if (!std::isnan(prevPrevClose_)) {
            bool prev_bearish = prevPrevClose_ < prevOpen_;  // Prev bar was bearish
            bool curr_bullish = prevClose_ > prevOpen_;       // Current bar is bullish
            bool engulfs = (prevOpen_ < prevPrevClose_) && (prevClose_ > prevOpen_);
            engulfing_bull = prev_bearish && curr_bullish && engulfs;

            bool prev_bullish = prevPrevClose_ > prevOpen_;
            bool curr_bearish = prevClose_ < prevOpen_;
            engulfs = (prevOpen_ > prevPrevClose_) && (prevClose_ < prevOpen_);
            engulfing_bear = prev_bullish && curr_bearish && engulfs;
        }
        feats_[k++] = engulfing_bull ? 1.0 : 0.0;
        feats_[k++] = engulfing_bear ? 1.0 : 0.0;
    }
}

int UnifiedFeatureEngine::warmup_remaining() const {
    // Conservative: max lookback across all indicators
    int max_period = std::max({
        cfg_.rsi14, cfg_.rsi21, cfg_.atr14, cfg_.bb20,
        cfg_.stoch14, cfg_.will14, cfg_.macd_slow, cfg_.don20,
        cfg_.sma50, cfg_.ema50
    });

    // Need at least max_period + 1 bars for all indicators to be valid
    int required_bars = max_period + 1;
    return std::max(0, required_bars - static_cast<int>(bar_count_));
}

std::vector<std::string> UnifiedFeatureEngine::get_unready_indicators() const {
    std::vector<std::string> unready;

    // Check each indicator's readiness
    if (!bb20_.is_ready()) unready.push_back("BB20");
    if (!rsi14_.is_ready()) unready.push_back("RSI14");
    if (!rsi21_.is_ready()) unready.push_back("RSI21");
    if (!atr14_.is_ready()) unready.push_back("ATR14");
    if (!stoch14_.is_ready()) unready.push_back("Stoch14");
    if (!will14_.is_ready()) unready.push_back("Will14");
    if (!don20_.is_ready()) unready.push_back("Don20");

    // Check moving averages
    if (bar_count_ < static_cast<size_t>(cfg_.sma10)) unready.push_back("SMA10");
    if (bar_count_ < static_cast<size_t>(cfg_.sma20)) unready.push_back("SMA20");
    if (bar_count_ < static_cast<size_t>(cfg_.sma50)) unready.push_back("SMA50");
    if (bar_count_ < static_cast<size_t>(cfg_.ema10)) unready.push_back("EMA10");
    if (bar_count_ < static_cast<size_t>(cfg_.ema20)) unready.push_back("EMA20");
    if (bar_count_ < static_cast<size_t>(cfg_.ema50)) unready.push_back("EMA50");

    return unready;
}

void UnifiedFeatureEngine::reset() {
    *this = UnifiedFeatureEngine(cfg_);
}

std::string UnifiedFeatureEngine::serialize() const {
    std::ostringstream os;
    os << std::setprecision(17);

    os << "prevTimestamp " << prevTimestamp_ << "\n";
    os << "prevClose " << prevClose_ << "\n";
    os << "prevPrevClose " << prevPrevClose_ << "\n";
    os << "prevOpen " << prevOpen_ << "\n";
    os << "prevHigh " << prevHigh_ << "\n";
    os << "prevLow " << prevLow_ << "\n";
    os << "prevVolume " << prevVolume_ << "\n";
    os << "bar_count " << bar_count_ << "\n";
    os << "obv " << obv_.value << "\n";
    os << "vwap " << vwap_.sumPV << " " << vwap_.sumV << "\n";

    // Add EMA/indicator states if exact resume needed
    // (Omitted for brevity; can be extended)

    return os.str();
}

void UnifiedFeatureEngine::restore(const std::string& blob) {
    reset();

    std::istringstream is(blob);
    std::string key;

    while (is >> key) {
        if (key == "prevTimestamp") is >> prevTimestamp_;
        else if (key == "prevClose") is >> prevClose_;
        else if (key == "prevPrevClose") is >> prevPrevClose_;
        else if (key == "prevOpen") is >> prevOpen_;
        else if (key == "prevHigh") is >> prevHigh_;
        else if (key == "prevLow") is >> prevLow_;
        else if (key == "prevVolume") is >> prevVolume_;
        else if (key == "bar_count") is >> bar_count_;
        else if (key == "obv") is >> obv_.value;
        else if (key == "vwap") is >> vwap_.sumPV >> vwap_.sumV;
    }
}

double UnifiedFeatureEngine::get_realized_volatility(int lookback) const {
    if (recent_returns_.empty() || static_cast<int>(recent_returns_.size()) < lookback) {
        return 0.0;  // Insufficient data
    }

    // Calculate standard deviation of returns over lookback period
    double sum = 0.0;
    int count = 0;

    // Get the last 'lookback' returns
    auto it = recent_returns_.rbegin();
    while (count < lookback && it != recent_returns_.rend()) {
        sum += *it;
        ++count;
        ++it;
    }

    double mean = sum / count;

    // Calculate variance
    double sum_sq_diff = 0.0;
    it = recent_returns_.rbegin();
    count = 0;
    while (count < lookback && it != recent_returns_.rend()) {
        double diff = *it - mean;
        sum_sq_diff += diff * diff;
        ++count;
        ++it;
    }

    double variance = sum_sq_diff / (count - 1);  // Sample variance
    return std::sqrt(variance);  // Standard deviation
}

double UnifiedFeatureEngine::get_annualized_volatility() const {
    double realized_vol = get_realized_volatility(20);  // 20-bar lookback

    // Annualize: volatility * sqrt(minutes per year)
    // Assuming 1-minute bars, 390 minutes/day, 252 trading days/year
    // Total minutes per year = 390 * 252 = 98,280
    double annualization_factor = std::sqrt(390.0 * 252.0);

    return realized_vol * annualization_factor;
}

} // namespace features
} // namespace sentio

```

