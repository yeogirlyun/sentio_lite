# BUG_REPORT_NO_TRADES_EXECUTED_MEGA - Complete Analysis

**Generated**: 2025-10-17 11:20:11
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/sentio_lite
**Source**: /Volumes/ExternalSSD/Dev/C++/sentio_lite/megadocs/BUG_REPORT_NO_TRADES_EXECUTED_MEGA.md
**Total Files**: 19

---

## 📋 **TABLE OF CONTENTS**

1. [build/generate_dashboard.py](#file-1)
2. [build/results.json](#file-2)
3. [include/core/bar.h](#file-3)
4. [include/core/math_utils.h](#file-4)
5. [include/core/types.h](#file-5)
6. [include/predictor/feature_extractor.h](#file-6)
7. [include/predictor/online_predictor.h](#file-7)
8. [include/trading/multi_symbol_trader.h](#file-8)
9. [include/trading/position.h](#file-9)
10. [include/trading/trade_history.h](#file-10)
11. [include/trading/trading_mode.h](#file-11)
12. [include/utils/circular_buffer.h](#file-12)
13. [include/utils/data_loader.h](#file-13)
14. [include/utils/date_filter.h](#file-14)
15. [include/utils/results_exporter.h](#file-15)
16. [src/main.cpp](#file-16)
17. [src/predictor/feature_extractor.cpp](#file-17)
18. [src/predictor/online_predictor.cpp](#file-18)
19. [src/trading/multi_symbol_trader.cpp](#file-19)

---

## 📄 **FILE 1 of 19**: build/generate_dashboard.py

**File Information**:
- **Path**: `build/generate_dashboard.py`
- **Size**: 339 lines
- **Modified**: 2025-10-17 10:42:32
- **Type**: py
- **Permissions**: -rw-r--r--

```text
#!/usr/bin/env python3
"""
Dashboard Generator Wrapper for Sentio Lite
Converts results.json to trades.jsonl and generates HTML dashboard
"""

import json
import sys
import os
import subprocess
from datetime import datetime
from pathlib import Path

def convert_results_to_trades(results_path, trades_path):
    """Convert results.json to trades.jsonl format"""
    with open(results_path) as f:
        results = json.load(f)

    # For now, create empty trades file if no trades
    # In future, we'll export actual trades from MultiSymbolTrader
    trades = []

    with open(trades_path, 'w') as f:
        for trade in trades:
            f.write(json.dumps(trade) + '\n')

    return trades_path

def generate_dashboard(results_path, output_path=None):
    """Generate HTML dashboard using rotation_trading_dashboard_html.py"""

    # Load results
    with open(results_path) as f:
        results = json.load(f)

    metadata = results.get('metadata', {})

    # Create output path with timestamp if not provided
    if output_path is None:
        # Create logs/dashboard directory
        logs_dir = Path('../logs/dashboard')
        logs_dir.mkdir(parents=True, exist_ok=True)

        # Generate filename with timestamp
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        test_date = metadata.get('start_date', 'unknown').replace('-', '')
        mode = metadata.get('mode', 'mock').lower()
        output_path = logs_dir / f'dashboard_{mode}_{test_date}_{timestamp}.html'
    else:
        # Ensure parent directory exists
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)

    # Convert to trades.jsonl (temporary file)
    trades_path = 'trades_temp.jsonl'
    convert_results_to_trades(results_path, trades_path)

    # Call rotation_trading_dashboard_html.py
    script_path = '../scripts/rotation_trading_dashboard_html.py'

    try:
        cmd = [
            'python3', script_path,
            '--trades', trades_path,
            '--output', str(output_path),
            '--data-dir', '../data',
            '--start-equity', str(metadata.get('initial_capital', 100000))
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)

        # Clean up temp file
        if os.path.exists(trades_path):
            os.remove(trades_path)

        if result.returncode != 0:
            # If rotation dashboard fails, fall back to simple dashboard
            print(f"⚠️  Advanced dashboard failed, using simple dashboard...")
            print(f"   Error: {result.stderr}")
            generate_simple_dashboard(results, output_path)
        else:
            print(f"\n✅ Dashboard generated: {output_path}")
            print(f"   View at: file://{os.path.abspath(output_path)}\n")

    except Exception as e:
        # Fallback to simple dashboard
        print(f"⚠️  Dashboard generation error: {e}")
        print(f"   Using simple dashboard instead...")
        if os.path.exists(trades_path):
            os.remove(trades_path)
        generate_simple_dashboard(results, output_path)

    return str(output_path)

def generate_simple_dashboard(results, output_path):
    """Generate simple HTML dashboard as fallback"""

    metadata = results.get('metadata', {})
    performance = results.get('performance', {})
    config = results.get('config', {})

    html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Sentio Lite - Mock Test Results</title>
    <style>
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            max-width: 1200px;
            margin: 40px auto;
            padding: 20px;
            background: #f5f5f5;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            margin-bottom: 30px;
        }}
        .header h1 {{
            margin: 0 0 10px 0;
        }}
        .header .subtitle {{
            opacity: 0.9;
            font-size: 14px;
        }}
        .section {{
            background: white;
            padding: 25px;
            margin-bottom: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        .section h2 {{
            margin-top: 0;
            color: #333;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }}
        .metric-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }}
        .metric {{
            text-align: center;
        }}
        .metric-label {{
            font-size: 12px;
            color: #666;
            text-transform: uppercase;
            letter-spacing: 1px;
        }}
        .metric-value {{
            font-size: 28px;
            font-weight: bold;
            color: #333;
            margin-top: 5px;
        }}
        .metric-value.positive {{
            color: #10b981;
        }}
        .metric-value.negative {{
            color: #ef4444;
        }}
        .metric-value.neutral {{
            color: #6b7280;
        }}
        .config-table {{
            width: 100%;
            border-collapse: collapse;
        }}
        .config-table td {{
            padding: 10px;
            border-bottom: 1px solid #e5e7eb;
        }}
        .config-table td:first-child {{
            font-weight: 600;
            color: #666;
            width: 200px;
        }}
        .alert {{
            background: #fef3c7;
            border-left: 4px solid #f59e0b;
            padding: 15px;
            margin: 20px 0;
            border-radius: 4px;
        }}
        .alert h3 {{
            margin: 0 0 10px 0;
            color: #92400e;
        }}
        .footer {{
            text-align: center;
            color: #666;
            font-size: 12px;
            margin-top: 40px;
        }}
    </style>
</head>
<body>
    <div class="header">
        <h1>📊 Sentio Lite - Mock Test Results</h1>
        <div class="subtitle">
            Mode: {metadata.get('mode', 'N/A')} |
            Test Date: {metadata.get('start_date', 'N/A')} |
            Generated: {metadata.get('timestamp', 'N/A')}
        </div>
    </div>

    <div class="section">
        <h2>Performance Summary</h2>
        <div class="metric-grid">
            <div class="metric">
                <div class="metric-label">Initial Capital</div>
                <div class="metric-value">${metadata.get('initial_capital', 0):,.2f}</div>
            </div>
            <div class="metric">
                <div class="metric-label">Final Equity</div>
                <div class="metric-value">${performance.get('final_equity', 0):,.2f}</div>
            </div>
            <div class="metric">
                <div class="metric-label">Total Return</div>
                <div class="metric-value {"positive" if performance.get("total_return", 0) > 0 else "negative" if performance.get("total_return", 0) < 0 else "neutral"}">
                    {performance.get('total_return', 0) * 100:+.2f}%
                </div>
            </div>
            <div class="metric">
                <div class="metric-label">MRD (Daily Return)</div>
                <div class="metric-value {"positive" if performance.get("mrd", 0) > 0 else "negative" if performance.get("mrd", 0) < 0 else "neutral"}">
                    {performance.get('mrd', 0) * 100:+.2f}%
                </div>
            </div>
        </div>
    </div>

    <div class="section">
        <h2>Trade Statistics</h2>
        <div class="metric-grid">
            <div class="metric">
                <div class="metric-label">Total Trades</div>
                <div class="metric-value">{performance.get('total_trades', 0)}</div>
            </div>
            <div class="metric">
                <div class="metric-label">Winning Trades</div>
                <div class="metric-value positive">{performance.get('winning_trades', 0)}</div>
            </div>
            <div class="metric">
                <div class="metric-label">Losing Trades</div>
                <div class="metric-value negative">{performance.get('losing_trades', 0)}</div>
            </div>
            <div class="metric">
                <div class="metric-label">Win Rate</div>
                <div class="metric-value">{performance.get('win_rate', 0) * 100:.1f}%</div>
            </div>
            <div class="metric">
                <div class="metric-label">Average Win</div>
                <div class="metric-value positive">${performance.get('avg_win', 0):.2f}</div>
            </div>
            <div class="metric">
                <div class="metric-label">Average Loss</div>
                <div class="metric-value negative">${performance.get('avg_loss', 0):.2f}</div>
            </div>
            <div class="metric">
                <div class="metric-label">Profit Factor</div>
                <div class="metric-value">{performance.get('profit_factor', 0):.2f}</div>
            </div>
            <div class="metric">
                <div class="metric-label">Max Drawdown</div>
                <div class="metric-value negative">{performance.get('max_drawdown', 0) * 100:.2f}%</div>
            </div>
        </div>
    </div>

    {"<div class='alert'><h3>⚠️ No Trades Executed</h3><p>The system completed warmup but did not execute any trades. This could be due to:</p><ul><li>Predictions below minimum threshold</li><li>Market conditions not favorable</li><li>Insufficient trading period after warmup</li></ul><p><strong>Recommendation:</strong> Try testing with a longer period or different parameters.</p></div>" if performance.get('total_trades', 0) == 0 else ""}

    <div class="section">
        <h2>Configuration</h2>
        <table class="config-table">
            <tr>
                <td>Symbols</td>
                <td>{metadata.get('symbols', 'N/A')}</td>
            </tr>
            <tr>
                <td>Test Period</td>
                <td>{metadata.get('start_date', 'N/A')} to {metadata.get('end_date', 'N/A')}</td>
            </tr>
            <tr>
                <td>Max Positions</td>
                <td>{config.get('max_positions', 0)}</td>
            </tr>
            <tr>
                <td>Stop Loss</td>
                <td>{config.get('stop_loss_pct', 0) * 100:.2f}%</td>
            </tr>
            <tr>
                <td>Profit Target</td>
                <td>{config.get('profit_target_pct', 0) * 100:.2f}%</td>
            </tr>
            <tr>
                <td>EWRLS Lambda</td>
                <td>{config.get('lambda', 0):.3f}</td>
            </tr>
            <tr>
                <td>Warmup Bars</td>
                <td>{config.get('min_bars_to_learn', 0)} bars</td>
            </tr>
            <tr>
                <td>Bars Per Day</td>
                <td>{config.get('bars_per_day', 0)} bars</td>
            </tr>
        </table>
    </div>

    <div class="footer">
        Generated by Sentio Lite v1.0 | {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
    </div>
</body>
</html>
"""

    with open(output_path, 'w') as f:
        f.write(html)

    print(f"\n✅ Dashboard generated: {output_path}")
    print(f"   View at: file://{os.path.abspath(output_path)}\n")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 generate_dashboard.py results.json [output.html]")
        print("\nIf output.html is not specified, dashboard will be saved to:")
        print("  ../logs/dashboard/dashboard_<mode>_<date>_<timestamp>.html")
        sys.exit(1)

    results_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None

    generate_dashboard(results_path, output_path)

```

## 📄 **FILE 2 of 19**: build/results.json

**File Information**:
- **Path**: `build/results.json`
- **Size**: 31 lines
- **Modified**: 2025-10-17 10:48:42
- **Type**: json
- **Permissions**: -rw-r--r--

```text
{
  "metadata": {
    "timestamp": "2025-10-17 10:48:42",
    "mode": "MOCK",
    "symbols": "TQQQ,SQQQ,SSO,SDS,TNA,TZA,FAS,FAZ,UVXY,SVXY",
    "start_date": "2024-10-16",
    "end_date": "2024-10-16",
    "initial_capital": 100000.0000
  },
  "performance": {
    "final_equity": 100000.0000,
    "total_return": 0.0000,
    "mrd": 0.0000,
    "total_trades": 0,
    "winning_trades": 0,
    "losing_trades": 0,
    "win_rate": 0.0000,
    "avg_win": 0.0000,
    "avg_loss": 0.0000,
    "profit_factor": 0.0000,
    "max_drawdown": 0.0000
  },
  "config": {
    "max_positions": 3,
    "stop_loss_pct": -0.0200,
    "profit_target_pct": 0.0500,
    "lambda": 0.9800,
    "min_bars_to_learn": 1170,
    "bars_per_day": 390
  }
}

```

## 📄 **FILE 3 of 19**: include/core/bar.h

**File Information**:
- **Path**: `include/core/bar.h`
- **Size**: 24 lines
- **Modified**: 2025-10-17 09:28:21
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "types.h"

namespace trading {

struct Bar {
    Timestamp timestamp;
    Price open;
    Price high;
    Price low;
    Price close;
    Volume volume;

    Bar() = default;
    Bar(Timestamp ts, Price o, Price h, Price l, Price c, Volume v)
        : timestamp(ts), open(o), high(h), low(l), close(c), volume(v) {}

    // Convenience constructor with timestamp_ms
    Bar(int64_t ts_ms, Price o, Price h, Price l, Price c, Volume v)
        : timestamp(from_timestamp_ms(ts_ms))
        , open(o), high(h), low(l), close(c), volume(v) {}
};

} // namespace trading

```

## 📄 **FILE 4 of 19**: include/core/math_utils.h

**File Information**:
- **Path**: `include/core/math_utils.h`
- **Size**: 66 lines
- **Modified**: 2025-10-17 09:43:08
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <Eigen/Dense>

namespace trading {

/**
 * Mathematical utility functions for statistical calculations
 * Used across feature extraction and analytics
 */
class MathUtils {
public:
    /**
     * Calculate arithmetic mean of values
     */
    static double mean(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    }

    /**
     * Calculate sample standard deviation
     */
    static double stddev(const std::vector<double>& values) {
        if (values.size() < 2) return 0.0;
        double m = mean(values);
        double sq_sum = 0.0;
        for (double v : values) {
            sq_sum += (v - m) * (v - m);
        }
        return std::sqrt(sq_sum / (values.size() - 1));
    }

    /**
     * Find maximum value in vector
     */
    static double max(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return *std::max_element(values.begin(), values.end());
    }

    /**
     * Find minimum value in vector
     */
    static double min(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return *std::min_element(values.begin(), values.end());
    }

    /**
     * Calculate exponential moving average
     */
    static double ema(const std::vector<double>& values, double alpha) {
        if (values.empty()) return 0.0;
        double ema_val = values[0];
        for (size_t i = 1; i < values.size(); ++i) {
            ema_val = alpha * values[i] + (1.0 - alpha) * ema_val;
        }
        return ema_val;
    }
};

} // namespace trading

```

## 📄 **FILE 5 of 19**: include/core/types.h

**File Information**:
- **Path**: `include/core/types.h`
- **Size**: 27 lines
- **Modified**: 2025-10-17 09:28:21
- **Type**: h
- **Permissions**: -rw-r--r--

```text
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

```

## 📄 **FILE 6 of 19**: include/predictor/feature_extractor.h

**File Information**:
- **Path**: `include/predictor/feature_extractor.h`
- **Size**: 92 lines
- **Modified**: 2025-10-17 09:43:59
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "core/bar.h"
#include "utils/circular_buffer.h"
#include <Eigen/Dense>
#include <optional>
#include <array>

namespace trading {

/**
 * Enhanced Feature Extractor - 25 Technical Indicators
 *
 * Extracts comprehensive set of proven technical indicators for online learning:
 * - Multi-timeframe momentum (1, 3, 5, 10 bars)
 * - Volatility measures (realized vol, ATR)
 * - Volume analysis (surge, relative volume)
 * - Price position indicators (range position, channel position)
 * - Trend strength (RSI-like, directional momentum)
 * - Interaction terms (momentum * volatility, etc.)
 *
 * Optimized for:
 * - O(1) incremental updates via CircularBuffer
 * - Minimal memory footprint (50-bar lookback)
 * - Production-ready stability (handles edge cases)
 */
class FeatureExtractor {
private:
    CircularBuffer<Bar> history_;
    static constexpr size_t LOOKBACK = 50;      // Increased from 20 for better features
    static constexpr size_t NUM_FEATURES = 25;  // Increased from 10

    // Cached values for incremental calculation
    double prev_close_;
    size_t bar_count_;

public:
    FeatureExtractor();

    /**
     * Extract features from new bar
     * @param bar New OHLCV bar
     * @return Feature vector (std::nullopt during warmup period)
     *
     * Returns std::nullopt if less than LOOKBACK bars have been seen.
     * Once warmed up, always returns valid 25-dimensional feature vector.
     */
    std::optional<Eigen::VectorXd> extract(const Bar& bar);

    /**
     * Access price history (for debugging/inspection)
     */
    const CircularBuffer<Bar>& history() const { return history_; }

    /**
     * Check if warmup period is complete
     */
    bool is_ready() const { return bar_count_ >= LOOKBACK; }

    /**
     * Get number of bars processed
     */
    size_t bar_count() const { return bar_count_; }

    /**
     * Reset to initial state
     */
    void reset();

    /**
     * Get feature names (for debugging/logging)
     */
    static std::vector<std::string> get_feature_names();

private:
    // Core feature calculations
    double calculate_momentum(const std::vector<Price>& prices, int period) const;
    double calculate_volatility(const std::vector<Price>& prices, int period) const;
    double calculate_atr(const std::vector<Bar>& bars, int period) const;
    double calculate_volume_surge(const std::vector<Volume>& volumes) const;
    double calculate_relative_volume(const std::vector<Volume>& volumes, int period) const;
    double calculate_price_position(const std::vector<Bar>& bars, Price current_price) const;
    double calculate_channel_position(const std::vector<Bar>& bars, int period) const;
    double calculate_rsi_like(const std::vector<Price>& prices, int period) const;
    double calculate_directional_momentum(const std::vector<Price>& prices, int period) const;

    // Utility helpers
    std::vector<Price> get_closes() const;
    std::vector<Volume> get_volumes() const;
    std::vector<Bar> get_bars() const;
};

} // namespace trading

```

## 📄 **FILE 7 of 19**: include/predictor/online_predictor.h

**File Information**:
- **Path**: `include/predictor/online_predictor.h`
- **Size**: 79 lines
- **Modified**: 2025-10-17 09:43:37
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include <Eigen/Dense>
#include <vector>

namespace trading {

/**
 * Exponentially Weighted Recursive Least Squares (EWRLS) Online Predictor
 *
 * Online learning algorithm with forgetting factor for non-stationary time series.
 * O(n^2) update complexity where n = number of features.
 *
 * Key properties:
 * - Adapts to changing market conditions via lambda (forgetting factor)
 * - No batch training required - learns incrementally
 * - Provably convergent under standard assumptions
 * - Efficient recursive updates
 *
 * Usage:
 *   OnlinePredictor predictor(num_features, lambda);
 *   double pred = predictor.predict(features);
 *   predictor.update(features, actual_return);
 */
class OnlinePredictor {
private:
    Eigen::VectorXd theta_;      // Model weights
    Eigen::MatrixXd P_;          // Covariance matrix
    double lambda_;              // Forgetting factor (0.95-0.99)
    size_t n_features_;
    size_t updates_;             // Track number of updates

public:
    /**
     * Constructor
     * @param n_features Number of input features (default 25)
     * @param lambda Forgetting factor (default 0.98)
     *               - Higher (closer to 1.0) = more memory, slower adaptation
     *               - Lower = faster adaptation, less stable
     *               - Typical range: 0.95-0.995
     */
    explicit OnlinePredictor(size_t n_features = 25, double lambda = 0.98);

    /**
     * Make prediction for given feature vector
     * @param features Input feature vector (must match n_features)
     * @return Predicted return (can be positive or negative)
     */
    double predict(const Eigen::VectorXd& features) const;

    /**
     * Update model with observed outcome
     * @param features Input feature vector used for prediction
     * @param actual_return Realized return (target variable)
     *
     * Updates model weights using EWRLS equations:
     * - error = actual - predicted
     * - gain = P * features / (lambda + features' * P * features)
     * - theta += gain * error
     * - P = (P - gain * features' * P) / lambda
     */
    void update(const Eigen::VectorXd& features, double actual_return);

    /**
     * Get current model weights (for inspection/debugging)
     */
    const Eigen::VectorXd& weights() const { return theta_; }

    /**
     * Get number of updates performed
     */
    size_t update_count() const { return updates_; }

    /**
     * Reset predictor to initial state
     */
    void reset();
};

} // namespace trading

```

## 📄 **FILE 8 of 19**: include/trading/multi_symbol_trader.h

**File Information**:
- **Path**: `include/trading/multi_symbol_trader.h`
- **Size**: 172 lines
- **Modified**: 2025-10-17 09:45:44
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "core/types.h"
#include "core/bar.h"
#include "predictor/online_predictor.h"
#include "predictor/feature_extractor.h"
#include "trading/position.h"
#include "trading/trade_history.h"
#include <unordered_map>
#include <memory>
#include <vector>

namespace trading {

/**
 * Prediction Data - Stores prediction and associated information
 */
struct PredictionData {
    double predicted_return;      // Predicted next-bar return
    Eigen::VectorXd features;     // Feature vector (25 dimensions)
    Price current_price;          // Current price
};

/**
 * Trading Configuration
 */
struct TradingConfig {
    double initial_capital = 100000.0;
    size_t max_positions = 3;
    double stop_loss_pct = -0.02;      // -2%
    double profit_target_pct = 0.05;   // 5%
    size_t min_bars_to_learn = 100;
    size_t lookback_window = 50;
    double lambda = 0.98;
    int bars_per_day = 390;
    bool eod_liquidation = true;
    double win_multiplier = 1.3;
    double loss_multiplier = 0.7;
    size_t trade_history_size = 3;     // Track last N trades for adaptive sizing
    double min_prediction_threshold = 0.001;  // Minimum predicted return to trade
};

/**
 * Multi-Symbol Online Trading System
 *
 * Features:
 * - Online learning per symbol (EWRLS with 25 features)
 * - Dynamic position management (max N concurrent positions)
 * - Automatic stop-loss and profit targets
 * - Adaptive position sizing based on recent performance
 * - EOD liquidation option
 * - Rotation strategy (top N by predicted return)
 *
 * Usage:
 *   MultiSymbolTrader trader(symbols, config);
 *   for (each bar) {
 *       trader.on_bar(market_data);
 *   }
 *   auto results = trader.get_results();
 */
class MultiSymbolTrader {
private:
    std::vector<Symbol> symbols_;
    TradingConfig config_;
    double cash_;

    // Per-symbol components
    std::unordered_map<Symbol, std::unique_ptr<OnlinePredictor>> predictors_;
    std::unordered_map<Symbol, std::unique_ptr<FeatureExtractor>> extractors_;
    std::unordered_map<Symbol, Position> positions_;
    std::unordered_map<Symbol, std::unique_ptr<TradeHistory>> trade_history_;

    size_t bars_seen_;
    int total_trades_;

public:
    /**
     * Constructor
     * @param symbols List of symbols to trade
     * @param config Trading configuration (optional, uses defaults if not provided)
     */
    explicit MultiSymbolTrader(const std::vector<Symbol>& symbols,
                              const TradingConfig& config = TradingConfig());

    /**
     * Process new market data bar
     * @param market_data Map of symbol -> bar for current timestamp
     *
     * Steps:
     * 1. Extract features and make predictions for each symbol
     * 2. Update predictors with realized returns
     * 3. Update existing positions (check stop-loss/profit targets)
     * 4. Make trading decisions (rotation to top N predicted symbols)
     * 5. EOD liquidation if enabled
     */
    void on_bar(const std::unordered_map<Symbol, Bar>& market_data);

    /**
     * Get current equity (cash + position values)
     */
    double get_equity(const std::unordered_map<Symbol, Bar>& market_data) const;

    /**
     * Backtest results structure
     */
    struct BacktestResults {
        double total_return;        // Total return as fraction
        double mrd;                 // Mean Return per Day
        double final_equity;        // Final equity value
        int total_trades;           // Number of completed trades
        int winning_trades;         // Number of profitable trades
        int losing_trades;          // Number of losing trades
        double win_rate;            // Fraction of winning trades
        double avg_win;             // Average win amount
        double avg_loss;            // Average loss amount
        double profit_factor;       // Gross profit / gross loss
        double max_drawdown;        // Maximum drawdown (not yet implemented)
    };

    /**
     * Get backtest results
     */
    BacktestResults get_results() const;

    /**
     * Get current positions (for monitoring)
     */
    const std::unordered_map<Symbol, Position>& positions() const { return positions_; }

    /**
     * Get current cash
     */
    double cash() const { return cash_; }

    /**
     * Get configuration
     */
    const TradingConfig& config() const { return config_; }

private:
    /**
     * Make trading decisions based on predictions
     */
    void make_trades(const std::unordered_map<Symbol, PredictionData>& predictions,
                    const std::unordered_map<Symbol, Bar>& market_data);

    /**
     * Update existing positions (check stop-loss/profit targets)
     */
    void update_positions(const std::unordered_map<Symbol, Bar>& market_data);

    /**
     * Calculate position size for a symbol using adaptive sizing
     */
    double calculate_position_size(const Symbol& symbol);

    /**
     * Enter new position
     */
    void enter_position(const Symbol& symbol, Price price, Timestamp time, double capital);

    /**
     * Exit existing position
     */
    double exit_position(const Symbol& symbol, Price price, Timestamp time);

    /**
     * Liquidate all positions
     */
    void liquidate_all(const std::unordered_map<Symbol, Bar>& market_data, const std::string& reason);
};

} // namespace trading

```

## 📄 **FILE 9 of 19**: include/trading/position.h

**File Information**:
- **Path**: `include/trading/position.h`
- **Size**: 63 lines
- **Modified**: 2025-10-17 09:45:14
- **Type**: h
- **Permissions**: -rw-r--r--

```text
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
 * - Unrealized P&L calculations
 */
struct Position {
    int shares;              // Number of shares (can be negative for short)
    Price entry_price;       // Entry price per share
    Timestamp entry_time;    // When position was opened

    Position() : shares(0), entry_price(0.0) {}

    Position(int s, Price p, Timestamp t)
        : shares(s), entry_price(p), entry_time(t) {}

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

```

## 📄 **FILE 10 of 19**: include/trading/trade_history.h

**File Information**:
- **Path**: `include/trading/trade_history.h`
- **Size**: 41 lines
- **Modified**: 2025-10-17 09:45:14
- **Type**: h
- **Permissions**: -rw-r--r--

```text
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

    TradeRecord()
        : pnl(0.0), pnl_pct(0.0), shares(0), entry_price(0.0), exit_price(0.0) {}

    TradeRecord(double p, double pct)
        : pnl(p), pnl_pct(pct), shares(0), entry_price(0.0), exit_price(0.0) {}

    TradeRecord(double p, double pct, Timestamp entry, Timestamp exit,
                const Symbol& sym, int sh, Price entry_pr, Price exit_pr)
        : pnl(p), pnl_pct(pct), entry_time(entry), exit_time(exit),
          symbol(sym), shares(sh), entry_price(entry_pr), exit_price(exit_pr) {}

    bool is_win() const { return pnl > 0; }
    bool is_loss() const { return pnl < 0; }
};

/**
 * Trade History - Circular buffer of recent trades
 * Used for adaptive position sizing and performance tracking
 */
using TradeHistory = CircularBuffer<TradeRecord>;

} // namespace trading

```

## 📄 **FILE 11 of 19**: include/trading/trading_mode.h

**File Information**:
- **Path**: `include/trading/trading_mode.h`
- **Size**: 87 lines
- **Modified**: 2025-10-17 10:17:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include <string>

namespace trading {

/**
 * Trading Mode - Mock vs Live
 */
enum class TradingMode {
    MOCK,   // Backtest on historical data
    LIVE    // Live paper trading (future: real trading)
};

/**
 * Convert string to TradingMode
 */
inline TradingMode parse_trading_mode(const std::string& mode_str) {
    if (mode_str == "live" || mode_str == "LIVE") {
        return TradingMode::LIVE;
    }
    return TradingMode::MOCK;
}

/**
 * Convert TradingMode to string
 */
inline std::string to_string(TradingMode mode) {
    return (mode == TradingMode::LIVE) ? "LIVE" : "MOCK";
}

/**
 * Default symbol lists for multi-symbol rotation trading
 */
namespace symbols {

/**
 * 6 core symbols (most liquid, reliable)
 */
inline const char* DEFAULT_6[] = {
    "TQQQ",  // 3x QQQ bull
    "SQQQ",  // 3x QQQ bear
    "UPRO",  // 3x SPY bull
    "SDS",   // 2x SPY bear
    "UVXY",  // VIX call
    "SVXY"   // VIX put
};

/**
 * 10 symbols for extended rotation (recommended)
 */
inline const char* DEFAULT_10[] = {
    // QQQ leveraged (3x)
    "TQQQ",  // 3x QQQ bull
    "SQQQ",  // 3x QQQ bear

    // SPY leveraged (2x - more stable than 3x)
    "SSO",   // 2x SPY bull
    "SDS",   // 2x SPY bear

    // Russell 2000 (3x)
    "TNA",   // 3x IWM bull
    "TZA",   // 3x IWM bear

    // Financial sector (3x)
    "FAS",   // 3x Finance bull
    "FAZ",   // 3x Finance bear

    // Volatility
    "UVXY",  // VIX call
    "SVXY"   // VIX put
};

/**
 * 14 symbols (maximum diversity)
 */
inline const char* DEFAULT_14[] = {
    "TQQQ", "SQQQ",  // QQQ 3x
    "SSO", "SDS",     // SPY 2x
    "TNA", "TZA",     // Russell 3x
    "FAS", "FAZ",     // Finance 3x
    "ERX", "ERY",     // Energy 3x
    "UVXY", "SVXY",   // Volatility
    "NUGT", "DUST"    // Gold miners 3x
};

} // namespace symbols
} // namespace trading

```

## 📄 **FILE 12 of 19**: include/utils/circular_buffer.h

**File Information**:
- **Path**: `include/utils/circular_buffer.h`
- **Size**: 116 lines
- **Modified**: 2025-10-17 09:43:08
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include <vector>
#include <stdexcept>

namespace trading {

/**
 * Circular Buffer - Fixed-size ring buffer with O(1) operations
 *
 * Efficiently stores most recent N elements with:
 * - O(1) push_back
 * - O(1) indexed access
 * - Cache-friendly contiguous storage
 * - Automatic wraparound
 *
 * Used for price history, feature windows, etc.
 */
template<typename T>
class CircularBuffer {
private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t size_;
    size_t head_;  // Index of oldest element
    size_t tail_;  // Index where next element will be inserted

public:
    /**
     * Construct circular buffer with fixed capacity
     * @param capacity Maximum number of elements to store
     */
    explicit CircularBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity), size_(0), head_(0), tail_(0) {}

    /**
     * Add element to buffer (overwrites oldest if full)
     */
    void push_back(const T& item) {
        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;
        if (size_ < capacity_) {
            size_++;
        } else {
            head_ = (head_ + 1) % capacity_;
        }
    }

    /**
     * Access element by index (0 = oldest, size-1 = newest)
     */
    T& operator[](size_t idx) {
        if (idx >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return buffer_[(head_ + idx) % capacity_];
    }

    const T& operator[](size_t idx) const {
        if (idx >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return buffer_[(head_ + idx) % capacity_];
    }

    /**
     * Number of elements currently in buffer
     */
    size_t size() const { return size_; }

    /**
     * Check if buffer is empty
     */
    bool empty() const { return size_ == 0; }

    /**
     * Check if buffer is at full capacity
     */
    bool full() const { return size_ == capacity_; }

    /**
     * Access most recent element
     */
    T& back() {
        if (empty()) throw std::runtime_error("Buffer is empty");
        return buffer_[(tail_ + capacity_ - 1) % capacity_];
    }

    const T& back() const {
        if (empty()) throw std::runtime_error("Buffer is empty");
        return buffer_[(tail_ + capacity_ - 1) % capacity_];
    }

    /**
     * Convert to vector (useful for bulk operations)
     * Returns elements in order from oldest to newest
     */
    std::vector<T> to_vector() const {
        std::vector<T> result;
        result.reserve(size_);
        for (size_t i = 0; i < size_; ++i) {
            result.push_back((*this)[i]);
        }
        return result;
    }

    /**
     * Clear all elements
     */
    void clear() {
        size_ = 0;
        head_ = 0;
        tail_ = 0;
    }
};

} // namespace trading

```

## 📄 **FILE 13 of 19**: include/utils/data_loader.h

**File Information**:
- **Path**: `include/utils/data_loader.h`
- **Size**: 73 lines
- **Modified**: 2025-10-17 09:58:51
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "core/bar.h"
#include "core/types.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace trading {

/**
 * Market Data Loader - Supports CSV and Binary formats
 *
 * CSV Format:
 *   timestamp_ms,symbol,open,high,low,close,volume
 *   1609459200000,AAPL,132.43,133.61,131.72,132.69,99116600
 *   ...
 *
 * Binary Format (optimized for speed):
 *   count (size_t)
 *   For each bar:
 *     timestamp_ms (int64_t)
 *     symbol_len (size_t)
 *     symbol (char*)
 *     open, high, low, close (double)
 *     volume (int64_t)
 *
 * Usage:
 *   auto data = DataLoader::load("AAPL.csv");
 *   auto data = DataLoader::load("QQQ.bin");  // 10-100x faster
 */
class DataLoader {
public:
    /**
     * Load market data from file (auto-detects format)
     * @param path Path to CSV or binary file
     * @return Vector of bars in chronological order
     */
    static std::vector<Bar> load(const std::string& path);

    /**
     * Load market data for multiple symbols
     * @param paths Map of symbol -> file path
     * @return Map of symbol -> vector of bars
     */
    static std::unordered_map<Symbol, std::vector<Bar>>
    load_multi_symbol(const std::unordered_map<Symbol, std::string>& paths);

    /**
     * Load market data for multiple symbols from directory
     * @param directory Directory containing data files
     * @param symbols List of symbols to load
     * @param extension File extension (".csv" or ".bin")
     * @return Map of symbol -> vector of bars
     */
    static std::unordered_map<Symbol, std::vector<Bar>>
    load_from_directory(const std::string& directory,
                       const std::vector<Symbol>& symbols,
                       const std::string& extension = ".bin");

    /**
     * Save bars to binary format (for converting CSV to binary)
     * @param bars Vector of bars to save
     * @param path Output path (.bin extension)
     */
    static void save_binary(const std::vector<Bar>& bars, const std::string& path);

private:
    static std::vector<Bar> load_csv(const std::string& path);
    static std::vector<Bar> load_binary(const std::string& path);
    static bool ends_with(const std::string& str, const std::string& suffix);
};

} // namespace trading

```

## 📄 **FILE 14 of 19**: include/utils/date_filter.h

**File Information**:
- **Path**: `include/utils/date_filter.h`
- **Size**: 97 lines
- **Modified**: 2025-10-17 10:09:06
- **Type**: h
- **Permissions**: -rw-r--r--

```text
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

```

## 📄 **FILE 15 of 19**: include/utils/results_exporter.h

**File Information**:
- **Path**: `include/utils/results_exporter.h`
- **Size**: 89 lines
- **Modified**: 2025-10-17 10:09:06
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once
#include "trading/multi_symbol_trader.h"
#include "core/types.h"
#include <string>
#include <fstream>
#include <iomanip>

namespace trading {

/**
 * Export trading results to JSON format for dashboard generation
 */
class ResultsExporter {
public:
    /**
     * Export results to JSON file
     * @param results Backtest results
     * @param trader Trader instance (for trade details)
     * @param output_path Output JSON file path
     * @param metadata Additional metadata (symbol list, config, etc.)
     */
    static void export_json(
        const MultiSymbolTrader::BacktestResults& results,
        const MultiSymbolTrader& trader,
        const std::string& output_path,
        const std::string& symbols_str,
        const std::string& mode_str,
        const std::string& start_date,
        const std::string& end_date
    ) {
        std::ofstream file(output_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot create results file: " + output_path);
        }

        file << std::fixed << std::setprecision(4);

        file << "{\n";
        file << "  \"metadata\": {\n";
        file << "    \"timestamp\": \"" << get_current_timestamp() << "\",\n";
        file << "    \"mode\": \"" << mode_str << "\",\n";
        file << "    \"symbols\": \"" << symbols_str << "\",\n";
        file << "    \"start_date\": \"" << start_date << "\",\n";
        file << "    \"end_date\": \"" << end_date << "\",\n";
        file << "    \"initial_capital\": " << trader.config().initial_capital << "\n";
        file << "  },\n";

        file << "  \"performance\": {\n";
        file << "    \"final_equity\": " << results.final_equity << ",\n";
        file << "    \"total_return\": " << results.total_return << ",\n";
        file << "    \"mrd\": " << results.mrd << ",\n";
        file << "    \"total_trades\": " << results.total_trades << ",\n";
        file << "    \"winning_trades\": " << results.winning_trades << ",\n";
        file << "    \"losing_trades\": " << results.losing_trades << ",\n";
        file << "    \"win_rate\": " << results.win_rate << ",\n";
        file << "    \"avg_win\": " << results.avg_win << ",\n";
        file << "    \"avg_loss\": " << results.avg_loss << ",\n";
        file << "    \"profit_factor\": " << results.profit_factor << ",\n";
        file << "    \"max_drawdown\": " << results.max_drawdown << "\n";
        file << "  },\n";

        file << "  \"config\": {\n";
        file << "    \"max_positions\": " << trader.config().max_positions << ",\n";
        file << "    \"stop_loss_pct\": " << trader.config().stop_loss_pct << ",\n";
        file << "    \"profit_target_pct\": " << trader.config().profit_target_pct << ",\n";
        file << "    \"lambda\": " << trader.config().lambda << ",\n";
        file << "    \"min_bars_to_learn\": " << trader.config().min_bars_to_learn << ",\n";
        file << "    \"bars_per_day\": " << trader.config().bars_per_day << "\n";
        file << "  }\n";

        file << "}\n";

        file.close();
    }

private:
    static std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&time_c, &tm);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace trading

```

## 📄 **FILE 16 of 19**: src/main.cpp

**File Information**:
- **Path**: `src/main.cpp`
- **Size**: 506 lines
- **Modified**: 2025-10-17 10:42:40
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "trading/multi_symbol_trader.h"
#include "trading/trading_mode.h"
#include "utils/data_loader.h"
#include "utils/date_filter.h"
#include "utils/results_exporter.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>

using namespace trading;

// Configuration from command line
struct Config {
    std::string data_dir = "data";
    std::string extension = ".bin";  // .bin or .csv
    std::vector<std::string> symbols;
    double capital = 100000.0;
    bool verbose = false;

    // Mode: mock (historical data test) or live (real-time trading)
    TradingMode mode = TradingMode::MOCK;
    std::string mode_str = "mock";

    // Date for testing (mock mode)
    std::string test_date;  // YYYY-MM-DD format (single day or recent if empty)

    // Warmup period
    int warmup_days = 3;     // Default 3 days
    size_t warmup_bars = 0;  // Calculated from warmup_days

    // Dashboard generation
    bool generate_dashboard = false;
    std::string dashboard_script = "generate_dashboard.py";
    std::string results_file = "results.json";

    // Trading parameters
    TradingConfig trading;
};

void print_usage(const char* program_name) {
    std::cout << "Sentio Lite - Multi-Symbol Rotation Trading\n\n"
              << "Two Modes (share exact same trading logic):\n"
              << "  mock  - Test on historical data (default: most recent date)\n"
              << "  live  - Real-time paper trading via Alpaca/Polygon\n\n"
              << "Usage: " << program_name << " <mock|live> [options]\n\n"
              << "Common Options:\n"
              << "  --symbols LIST       Comma-separated symbols or 6|10|14 (default: 10)\n"
              << "  --warmup-days N      Warmup days before trading (default: 3)\n"
              << "  --capital AMOUNT     Initial capital (default: 100000)\n"
              << "  --max-positions N    Max concurrent positions (default: 3)\n"
              << "  --generate-dashboard Generate HTML dashboard report\n"
              << "  --verbose            Show detailed progress\n\n"
              << "Mock Mode Options:\n"
              << "  --date YYYY-MM-DD    Test specific date (default: most recent)\n"
              << "  --data-dir DIR       Data directory (default: data)\n"
              << "  --extension EXT      File extension: .bin or .csv (default: .bin)\n\n"
              << "Live Mode Options:\n"
              << "  --fifo PATH          FIFO pipe path (default: /tmp/alpaca_bars.fifo)\n"
              << "  --websocket TYPE     Websocket: alpaca or polygon (default: alpaca)\n\n"
              << "Trading Parameters:\n"
              << "  --stop-loss PCT      Stop loss percentage (default: -0.02)\n"
              << "  --profit-target PCT  Profit target percentage (default: 0.05)\n"
              << "  --lambda LAMBDA      EWRLS forgetting factor (default: 0.98)\n\n"
              << "Output Options:\n"
              << "  --results-file FILE  Results JSON file (default: results.json)\n"
              << "  --help               Show this help message\n\n"
              << "Examples:\n\n"
              << "  # Mock mode - test most recent date with 10 symbols\n"
              << "  " << program_name << " mock\n\n"
              << "  # Mock mode - test specific date\n"
              << "  " << program_name << " mock --date 2024-10-15\n\n"
              << "  # Mock mode - test with custom symbols and generate dashboard\n"
              << "  " << program_name << " mock --symbols TQQQ,SQQQ,SSO,SDS \\\n"
              << "    --date 2024-10-15 --generate-dashboard\n\n"
              << "  # Live mode - paper trade with 10 symbols\n"
              << "  " << program_name << " live\n\n"
              << "  # Live mode - paper trade with 6 symbols and 5-day warmup\n"
              << "  " << program_name << " live --symbols 6 --warmup-days 5\n\n"
              << "Default Symbol Lists:\n"
              << "  6:  TQQQ, SQQQ, UPRO, SDS, UVXY, SVXY\n"
              << "  10: TQQQ, SQQQ, SSO, SDS, TNA, TZA, FAS, FAZ, UVXY, SVXY (default)\n"
              << "  14: + UPRO, SPXS, ERX, ERY, NUGT, DUST\n\n"
              << "Key Insight:\n"
              << "  Mock and live modes share the EXACT same trading logic.\n"
              << "  Research and optimize in mock mode, then run live with confidence!\n";
}

bool parse_symbols(const std::string& symbols_str, std::vector<std::string>& symbols) {
    // Check for default lists
    if (symbols_str == "6") {
        for (const auto& sym : symbols::DEFAULT_6) symbols.push_back(sym);
        return true;
    } else if (symbols_str == "10") {
        for (const auto& sym : symbols::DEFAULT_10) symbols.push_back(sym);
        return true;
    } else if (symbols_str == "14") {
        for (const auto& sym : symbols::DEFAULT_14) symbols.push_back(sym);
        return true;
    }

    // Parse comma-separated list
    std::istringstream iss(symbols_str);
    std::string symbol;
    while (std::getline(iss, symbol, ',')) {
        if (!symbol.empty()) {
            symbols.push_back(symbol);
        }
    }
    return !symbols.empty();
}

bool parse_args(int argc, char* argv[], Config& config) {
    if (argc < 2) {
        return false;
    }

    // First argument is mode
    std::string mode_arg = argv[1];
    if (mode_arg == "--help" || mode_arg == "-h") {
        return false;
    }

    if (mode_arg != "mock" && mode_arg != "live") {
        std::cerr << "Error: First argument must be 'mock' or 'live'\n";
        return false;
    }

    config.mode_str = mode_arg;
    config.mode = parse_trading_mode(mode_arg);

    // Default symbols to 10
    parse_symbols("10", config.symbols);

    // Parse remaining options
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        // Data options
        else if (arg == "--data-dir" && i + 1 < argc) {
            config.data_dir = argv[++i];
        }
        else if (arg == "--extension" && i + 1 < argc) {
            config.extension = argv[++i];
            if (config.extension[0] != '.') {
                config.extension = "." + config.extension;
            }
        }
        // Symbol options
        else if (arg == "--symbols" && i + 1 < argc) {
            config.symbols.clear();
            if (!parse_symbols(argv[++i], config.symbols)) {
                std::cerr << "Error: Invalid symbols specification\n";
                return false;
            }
        }
        // Date option (mock mode)
        else if (arg == "--date" && i + 1 < argc) {
            config.test_date = argv[++i];
        }
        // Warmup
        else if (arg == "--warmup-days" && i + 1 < argc) {
            config.warmup_days = std::stoi(argv[++i]);
        }
        // Trading parameters
        else if (arg == "--capital" && i + 1 < argc) {
            config.capital = std::stod(argv[++i]);
            config.trading.initial_capital = config.capital;
        }
        else if (arg == "--max-positions" && i + 1 < argc) {
            config.trading.max_positions = std::stoul(argv[++i]);
        }
        else if (arg == "--stop-loss" && i + 1 < argc) {
            config.trading.stop_loss_pct = std::stod(argv[++i]);
        }
        else if (arg == "--profit-target" && i + 1 < argc) {
            config.trading.profit_target_pct = std::stod(argv[++i]);
        }
        else if (arg == "--lambda" && i + 1 < argc) {
            config.trading.lambda = std::stod(argv[++i]);
        }
        // Output options
        else if (arg == "--generate-dashboard") {
            config.generate_dashboard = true;
        }
        else if (arg == "--results-file" && i + 1 < argc) {
            config.results_file = argv[++i];
        }
        else if (arg == "--verbose") {
            config.verbose = true;
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            return false;
        }
    }

    // Calculate warmup bars
    config.warmup_bars = config.warmup_days * config.trading.bars_per_day;

    return true;
}

void generate_dashboard(const std::string& results_file, const std::string& script_path) {
    std::cout << "\nGenerating dashboard...\n";

    // Call dashboard script - it will auto-generate timestamped filename
    std::string command = "python3 " + script_path + " " + results_file;
    int ret = system(command.c_str());

    if (ret != 0) {
        std::cerr << "⚠️  Dashboard generation failed (code: " << ret << ")\n";
        std::cerr << "   Command: " << command << "\n";
    }
    // Success message is printed by the Python script
}

std::string get_most_recent_date(const std::unordered_map<Symbol, std::vector<Bar>>& all_data) {
    Timestamp max_timestamp = std::chrono::system_clock::time_point::min();
    for (const auto& [symbol, bars] : all_data) {
        if (!bars.empty()) {
            max_timestamp = std::max(max_timestamp, bars.back().timestamp);
        }
    }

    // Convert timestamp to YYYY-MM-DD
    auto duration = max_timestamp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    time_t time = static_cast<time_t>(seconds);
    struct tm* timeinfo = localtime(&time);
    char buffer[11];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
    return std::string(buffer);
}

int run_mock_mode(Config& config) {
    try {
        // Load market data
        std::cout << "Loading market data from " << config.data_dir << "...\n";
        auto start_load = std::chrono::high_resolution_clock::now();

        auto all_data = DataLoader::load_from_directory(
            config.data_dir,
            config.symbols,
            config.extension
        );

        auto end_load = std::chrono::high_resolution_clock::now();
        auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_load - start_load).count();

        std::cout << "Data loaded in " << load_duration << "ms\n";

        // Determine test date
        std::string test_date = config.test_date;
        if (test_date.empty()) {
            test_date = get_most_recent_date(all_data);
            std::cout << "Testing most recent date: " << test_date << "\n";
        } else {
            std::cout << "Testing specific date: " << test_date << "\n";
        }

        // Filter to test date (include warmup period)
        std::cout << "Filtering to test date (including " << config.warmup_days << " days warmup)...\n";

        // Calculate start date (test_date - warmup_days)
        // For simplicity, we'll filter to get enough bars around the test date
        // In production, you'd calculate exact date arithmetic

        for (auto& [symbol, bars] : all_data) {
            // For now, just take the most recent bars that include warmup + test day
            size_t total_bars_needed = config.warmup_bars + config.trading.bars_per_day;
            if (bars.size() > total_bars_needed) {
                bars = std::vector<Bar>(bars.end() - total_bars_needed, bars.end());
            }
            std::cout << "  " << symbol << ": " << bars.size() << " bars\n";
        }

        // Find minimum number of bars across all symbols
        size_t min_bars = std::numeric_limits<size_t>::max();
        for (const auto& [symbol, bars] : all_data) {
            min_bars = std::min(min_bars, bars.size());
        }

        if (min_bars < config.warmup_bars + 100) {
            std::cerr << "⚠️  Warning: Only " << min_bars << " bars available\n";
            std::cerr << "   Need at least " << config.warmup_bars << " for warmup\n";
            if (min_bars < config.warmup_bars) {
                config.warmup_bars = min_bars / 2;
                std::cerr << "   Reducing warmup to " << config.warmup_bars << " bars\n";
            }
        }

        std::cout << "\nRunning MOCK mode (" << min_bars << " bars)...\n";
        std::cout << "  Warmup: " << config.warmup_bars << " bars (~"
                  << (config.warmup_bars / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Trading: " << (min_bars - config.warmup_bars) << " bars (~"
                  << ((min_bars - config.warmup_bars) / config.trading.bars_per_day) << " days)\n";
        std::cout << "  Features: 25 technical indicators\n";
        std::cout << "  Predictor: EWRLS (Online Learning, λ=" << config.trading.lambda << ")\n";
        std::cout << "  Strategy: Multi-symbol rotation (top " << config.trading.max_positions << ")\n\n";

        // Adjust min_bars_to_learn based on warmup
        config.trading.min_bars_to_learn = config.warmup_bars;

        // Initialize trader
        MultiSymbolTrader trader(config.symbols, config.trading);

        // Process bars (same logic as live mode would use)
        auto start_trading = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < min_bars; ++i) {
            // Create market snapshot for this bar
            std::unordered_map<Symbol, Bar> market_snapshot;
            for (const auto& symbol : config.symbols) {
                market_snapshot[symbol] = all_data[symbol][i];
            }

            // Process bar (SAME CODE AS LIVE MODE)
            trader.on_bar(market_snapshot);

            // Progress update
            if (i == config.warmup_bars - 1) {
                std::cout << "  ✅ Warmup complete (" << config.warmup_bars << " bars), starting trading...\n";
            }

            if (config.verbose && i >= config.warmup_bars && (i - config.warmup_bars + 1) % 100 == 0) {
                double equity = trader.get_equity(market_snapshot);
                double return_pct = (equity - config.capital) / config.capital * 100;
                std::cout << "  [" << (i - config.warmup_bars + 1) << "/" << (min_bars - config.warmup_bars) << "] "
                          << "Equity: $" << std::fixed << std::setprecision(2) << equity
                          << " (" << std::showpos << return_pct << std::noshowpos << "%)"
                          << std::endl;
            }
        }

        auto end_trading = std::chrono::high_resolution_clock::now();
        auto trading_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_trading - start_trading).count();

        // Get results
        auto results = trader.get_results();

        // Export results for dashboard
        if (config.generate_dashboard) {
            std::string symbols_str;
            for (size_t i = 0; i < config.symbols.size(); ++i) {
                symbols_str += config.symbols[i];
                if (i < config.symbols.size() - 1) symbols_str += ",";
            }

            ResultsExporter::export_json(
                results, trader, config.results_file,
                symbols_str, "MOCK",
                test_date, test_date
            );
            std::cout << "\n✅ Results exported to: " << config.results_file << "\n";
        }

        // Print results
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                 MOCK MODE Results                          ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

        std::cout << "Test Summary:\n";
        std::cout << "  Test Date:          " << test_date << "\n";
        std::cout << "  Warmup:             " << (config.warmup_bars / config.trading.bars_per_day) << " days\n";
        std::cout << "  Trading Period:     " << ((min_bars - config.warmup_bars) / config.trading.bars_per_day) << " days\n";
        std::cout << "\n";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Performance:\n";
        std::cout << "  Initial Capital:    $" << config.capital << "\n";
        std::cout << "  Final Equity:       $" << results.final_equity << "\n";
        std::cout << "  Total Return:       " << std::showpos << (results.total_return * 100)
                  << std::noshowpos << "%\n";
        std::cout << "  MRD (Daily):        " << std::showpos << (results.mrd * 100)
                  << std::noshowpos << "% per day\n";
        std::cout << "\n";

        std::cout << "Trade Statistics:\n";
        std::cout << "  Total Trades:       " << results.total_trades << "\n";
        std::cout << "  Winning Trades:     " << results.winning_trades << "\n";
        std::cout << "  Losing Trades:      " << results.losing_trades << "\n";
        std::cout << std::setprecision(1);
        std::cout << "  Win Rate:           " << (results.win_rate * 100) << "%\n";
        std::cout << std::setprecision(2);
        std::cout << "  Average Win:        $" << results.avg_win << "\n";
        std::cout << "  Average Loss:       $" << results.avg_loss << "\n";
        std::cout << "  Profit Factor:      " << results.profit_factor << "\n";
        std::cout << "\n";

        std::cout << "Execution:\n";
        std::cout << "  Bars Processed:     " << min_bars << " ("
                  << config.warmup_bars << " warmup + "
                  << (min_bars - config.warmup_bars) << " trading)\n";
        std::cout << "  Data Load Time:     " << load_duration << "ms\n";
        std::cout << "  Execution Time:     " << trading_duration << "ms\n";
        std::cout << "  Total Time:         " << (load_duration + trading_duration) << "ms\n";
        std::cout << "\n";

        // Performance assessment
        std::cout << "Assessment: ";
        if (results.total_return > 0.02 && results.win_rate > 0.55) {
            std::cout << "🟢 Excellent (ready for live)\n";
        } else if (results.total_return > 0.01 && results.win_rate > 0.50) {
            std::cout << "🟡 Good (consider more testing)\n";
        } else if (results.total_return > 0.0) {
            std::cout << "🟠 Moderate (needs optimization)\n";
        } else {
            std::cout << "🔴 Poor (not ready for live)\n";
        }

        std::cout << "\n";

        // Generate dashboard if requested
        if (config.generate_dashboard) {
            generate_dashboard(config.results_file, config.dashboard_script);
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n\n";
        return 1;
    }
}

int run_live_mode(Config& config) {
    (void)config;  // Suppress unused warning - will be used when live mode is implemented

    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              LIVE MODE (Paper Trading)                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    std::cout << "⚠️  LIVE MODE NOT YET IMPLEMENTED\n\n";
    std::cout << "To implement live mode:\n";
    std::cout << "  1. Start websocket bridge (Alpaca or Polygon)\n";
    std::cout << "  2. Read bars from FIFO pipe\n";
    std::cout << "  3. Process bars using SAME trading logic as mock mode\n";
    std::cout << "  4. Submit orders via broker API\n\n";
    std::cout << "The beauty: Mock and live share EXACT same trading code!\n";
    std::cout << "Research in mock mode = confidence in live mode\n\n";

    return 1;
}

int main(int argc, char* argv[]) {
    Config config;

    if (!parse_args(argc, argv, config)) {
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         Sentio Lite - Rotation Trading System             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Print configuration
    std::cout << "Configuration:\n";
    std::cout << "  Mode: " << to_string(config.mode);
    if (config.mode == TradingMode::LIVE) {
        std::cout << " (⚠️  NOT YET IMPLEMENTED)";
    }
    std::cout << "\n";

    std::cout << "  Symbols (" << config.symbols.size() << "): ";
    for (size_t i = 0; i < config.symbols.size(); ++i) {
        std::cout << config.symbols[i];
        if (i < config.symbols.size() - 1) std::cout << ", ";
    }
    std::cout << "\n";

    std::cout << "  Warmup Period: " << config.warmup_days << " days ("
              << config.warmup_bars << " bars)\n";
    std::cout << "  Initial Capital: $" << std::fixed << std::setprecision(2)
              << config.capital << "\n";
    std::cout << "  Max Positions: " << config.trading.max_positions << "\n";
    std::cout << "  Stop Loss: " << (config.trading.stop_loss_pct * 100) << "%\n";
    std::cout << "  Profit Target: " << (config.trading.profit_target_pct * 100) << "%\n";

    if (config.generate_dashboard) {
        std::cout << "  Dashboard: Enabled\n";
    }
    std::cout << "\n";

    // Run appropriate mode
    if (config.mode == TradingMode::MOCK) {
        return run_mock_mode(config);
    } else {
        return run_live_mode(config);
    }
}

```

## 📄 **FILE 17 of 19**: src/predictor/feature_extractor.cpp

**File Information**:
- **Path**: `src/predictor/feature_extractor.cpp`
- **Size**: 310 lines
- **Modified**: 2025-10-17 09:44:46
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "predictor/feature_extractor.h"
#include "core/math_utils.h"
#include <algorithm>
#include <cmath>

namespace trading {

FeatureExtractor::FeatureExtractor()
    : history_(LOOKBACK), prev_close_(0.0), bar_count_(0) {}

std::optional<Eigen::VectorXd> FeatureExtractor::extract(const Bar& bar) {
    history_.push_back(bar);
    bar_count_++;

    // Need full lookback window for reliable features
    if (!is_ready()) {
        prev_close_ = bar.close;
        return std::nullopt;
    }

    // Get historical data
    auto prices = get_closes();
    auto volumes = get_volumes();
    auto bars = get_bars();

    Eigen::VectorXd features(NUM_FEATURES);

    // ===== MOMENTUM FEATURES (0-3) =====
    // Short-term to longer-term momentum
    features(0) = calculate_momentum(prices, 1);   // 1-bar return
    features(1) = calculate_momentum(prices, 3);   // 3-bar return
    features(2) = calculate_momentum(prices, 5);   // 5-bar return
    features(3) = calculate_momentum(prices, 10);  // 10-bar return

    // ===== VOLATILITY FEATURES (4-6) =====
    features(4) = calculate_volatility(prices, 10);  // 10-bar realized vol
    features(5) = calculate_volatility(prices, 20);  // 20-bar realized vol
    features(6) = calculate_atr(bars, 14);           // Average True Range

    // ===== VOLUME FEATURES (7-8) =====
    features(7) = calculate_volume_surge(volumes);           // Recent vs average
    features(8) = calculate_relative_volume(volumes, 20);    // Normalized volume

    // ===== PRICE POSITION FEATURES (9-11) =====
    features(9) = calculate_price_position(bars, bar.close);    // Position in 50-bar range
    features(10) = calculate_channel_position(bars, 20);        // Position in 20-bar range
    features(11) = calculate_channel_position(bars, 10);        // Position in 10-bar range

    // ===== TREND STRENGTH FEATURES (12-14) =====
    features(12) = calculate_rsi_like(prices, 14);              // 14-period RSI-like
    features(13) = calculate_directional_momentum(prices, 10);  // Directional strength
    features(14) = calculate_directional_momentum(prices, 20);  // Longer-term direction

    // ===== INTERACTION TERMS (15-19) =====
    // These capture non-linear relationships
    features(15) = features(0) * features(4);    // 1-bar momentum * 10-bar volatility
    features(16) = features(2) * features(4);    // 5-bar momentum * volatility
    features(17) = features(3) * features(7);    // 10-bar momentum * volume surge
    features(18) = features(12) * features(4);   // RSI * volatility
    features(19) = features(9) * features(13);   // Price position * direction

    // ===== ACCELERATION FEATURES (20-22) =====
    // Rate of change of momentum
    features(20) = calculate_momentum(prices, 2) - calculate_momentum(prices, 5);
    features(21) = calculate_momentum(prices, 5) - calculate_momentum(prices, 10);
    features(22) = features(4) - features(5);  // Vol change (10-bar vs 20-bar)

    // ===== DERIVED FEATURES (23-24) =====
    features(23) = std::log(1.0 + std::abs(features(3)));  // Log-scaled momentum
    features(24) = 1.0;  // Bias term (always 1.0)

    prev_close_ = bar.close;
    return features;
}

double FeatureExtractor::calculate_momentum(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n <= static_cast<size_t>(period)) return 0.0;

    Price current = prices[n - 1];
    Price past = prices[n - 1 - period];

    if (past == 0 || std::abs(past) < 1e-10) return 0.0;
    return (current - past) / past;
}

double FeatureExtractor::calculate_volatility(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2 || static_cast<size_t>(period) > n) return 0.0;

    std::vector<double> returns;
    size_t start = n - period;
    for (size_t i = start + 1; i < n; ++i) {
        if (prices[i-1] != 0 && std::abs(prices[i-1]) > 1e-10) {
            returns.push_back((prices[i] - prices[i-1]) / prices[i-1]);
        }
    }

    if (returns.empty()) return 0.0;
    return MathUtils::stddev(returns);
}

double FeatureExtractor::calculate_atr(const std::vector<Bar>& bars, int period) const {
    size_t n = bars.size();
    if (n < 2 || static_cast<size_t>(period) > n) return 0.0;

    std::vector<double> true_ranges;
    size_t start = n - period;

    for (size_t i = start; i < n; ++i) {
        double high_low = bars[i].high - bars[i].low;
        double high_close = (i > 0) ? std::abs(bars[i].high - bars[i-1].close) : 0.0;
        double low_close = (i > 0) ? std::abs(bars[i].low - bars[i-1].close) : 0.0;
        double tr = std::max({high_low, high_close, low_close});
        true_ranges.push_back(tr);
    }

    if (true_ranges.empty()) return 0.0;

    // Normalize by current price
    Price current_price = bars[n-1].close;
    if (current_price == 0 || std::abs(current_price) < 1e-10) return 0.0;

    return MathUtils::mean(true_ranges) / current_price;
}

double FeatureExtractor::calculate_volume_surge(const std::vector<Volume>& volumes) const {
    if (volumes.empty()) return 1.0;

    // Compare recent volume (last 5 bars) to average
    size_t n = volumes.size();
    size_t recent_window = std::min(static_cast<size_t>(5), n);

    double recent_avg = 0.0;
    for (size_t i = n - recent_window; i < n; ++i) {
        recent_avg += static_cast<double>(volumes[i]);
    }
    recent_avg /= recent_window;

    double total_avg = 0.0;
    for (const auto& v : volumes) {
        total_avg += static_cast<double>(v);
    }
    total_avg /= volumes.size();

    if (total_avg == 0 || std::abs(total_avg) < 1e-10) return 1.0;
    return recent_avg / total_avg;
}

double FeatureExtractor::calculate_relative_volume(const std::vector<Volume>& volumes, int period) const {
    size_t n = volumes.size();
    if (n == 0) return 0.0;

    size_t window = std::min(static_cast<size_t>(period), n);
    double avg_volume = 0.0;

    for (size_t i = n - window; i < n; ++i) {
        avg_volume += static_cast<double>(volumes[i]);
    }
    avg_volume /= window;

    double current_volume = static_cast<double>(volumes[n-1]);

    if (avg_volume == 0 || std::abs(avg_volume) < 1e-10) return 0.0;
    return (current_volume - avg_volume) / avg_volume;
}

double FeatureExtractor::calculate_price_position(const std::vector<Bar>& bars,
                                                   Price current_price) const {
    if (bars.empty()) return 0.5;

    std::vector<double> highs, lows;
    for (const auto& bar : bars) {
        highs.push_back(bar.high);
        lows.push_back(bar.low);
    }

    double high_n = MathUtils::max(highs);
    double low_n = MathUtils::min(lows);
    double range = high_n - low_n;

    if (range < 1e-8) return 0.5;
    return (current_price - low_n) / range;
}

double FeatureExtractor::calculate_channel_position(const std::vector<Bar>& bars, int period) const {
    size_t n = bars.size();
    if (n == 0) return 0.5;

    size_t window = std::min(static_cast<size_t>(period), n);
    std::vector<double> highs, lows;

    for (size_t i = n - window; i < n; ++i) {
        highs.push_back(bars[i].high);
        lows.push_back(bars[i].low);
    }

    double high_n = MathUtils::max(highs);
    double low_n = MathUtils::min(lows);
    double range = high_n - low_n;

    Price current_price = bars[n-1].close;

    if (range < 1e-8) return 0.5;
    return (current_price - low_n) / range;
}

double FeatureExtractor::calculate_rsi_like(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2) return 0.5;

    size_t window = std::min(static_cast<size_t>(period), n - 1);
    std::vector<double> gains, losses;

    for (size_t i = n - window; i < n; ++i) {
        if (prices[i-1] != 0 && std::abs(prices[i-1]) > 1e-10) {
            double ret = (prices[i] - prices[i-1]) / prices[i-1];
            if (ret > 0) {
                gains.push_back(ret);
                losses.push_back(0.0);
            } else {
                gains.push_back(0.0);
                losses.push_back(-ret);
            }
        }
    }

    if (gains.empty()) return 0.5;

    double avg_gain = MathUtils::mean(gains);
    double avg_loss = MathUtils::mean(losses);

    if (avg_loss < 1e-8) return 1.0;
    if (avg_gain < 1e-8) return 0.0;

    // Normalize to [0, 1] range like RSI
    double rs = avg_gain / avg_loss;
    return rs / (1.0 + rs);
}

double FeatureExtractor::calculate_directional_momentum(const std::vector<Price>& prices, int period) const {
    size_t n = prices.size();
    if (n < 2 || static_cast<size_t>(period) >= n) return 0.0;

    int up_moves = 0;
    int down_moves = 0;
    size_t start = n - period - 1;

    for (size_t i = start + 1; i < n; ++i) {
        if (prices[i] > prices[i-1]) up_moves++;
        else if (prices[i] < prices[i-1]) down_moves++;
    }

    int total_moves = up_moves + down_moves;
    if (total_moves == 0) return 0.0;

    // Return net directional bias: +1 all up, -1 all down, 0 neutral
    return static_cast<double>(up_moves - down_moves) / total_moves;
}

void FeatureExtractor::reset() {
    history_.clear();
    prev_close_ = 0.0;
    bar_count_ = 0;
}

std::vector<Price> FeatureExtractor::get_closes() const {
    std::vector<Price> closes;
    closes.reserve(history_.size());
    for (size_t i = 0; i < history_.size(); ++i) {
        closes.push_back(history_[i].close);
    }
    return closes;
}

std::vector<Volume> FeatureExtractor::get_volumes() const {
    std::vector<Volume> volumes;
    volumes.reserve(history_.size());
    for (size_t i = 0; i < history_.size(); ++i) {
        volumes.push_back(history_[i].volume);
    }
    return volumes;
}

std::vector<Bar> FeatureExtractor::get_bars() const {
    return history_.to_vector();
}

std::vector<std::string> FeatureExtractor::get_feature_names() {
    return {
        // Momentum (0-3)
        "momentum_1", "momentum_3", "momentum_5", "momentum_10",
        // Volatility (4-6)
        "volatility_10", "volatility_20", "atr_14",
        // Volume (7-8)
        "volume_surge", "relative_volume_20",
        // Price Position (9-11)
        "price_position_50", "channel_position_20", "channel_position_10",
        // Trend Strength (12-14)
        "rsi_14", "directional_momentum_10", "directional_momentum_20",
        // Interactions (15-19)
        "mom1_x_vol10", "mom5_x_vol10", "mom10_x_volsurge", "rsi_x_vol", "pricepos_x_direction",
        // Acceleration (20-22)
        "momentum_accel_short", "momentum_accel_long", "volatility_change",
        // Derived (23-24)
        "log_momentum", "bias"
    };
}

} // namespace trading

```

## 📄 **FILE 18 of 19**: src/predictor/online_predictor.cpp

**File Information**:
- **Path**: `src/predictor/online_predictor.cpp`
- **Size**: 68 lines
- **Modified**: 2025-10-17 09:43:37
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "predictor/online_predictor.h"
#include <iostream>
#include <stdexcept>

namespace trading {

OnlinePredictor::OnlinePredictor(size_t n_features, double lambda)
    : theta_(Eigen::VectorXd::Zero(n_features)),
      P_(Eigen::MatrixXd::Identity(n_features, n_features) * 100.0),
      lambda_(lambda),
      n_features_(n_features),
      updates_(0) {

    if (lambda <= 0.0 || lambda > 1.0) {
        throw std::invalid_argument("Lambda must be in (0, 1]");
    }
}

double OnlinePredictor::predict(const Eigen::VectorXd& features) const {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error(
            "Feature size mismatch: expected " + std::to_string(n_features_) +
            " but got " + std::to_string(features.size())
        );
    }
    return theta_.dot(features);
}

void OnlinePredictor::update(const Eigen::VectorXd& features, double actual_return) {
    if (features.size() != static_cast<Eigen::Index>(n_features_)) {
        throw std::runtime_error(
            "Feature size mismatch: expected " + std::to_string(n_features_) +
            " but got " + std::to_string(features.size())
        );
    }

    // EWRLS update equations
    // error = y - y_pred
    double error = actual_return - predict(features);

    // Calculate gain vector k = P * x / (lambda + x' * P * x)
    Eigen::VectorXd Px = P_ * features;
    double denominator = lambda_ + features.dot(Px);

    // Avoid division by zero
    if (std::abs(denominator) < 1e-10) {
        denominator = 1e-10;
    }

    Eigen::VectorXd k = Px / denominator;

    // Update weights: theta += k * error
    theta_ += k * error;

    // Update covariance matrix: P = (P - k * x' * P) / lambda
    P_ = (P_ - k * features.transpose() * P_) / lambda_;

    updates_++;
}

void OnlinePredictor::reset() {
    theta_.setZero();
    P_.setIdentity();
    P_ *= 100.0;
    updates_ = 0;
}

} // namespace trading

```

## 📄 **FILE 19 of 19**: src/trading/multi_symbol_trader.cpp

**File Information**:
- **Path**: `src/trading/multi_symbol_trader.cpp`
- **Size**: 304 lines
- **Modified**: 2025-10-17 09:46:37
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "trading/multi_symbol_trader.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace trading {

MultiSymbolTrader::MultiSymbolTrader(const std::vector<Symbol>& symbols,
                                     const TradingConfig& config)
    : symbols_(symbols),
      config_(config),
      cash_(config.initial_capital),
      bars_seen_(0),
      total_trades_(0) {

    // Initialize per-symbol components
    for (const auto& symbol : symbols_) {
        // Predictor with 25 features
        predictors_[symbol] = std::make_unique<OnlinePredictor>(25, config_.lambda);

        // Feature extractor with 50-bar lookback
        extractors_[symbol] = std::make_unique<FeatureExtractor>();

        // Trade history for adaptive sizing
        trade_history_[symbol] = std::make_unique<TradeHistory>(config_.trade_history_size);
    }
}

void MultiSymbolTrader::on_bar(const std::unordered_map<Symbol, Bar>& market_data) {
    bars_seen_++;

    // Step 1: Extract features and make predictions
    std::unordered_map<Symbol, PredictionData> predictions;

    for (const auto& symbol : symbols_) {
        auto it = market_data.find(symbol);
        if (it == market_data.end()) continue;

        const Bar& bar = it->second;
        auto features = extractors_[symbol]->extract(bar);

        if (features.has_value()) {
            // Make prediction
            double pred_return = predictors_[symbol]->predict(features.value());
            predictions[symbol] = {pred_return, features.value(), bar.close};

            // Update predictor with realized return (if we have enough history)
            if (bars_seen_ > 1 && extractors_[symbol]->bar_count() >= 2) {
                const auto& history = extractors_[symbol]->history();
                Price prev_price = history[history.size() - 2].close;
                if (prev_price > 0) {
                    double actual_return = (bar.close - prev_price) / prev_price;
                    predictors_[symbol]->update(features.value(), actual_return);
                }
            }
        }
    }

    // Step 2: Update existing positions (check stop-loss/profit targets)
    update_positions(market_data);

    // Step 3: Make trading decisions (after warmup period)
    if (bars_seen_ > config_.min_bars_to_learn) {
        make_trades(predictions, market_data);
    }

    // Step 4: EOD liquidation
    if (config_.eod_liquidation &&
        bars_seen_ % config_.bars_per_day == config_.bars_per_day - 1) {
        liquidate_all(market_data, "EOD");
    }
}

void MultiSymbolTrader::make_trades(const std::unordered_map<Symbol, PredictionData>& predictions,
                                    const std::unordered_map<Symbol, Bar>& market_data) {

    // Rank symbols by predicted return
    std::vector<std::pair<Symbol, double>> ranked;
    for (const auto& [symbol, pred] : predictions) {
        ranked.emplace_back(symbol, pred.predicted_return);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Get top N symbols
    std::vector<Symbol> top_symbols;
    for (size_t i = 0; i < std::min(ranked.size(), config_.max_positions); ++i) {
        if (ranked[i].second > config_.min_prediction_threshold) {
            top_symbols.push_back(ranked[i].first);
        }
    }

    // Exit positions not in top N
    std::vector<Symbol> to_exit;
    for (const auto& [symbol, pos] : positions_) {
        if (std::find(top_symbols.begin(), top_symbols.end(), symbol) == top_symbols.end()) {
            to_exit.push_back(symbol);
        }
    }

    for (const auto& symbol : to_exit) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            exit_position(symbol, it->second.close, it->second.timestamp);
        }
    }

    // Enter new positions if we have capacity
    for (const auto& [symbol, pred_return] : ranked) {
        if (positions_.size() >= config_.max_positions) break;

        if (positions_.find(symbol) == positions_.end() &&
            pred_return > config_.min_prediction_threshold) {

            double size = calculate_position_size(symbol);
            if (size > 0) {
                auto it = market_data.find(symbol);
                if (it != market_data.end()) {
                    enter_position(symbol, it->second.close, it->second.timestamp, size);
                }
            }
        }
    }
}

void MultiSymbolTrader::update_positions(const std::unordered_map<Symbol, Bar>& market_data) {
    std::vector<Symbol> to_exit;

    for (const auto& [symbol, pos] : positions_) {
        auto it = market_data.find(symbol);
        if (it == market_data.end()) continue;

        Price current_price = it->second.close;
        double pnl_pct = pos.pnl_percentage(current_price);

        // Check stop loss or profit target
        if (pnl_pct <= config_.stop_loss_pct || pnl_pct >= config_.profit_target_pct) {
            to_exit.push_back(symbol);
        }
    }

    for (const auto& symbol : to_exit) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            std::string reason = positions_[symbol].pnl_percentage(it->second.close) < 0
                                 ? "StopLoss" : "ProfitTarget";
            exit_position(symbol, it->second.close, it->second.timestamp);
        }
    }
}

double MultiSymbolTrader::calculate_position_size(const Symbol& symbol) {
    // Base size: Equal weight across max positions, using 95% of cash
    double base_size = (cash_ * 0.95) / config_.max_positions;

    // Adaptive sizing based on recent trade history
    auto& history = *trade_history_[symbol];
    if (history.size() >= config_.trade_history_size) {
        bool all_wins = true;
        bool all_losses = true;

        for (size_t i = 0; i < history.size(); ++i) {
            if (history[i].pnl <= 0) all_wins = false;
            if (history[i].pnl >= 0) all_losses = false;
        }

        if (all_wins) {
            return base_size * config_.win_multiplier;  // Increase after consecutive wins
        } else if (all_losses) {
            return base_size * config_.loss_multiplier;  // Decrease after consecutive losses
        }
    }

    return base_size;
}

void MultiSymbolTrader::enter_position(const Symbol& symbol, Price price,
                                       Timestamp time, double capital) {
    if (capital > cash_) {
        capital = cash_;  // Don't over-leverage
    }

    int shares = static_cast<int>(capital / price);
    if (shares > 0 && capital <= cash_) {
        positions_[symbol] = Position(shares, price, time);
        cash_ -= shares * price;
    }
}

double MultiSymbolTrader::exit_position(const Symbol& symbol, Price price, Timestamp time) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) return 0.0;

    const Position& pos = it->second;
    double proceeds = pos.shares * price;
    double pnl = proceeds - (pos.shares * pos.entry_price);
    double pnl_pct = pnl / (pos.shares * pos.entry_price);

    // Record trade for adaptive sizing
    TradeRecord trade(pnl, pnl_pct, pos.entry_time, time, symbol,
                     pos.shares, pos.entry_price, price);
    trade_history_[symbol]->push_back(trade);

    cash_ += proceeds;
    positions_.erase(it);
    total_trades_++;

    return pnl;
}

void MultiSymbolTrader::liquidate_all(const std::unordered_map<Symbol, Bar>& market_data,
                                      const std::string& reason) {
    std::vector<Symbol> symbols_to_exit;
    for (const auto& [symbol, pos] : positions_) {
        symbols_to_exit.push_back(symbol);
    }

    for (const auto& symbol : symbols_to_exit) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            exit_position(symbol, it->second.close, it->second.timestamp);
        }
    }
}

double MultiSymbolTrader::get_equity(const std::unordered_map<Symbol, Bar>& market_data) const {
    double equity = cash_;

    for (const auto& [symbol, pos] : positions_) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            equity += pos.market_value(it->second.close);
        }
    }

    return equity;
}

MultiSymbolTrader::BacktestResults MultiSymbolTrader::get_results() const {
    BacktestResults results;

    // Collect all trades across all symbols
    std::vector<TradeRecord> all_trades;
    for (const auto& [symbol, history] : trade_history_) {
        for (size_t i = 0; i < history->size(); ++i) {
            all_trades.push_back((*history)[i]);
        }
    }

    results.total_trades = total_trades_;
    results.winning_trades = 0;
    results.losing_trades = 0;
    double gross_profit = 0.0;
    double gross_loss = 0.0;

    for (const auto& trade : all_trades) {
        if (trade.is_win()) {
            results.winning_trades++;
            gross_profit += trade.pnl;
        } else if (trade.is_loss()) {
            results.losing_trades++;
            gross_loss += std::abs(trade.pnl);
        }
    }

    results.win_rate = (results.total_trades > 0)
                       ? static_cast<double>(results.winning_trades) / results.total_trades
                       : 0.0;

    results.avg_win = (results.winning_trades > 0)
                      ? gross_profit / results.winning_trades
                      : 0.0;

    results.avg_loss = (results.losing_trades > 0)
                       ? gross_loss / results.losing_trades
                       : 0.0;

    results.profit_factor = (gross_loss > 0)
                            ? gross_profit / gross_loss
                            : (gross_profit > 0 ? 999.0 : 0.0);

    // Calculate equity metrics
    // Note: For accurate final_equity, need last market_data - so this is approximate
    results.final_equity = cash_;
    for (const auto& [symbol, pos] : positions_) {
        // Use entry price as approximation (ideally should use last known price)
        results.final_equity += pos.market_value(pos.entry_price);
    }

    results.total_return = (config_.initial_capital > 0)
                          ? (results.final_equity - config_.initial_capital) / config_.initial_capital
                          : 0.0;

    double days_traded = static_cast<double>(bars_seen_) / config_.bars_per_day;
    results.mrd = (days_traded > 0) ? results.total_return / days_traded : 0.0;

    results.max_drawdown = 0.0;  // TODO: Implement drawdown tracking

    return results;
}

} // namespace trading

```

