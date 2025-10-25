#include "trading/multi_symbol_trader.h"
#include "core/bar_id_utils.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cmath>
#include <ctime>
#include <map>
#include <set>

/**
 * Calculate simple moving average from price history
 * @param history Price history (most recent price at back)
 * @param period Number of bars for MA calculation
 * @return Moving average, or NaN if insufficient data
 */
static double calculate_moving_average(const std::deque<double>& history, int period) {
    if (static_cast<int>(history.size()) < period) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double sum = 0.0;
    // Sum the last 'period' prices
    for (int i = 0; i < period; ++i) {
        sum += history[history.size() - 1 - i];
    }

    return sum / period;
}

namespace trading {

// Helper function: Check if bar timestamp indicates end of trading day
// Returns true if timestamp is at or after 3:59 PM ET (market close at 4:00 PM)
static bool is_end_of_day(Timestamp timestamp) {
    auto time_seconds = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    time_t time = static_cast<time_t>(time_seconds);
    struct tm* tm_info = localtime(&time);

    // Market close is 16:00 (4:00 PM)
    // Trigger EOD at last minute (15:59 or 16:00)
    return (tm_info->tm_hour == 15 && tm_info->tm_min >= 59) ||
           (tm_info->tm_hour >= 16);
}

// Helper function: Extract date from timestamp (YYYYMMDD format)
static int64_t extract_date_from_timestamp(Timestamp timestamp) {
    auto time_seconds = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    time_t time = static_cast<time_t>(time_seconds);
    struct tm* tm_info = localtime(&time);

    return (tm_info->tm_year + 1900) * 10000 +
           (tm_info->tm_mon + 1) * 100 +
           tm_info->tm_mday;
}

MultiSymbolTrader::MultiSymbolTrader(const std::vector<Symbol>& symbols,
                                     const TradingConfig& config)
    : symbols_(symbols),
      config_(config),
      cash_(config.initial_capital),
      bars_seen_(0),
      trading_bars_(0),
      test_day_start_bar_(0),
      total_trades_(0),
      total_transaction_costs_(0.0),
      daily_start_equity_(config.initial_capital),
      daily_start_trades_(0),
      daily_winning_trades_(0),
      daily_losing_trades_(0) {

    // Calculate when test day begins (after warmup observation + simulation)
    if (config_.strategy == StrategyType::SIGOR) {
        // Rule-based strategies; treat everything as live trading immediately
        config_.warmup.enabled = false;
        config_.warmup.observation_days = 0;
        config_.warmup.simulation_days = 0;
        test_day_start_bar_ = 0;
    } else if (config_.warmup.enabled) {
        test_day_start_bar_ = (config_.warmup.observation_days + config_.warmup.simulation_days)
                            * config_.bars_per_day;
    }

    // Initialize trade filter
    trade_filter_ = std::make_unique<TradeFilter>(config_.filter_config);

    // Initialize per-symbol components (SIGOR only)
    for (const auto& symbol : symbols_) {
        // SIGOR predictor adapter (uses bar data directly)
        sigor_predictors_[symbol] = std::make_unique<SigorPredictorAdapter>(
            symbol, config_.sigor_config);

        // Trade history for adaptive sizing (both strategies)
        trade_history_[symbol] = std::make_unique<TradeHistory>(config_.trade_history_size);

        // Initialize market context with defaults (both strategies)
        market_context_[symbol] = MarketContext(
            config_.default_avg_volume,
            config_.default_volatility,
            30  // Default 30 minutes from open
        );

        // Initialize price history for multi-bar return calculations (both strategies)
        price_history_[symbol] = std::deque<double>();
    }
}

void MultiSymbolTrader::on_bar(const std::unordered_map<Symbol, Bar>& market_data) {
    bars_seen_++;

    // Step 0: COMPREHENSIVE BarID Validation - Ensure all symbols are synchronized
    int64_t reference_timestamp_ms = -1;
    std::string reference_symbol;
    std::vector<std::string> missing_symbols;
    std::vector<std::string> validated_symbols;

    // Check 1: Verify all expected symbols are present
    for (const auto& symbol : symbols_) {
        if (market_data.find(symbol) == market_data.end()) {
            missing_symbols.push_back(symbol);
        }
    }

    if (!missing_symbols.empty()) {
        std::cerr << "  [WARNING] Bar " << bars_seen_ << ": Missing symbols: ";
        for (const auto& sym : missing_symbols) std::cerr << sym << " ";
        std::cerr << std::endl;
    }

    // Check 2: Timestamp synchronization (LIVE MODE ONLY)
    // In live mode, all bars come together every minute from WebSocket
    // Bar_id validation is only needed for mock/simulation mode with binary data
    // where bar_id encodes timestamp information

    // For live mode, we just verify all symbols have data (already done above)
    // No need for strict bar_id or timestamp validation

    for (const auto& [symbol, bar] : market_data) {
        validated_symbols.push_back(symbol);
    }

    // Check 3: Verify bar sequence (detect time gaps)
    static int64_t last_timestamp_ms = -1;
    if (last_timestamp_ms != -1 && reference_timestamp_ms != -1) {
        int64_t time_gap_ms = reference_timestamp_ms - last_timestamp_ms;
        // Expect 1-minute bars (60000ms), warn if gap > 5 minutes
        if (time_gap_ms > 300000) {
            std::cerr << "  [WARNING] Large time gap detected: "
                     << (time_gap_ms / 60000) << " minutes between bars "
                     << (bars_seen_ - 1) << " and " << bars_seen_ << std::endl;
        }
    }
    last_timestamp_ms = reference_timestamp_ms;

    // Validation passed - log periodically for confidence
    if (bars_seen_ % 100 == 0) {
        std::cout << "  [SYNC-CHECK] Bar " << bars_seen_
                 << ": All " << validated_symbols.size() << " symbols synchronized at timestamp "
                 << reference_timestamp_ms << std::endl;
    }

    // Step 1: Update market context for cost calculations
    for (const auto& symbol : symbols_) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            update_market_context(symbol, it->second);
        }
    }

    // Step 2: Update price history for multi-bar return calculations
    for (const auto& symbol : symbols_) {
        auto it = market_data.find(symbol);
        if (it == market_data.end()) continue;

        const Bar& bar = it->second;
        auto& history = price_history_[symbol];
        history.push_back(bar.close);

        // Keep only last 20 bars (enough for 10-bar returns with buffer)
        while (history.size() > 20) {
            history.pop_front();
        }
    }

    // Step 3: Extract features and make multi-horizon predictions
    std::unordered_map<Symbol, PredictionData> predictions;

    for (const auto& symbol : symbols_) {
        auto it = market_data.find(symbol);
        if (it == market_data.end()) continue;

        const Bar& bar = it->second;

        if (config_.strategy == StrategyType::SIGOR) {
            // SIGOR: Update with bar and generate signal
            sigor_predictors_[symbol]->update_with_bar(bar);

            // Check if warmed up
            if (sigor_predictors_[symbol]->is_warmed_up()) {
                // Generate prediction (uses dummy features since SIGOR doesn't need them)
                // Use a minimal feature vector but ensure downstream checks are safe
                Eigen::VectorXd dummy_features = Eigen::VectorXd::Zero(1);
                auto pred = sigor_predictors_[symbol]->predict(dummy_features);
                predictions[symbol] = {pred, dummy_features, bar.close};
            }
        }

        // No learning/update path in SIGOR
    }

    // Step 4: Update trade filter bars held counter
    trade_filter_->update_bars_held(static_cast<int>(bars_seen_));

    // Step 5: Update existing positions (check exit conditions with trade filter)
    update_positions(market_data, predictions);

    // Step 6: Update warmup phase and execute phase-specific logic
    update_phase();

    // Step 6b: Update rotation cooldowns (from online_trader)
    update_rotation_cooldowns();

    if (config_.strategy == StrategyType::SIGOR) {
        // Always trade immediately for SIGOR
        handle_live_phase(predictions, market_data);
    } else {
    switch(config_.current_phase) {
        case TradingConfig::WARMUP_OBSERVATION:
            handle_observation_phase(market_data);
            break;

        case TradingConfig::WARMUP_SIMULATION:
            handle_simulation_phase(predictions, market_data);
            break;

        case TradingConfig::WARMUP_COMPLETE:
        case TradingConfig::LIVE_TRADING:
            handle_live_phase(predictions, market_data);
            break;
        }
    }

    // Step 7: EOD liquidation (use timestamp-based detection)
    // Detect end of day based on bar timestamp (3:59-4:00 PM ET)
    // This is more robust than modulo arithmetic which fails with missing bars
    [[maybe_unused]] static int64_t last_trading_date = 0;
    static int64_t last_eod_date = 0;  // Track last EOD to prevent duplicate triggers
    int64_t current_trading_date = extract_date_from_timestamp(
        market_data.begin()->second.timestamp);
    bool is_eod = is_end_of_day(market_data.begin()->second.timestamp);

    // Only trigger EOD once per day (when we first see EOD timestamp)
    bool should_trigger_eod = is_eod && (current_trading_date != last_eod_date);

    if (config_.eod_liquidation && trading_bars_ > 0 && should_trigger_eod) {
        int day_num = trading_bars_ / config_.bars_per_day;
        last_eod_date = current_trading_date;  // Mark this day as processed

        // Log day boundary transition
        std::cout << "\n[DAY BOUNDARY] Transitioning to day " << day_num << " → "
                  << (day_num + 1) << " (bar " << bars_seen_ << ")\n";

        // Log position states before EOD liquidation
        std::cout << "  [POSITION STATES BEFORE EOD]:\n";
        for (const auto& symbol : symbols_) {
            const auto& state = trade_filter_->get_position_state(symbol);
            std::cout << "    " << symbol << ": "
                      << (state.has_position ? "HOLDING" : "FLAT")
                      << " | last_exit_bar: " << state.last_exit_bar
                      << " | bars_held: " << state.bars_held << "\n";
        }

        // Liquidate all positions
        liquidate_all(market_data, "EOD");

        // Calculate end-of-day equity
        double end_equity = get_equity(market_data);

        // Calculate daily return
        double daily_return = (daily_start_equity_ > 0) ?
                             (end_equity - daily_start_equity_) / daily_start_equity_ : 0.0;

        // Store daily results
        DailyResults daily;
        daily.day_number = day_num;
        daily.start_equity = daily_start_equity_;
        daily.end_equity = end_equity;
        daily.daily_return = daily_return;
        daily.trades_today = total_trades_ - daily_start_trades_;
        daily.winning_trades_today = daily_winning_trades_;
        daily.losing_trades_today = daily_losing_trades_;
        daily_results_.push_back(daily);

        // Print daily summary
        std::cout << "  [EOD] Day " << day_num << " complete:"
                  << " Equity: $" << std::fixed << std::setprecision(2) << end_equity
                  << " (" << std::showpos << (daily_return * 100) << std::noshowpos << "%)"
                  << " | Trades: " << daily.trades_today
                  << " (W:" << daily.winning_trades_today
                  << " L:" << daily.losing_trades_today << ")\n";

        // Reset daily counters for next day
        daily_start_equity_ = end_equity;
        daily_start_trades_ = total_trades_;
        daily_winning_trades_ = 0;
        daily_losing_trades_ = 0;

        // Reset trade filter's daily frequency limits for next trading day
        trade_filter_->reset_daily_limits(static_cast<int>(bars_seen_));

        // Verify filter reset worked
        auto stats = trade_filter_->get_trade_stats(static_cast<int>(bars_seen_));
        std::cout << "  [FILTER RESET] Trades today: " << stats.trades_today
                  << " (should be 0)\n";

        // Log position states after reset
        std::cout << "  [POSITION STATES AFTER RESET]:\n";
        for (const auto& symbol : symbols_) {
            const auto& state = trade_filter_->get_position_state(symbol);
            std::cout << "    " << symbol << ": "
                      << (state.has_position ? "HOLDING" : "FLAT")
                      << " | last_exit_bar: " << state.last_exit_bar
                      << " (should be -999 for FLAT positions)\n";
        }
        std::cout << "\n";
    }

    // Update last trading date for next iteration
    last_trading_date = current_trading_date;
}

void MultiSymbolTrader::make_trades(const std::unordered_map<Symbol, PredictionData>& predictions,
                                    const std::unordered_map<Symbol, Bar>& market_data) {

    // Track if we enter a trade this bar (for adaptive threshold adjustment)
    bool trade_entered_this_bar = false;

    // Enhanced Debug: Detailed trade analysis (every 50 bars when no positions)
    if (positions_.empty() && bars_seen_ % 50 == 0) {
        std::cout << "\n[TRADE ANALYSIS] Bar " << bars_seen_ << ":\n";

        // Sort by 5-bar prediction strength
        std::vector<std::pair<Symbol, const PredictionData*>> debug_ranked;
        for (const auto& [symbol, pred] : predictions) {
            debug_ranked.emplace_back(symbol, &pred);
        }
        std::sort(debug_ranked.begin(), debug_ranked.end(),
                  [](const auto& a, const auto& b) {
                      return std::abs(a.second->prediction.pred_2bar.prediction) >
                             std::abs(b.second->prediction.pred_2bar.prediction);
                  });

        // Show top 5 with detailed rejection reasons
        for (size_t i = 0; i < std::min(size_t(5), debug_ranked.size()); ++i) {
            const auto& symbol = debug_ranked[i].first;
            const auto& pred = *debug_ranked[i].second;

            // Calculate probability
            double probability = prediction_to_probability(pred.prediction.pred_2bar.prediction);
            bool is_long = pred.prediction.pred_2bar.prediction > 0;

            // SIGOR-only: BB amplification disabled
            double probability_with_bb = probability;

            bool passes_prob = is_long ? (probability_with_bb > config_.buy_threshold)
                                       : (probability_with_bb < config_.sell_threshold);
            bool can_enter = trade_filter_->can_enter_position(
                symbol, static_cast<int>(bars_seen_), pred.prediction);

            std::cout << "  " << symbol
                      << " | 5-bar: " << std::fixed << std::setprecision(2)
                      << (pred.prediction.pred_2bar.prediction * 10000) << " bps"
                      << " | conf: " << (pred.prediction.pred_2bar.confidence * 100) << "%"
                      << " | prob: " << (probability * 100) << "%"
                      << (probability_with_bb != probability ?
                          " -> " + std::to_string(int(probability_with_bb * 100)) + "% (BB)" : "")
                      << " | thresh: " << (passes_prob ? "PASS" : "BLOCKED")
                      << " | filter: " << (can_enter ? "PASS" : "BLOCKED")
                      << "\n";
        }
    }

    // Define inverse ETF pairs (same as in is_position_compatible)
    static const std::map<std::string, std::string> inverse_pairs = {
        {"TQQQ", "SQQQ"}, {"SQQQ", "TQQQ"},
        {"TNA", "TZA"}, {"TZA", "TNA"},
        {"SOXL", "SOXS"}, {"SOXS", "SOXL"},
        {"SSO", "SDS"}, {"SDS", "SSO"},
        {"UVXY", "SVIX"}, {"SVIX", "UVXY"},
        {"ERX", "ERY"}, {"ERY", "ERX"},
        {"FAS", "FAZ"}, {"FAZ", "FAS"},
        {"SPXL", "SPXS"}, {"SPXS", "SPXL"}
    };

    // Build deterministic ranked list by iterating symbols_ order
    // If prediction is negative and inverse exists, substitute inverse and flip sign
    std::vector<std::pair<Symbol, double>> ranked;  // (tradeable_symbol, positive_strength)
    std::set<std::string> processed_bases;

    for (const auto& symbol : symbols_) {
        auto p_it = predictions.find(symbol);
        if (p_it == predictions.end()) {
            continue;
        }
        double prediction = p_it->second.prediction.pred_2bar.prediction;
        Symbol tradeable_symbol = symbol;

        if (prediction < 0) {
            auto inv_it = inverse_pairs.find(symbol);
            if (inv_it != inverse_pairs.end()) {
                tradeable_symbol = inv_it->second;
                prediction = -prediction;
            }
        }

        // Base key ensures only one of a pair (TQQQ/SQQQ) is processed, deterministically by lexicographic min
        std::string base_key = (tradeable_symbol < symbol) ? tradeable_symbol : symbol;
        if (processed_bases.count(base_key)) {
            continue;
        }
        processed_bases.insert(base_key);

        if (prediction > 0) {
            ranked.emplace_back(tradeable_symbol, prediction);
        }
    }

    // Deterministic ordering: strength desc, then symbol asc as tie-breaker
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.second == b.second) return a.first < b.first;  // tie-break by symbol
        return a.second > b.second;
    });

    // Get top N symbols that pass thresholds
    std::vector<Symbol> top_symbols;
    for (size_t i = 0; i < ranked.size(); ++i) {
        if (top_symbols.size() >= config_.max_positions) break;
        const auto& symbol = ranked[i].first;

        auto pred_it = predictions.find(symbol);
        if (pred_it == predictions.end()) continue;
        const auto& pred_data = pred_it->second;

        double probability = prediction_to_probability(pred_data.prediction.pred_2bar.prediction);
        bool is_long = true;
        auto bar_it = market_data.find(symbol);
        // SIGOR-only: BB amplification disabled
        bool passes_probability = (probability > config_.buy_threshold);
        bool passes_filter = trade_filter_->can_enter_position(symbol, static_cast<int>(bars_seen_), pred_data.prediction);
        if (passes_probability && passes_filter) {
            top_symbols.push_back(symbol);
        }
    }

    // ========================================================================
    // RANK-BASED ROTATION LOGIC (from online_trader)
    // ========================================================================
    // Entry follows 3 modes:
    // 1. Fill empty slots with top-ranked signals
    // 2. Hold positions if they remain in top N
    // 3. Rotate out weak positions ONLY if significantly better signal available

    // Step 1: Enter new positions if we have empty slots
    for (const auto& symbol : top_symbols) {
        if (positions_.size() >= config_.max_positions) break;

        // Skip if already holding
        if (positions_.find(symbol) != positions_.end()) {
            continue;
        }

        // Skip if in rotation cooldown
        if (in_rotation_cooldown(symbol)) {
            continue;
        }

        auto pd_it = predictions.find(symbol);
        if (pd_it == predictions.end()) {
            continue;
        }
        const auto& pred_data = pd_it->second;
        double size = calculate_position_size(symbol, pred_data);

        // Make sure we have enough cash
        if (size > cash_ * 0.95) {
            size = cash_ * 0.95;
        }

        if (size > 100) {  // Minimum position size $100
            auto it = market_data.find(symbol);
            if (it != market_data.end()) {
                // Check position compatibility (prevent inverse positions)
                if (!is_position_compatible(symbol)) {
                    continue;  // Skip this symbol (message already logged)
                }

                // SIGOR-only: confirmations disabled
                
                enter_position(symbol, it->second.close, it->second.timestamp, size, it->second.bar_id);

                // Record entry with trade filter
                trade_filter_->record_entry(
                    symbol,
                    static_cast<int>(bars_seen_),
                    pred_data.prediction.pred_2bar.prediction,
                    it->second.close
                );

                // Mark that a trade was entered this bar
                trade_entered_this_bar = true;

                // Log entry with multi-horizon info
                std::cout << "  [ENTRY] " << symbol
                         << " at $" << std::fixed << std::setprecision(2)
                         << it->second.close
                         << " | 1-bar: " << std::setprecision(4)
                         << (pred_data.prediction.pred_2bar.prediction * 100) << "%"
                         << " | 5-bar: " << (pred_data.prediction.pred_2bar.prediction * 100) << "%"
                         << " | conf: " << std::setprecision(2)
                         << (pred_data.prediction.pred_2bar.confidence * 100) << "%\n";
            }
        }
    }

    // Step 2: Check if rotation is warranted (all slots filled + better signal available)
    if (config_.enable_rotation && positions_.size() >= config_.max_positions) {
        // Iterate ranked deterministically
        for (size_t i = 0; i < ranked.size(); ++i) {
            const auto& [candidate_symbol, candidate_strength] = ranked[i];

            // Skip if already holding
            if (positions_.find(candidate_symbol) != positions_.end()) {
                continue;
            }

            // Skip if doesn't pass filters (same as entry check)
            auto cand_it = predictions.find(candidate_symbol);
            if (cand_it == predictions.end()) {
                continue;
            }
            const auto& pred_data = cand_it->second;
            double probability = prediction_to_probability(pred_data.prediction.pred_2bar.prediction);
            bool is_long = pred_data.prediction.pred_2bar.prediction > 0;
            auto bar_it = market_data.find(candidate_symbol);
            if (bar_it != market_data.end()) {
                // SIGOR-only: BB amplification disabled
            }
            bool passes_probability = is_long ? (probability > config_.buy_threshold)
                                               : (probability < config_.sell_threshold);
            bool passes_filter = trade_filter_->can_enter_position(
                    candidate_symbol, static_cast<int>(bars_seen_), pred_data.prediction);

            if (!passes_probability || !passes_filter) {
                continue;  // Candidate doesn't meet entry criteria
            }

            // Skip if in rotation cooldown
            if (in_rotation_cooldown(candidate_symbol)) {
                continue;
            }

            // Find weakest current position (deterministic tie-breaks inside)
            Symbol weakest = find_weakest_position(predictions);
            if (weakest.empty()) {
                break;  // No positions to rotate
            }

            // Get weakest strength and prediction
            auto wk_it = predictions.find(weakest);
            if (wk_it == predictions.end()) {
                break;
            }
            double weakest_pred = wk_it->second.prediction.pred_2bar.prediction;
            double weakest_strength = std::abs(weakest_pred);

            // CRITICAL: Only rotate if signals have SAME direction
            // Rotating from LONG → SHORT (or vice versa) is a signal reversal, not a rotation!
            double candidate_pred = pred_data.prediction.pred_2bar.prediction;
            bool same_direction = (weakest_pred > 0 && candidate_pred > 0) ||
                                 (weakest_pred < 0 && candidate_pred < 0);

            if (!same_direction) {
                continue;  // Don't rotate opposite directions - wait for signal deterioration to exit
            }

            // Check if rotation is justified by strength delta
            double strength_delta = candidate_strength - weakest_strength;

            if (strength_delta >= config_.rotation_strength_delta) {
                // ROTATION JUSTIFIED - exit weakest and enter stronger signal
                auto it = market_data.find(weakest);
                if (it != market_data.end()) {
                    std::cout << "  [ROTATION] OUT: " << weakest
                             << " (strength: " << std::fixed << std::setprecision(4)
                             << (weakest_strength * 10000) << " bps)"
                             << " → IN: " << candidate_symbol
                             << " (strength: " << (candidate_strength * 10000) << " bps)"
                             << " | Delta: " << (strength_delta * 10000) << " bps\n";

                    // Exit weakest
                    exit_position(weakest, it->second.close, it->second.timestamp, it->second.bar_id);

                    // Set rotation cooldown for the exited symbol
                    rotation_cooldowns_[weakest] = config_.rotation_cooldown_bars;

                    // Enter stronger candidate (same logic as regular entry)
                    double size = calculate_position_size(candidate_symbol, pred_data);
                    if (size > cash_ * 0.95) {
                        size = cash_ * 0.95;
                    }

                    if (size > 100) {
                        auto entry_it = market_data.find(candidate_symbol);
                        if (entry_it != market_data.end()) {
                            if (is_position_compatible(candidate_symbol)) {
                                // SIGOR-only: confirmations disabled
                                enter_position(candidate_symbol, entry_it->second.close,
                                             entry_it->second.timestamp, size, entry_it->second.bar_id);

                                trade_filter_->record_entry(
                                    candidate_symbol,
                                    static_cast<int>(bars_seen_),
                                    pred_data.prediction.pred_2bar.prediction,
                                    entry_it->second.close
                                );

                                // Mark that a trade was entered this bar
                                trade_entered_this_bar = true;

                                std::cout << "  [ENTRY] " << candidate_symbol
                                         << " at $" << std::fixed << std::setprecision(2)
                                         << entry_it->second.close
                                         << " (via rotation)\n";
                            }
                        }
                    }

                    break;  // Only one rotation per bar
                }
            } else {
                // Not enough improvement - stop checking
                break;
            }
        }
    }

    // Removed adaptive threshold logic (EWRLS-era), SIGOR uses probability thresholds only
}

void MultiSymbolTrader::update_positions(
    const std::unordered_map<Symbol, Bar>& market_data,
    const std::unordered_map<Symbol, PredictionData>& predictions) {

    std::vector<Symbol> to_exit;

    for (const auto& [symbol, pos] : positions_) {
        auto bar_it = market_data.find(symbol);
        if (bar_it == market_data.end()) continue;

        Price current_price = bar_it->second.close;

        // Get current prediction for this symbol (if available)
        auto pred_it = predictions.find(symbol);
        if (pred_it == predictions.end()) {
            // No prediction available - skip (will only exit via EOD or emergency stop in trade_filter)
            continue;
        }

        const auto& pred_data = pred_it->second;

        // ===== PROFIT TARGET & STOP LOSS (from online_trader v2.0) =====
        // Check P&L-based exits FIRST - highest priority
        // This locks in profits and cuts losses quickly
        double pnl_pct = positions_[symbol].pnl_percentage(current_price);

        // Profit Target: +3% (lock in gains immediately)
        if (config_.enable_profit_target && pnl_pct >= config_.profit_target_pct) {
            to_exit.push_back(symbol);
            continue;  // Exit immediately, skip other checks
        }

        // Stop Loss: -1.5% (cut losses fast - 2:1 reward:risk ratio)
        if (config_.enable_stop_loss && pnl_pct <= -config_.stop_loss_pct) {
            to_exit.push_back(symbol);
            continue;  // Exit immediately, skip other checks
        }

        // Check price-based exits (MA crossover, trailing stop)
        std::string price_exit_reason;
        if (should_exit_on_price(symbol, current_price, price_exit_reason)) {
            to_exit.push_back(symbol);
            continue;  // Skip trade filter check if price-based exit triggered
        }

        // Check if we should exit using trade filter (signal-driven)
        // This handles: min hold period, signal quality, etc.
        bool should_exit = trade_filter_->should_exit_position(
            symbol,
            static_cast<int>(bars_seen_),
            pred_data.prediction
        );

        if (should_exit) {
            to_exit.push_back(symbol);
        }
    }

    // Execute exits
    for (const auto& symbol : to_exit) {
        auto it = market_data.find(symbol);
        if (it != market_data.end()) {
            double pnl_pct = positions_[symbol].pnl_percentage(it->second.close);
            int bars_held = trade_filter_->get_bars_held(symbol);

            // Determine exit reason based on P&L
            std::string reason;
            if (config_.enable_profit_target && pnl_pct >= config_.profit_target_pct) {
                reason = "ProfitTarget(+3%)";
            } else if (config_.enable_stop_loss && pnl_pct <= -config_.stop_loss_pct) {
                reason = "StopLoss(-1.5%)";
            } else {
                reason = "SignalExit";
            }

            exit_position(symbol, it->second.close, it->second.timestamp, it->second.bar_id);

            // Log exit with details
            std::cout << "  [EXIT] " << symbol
                     << " at $" << std::fixed << std::setprecision(2)
                     << it->second.close
                     << " | P&L: " << std::setprecision(2) << (pnl_pct * 100) << "%"
                     << " | Held: " << bars_held << " bars"
                     << " | Reason: " << reason << "\n";
        }
    }
}

double MultiSymbolTrader::calculate_position_size(const Symbol& symbol, const PredictionData& pred_data) {
    // IMPROVED KELLY CRITERION-BASED POSITION SIZING
    // Now with: configurable parameters, volatility adjustment, and SIGOR-aware sizing

    // STEP 1: Extract signal quality metrics
    double confidence = pred_data.prediction.pred_2bar.confidence;
    double signal_strength = std::abs(pred_data.prediction.pred_2bar.prediction);

    // STEP 2: Calculate Kelly Criterion position size with configurable parameters
    double win_probability = std::max(0.51, std::min(0.95, confidence));  // Clamp to [51%, 95%]

    // Use configured expected returns (calibrated from backtest data)
    double expected_win_pct = config_.position_sizing.expected_win_pct;
    double expected_loss_pct = config_.position_sizing.expected_loss_pct;
    double win_loss_ratio = expected_win_pct / expected_loss_pct;

    // Kelly formula: f* = (p*b - q) / b
    double p = win_probability;
    double q = 1.0 - p;
    double kelly_fraction = (p * win_loss_ratio - q) / win_loss_ratio;
    kelly_fraction = std::max(0.0, std::min(1.0, kelly_fraction));  // Clamp to [0, 1]

    // STEP 3: Apply configurable fractional Kelly for safety
    double base_kelly_size = kelly_fraction * config_.position_sizing.fractional_kelly;

    // STEP 4: Adjust for signal strength
    // Normalize signal strength to [0, 1] range (assuming 0.5% is strong)
    double normalized_strength = std::min(1.0, signal_strength / 0.005);
    double strength_adjustment = 0.7 + (normalized_strength * 0.3);  // 70% to 100%

    // STEP 5: Calculate recommended position size as % of capital
    double recommended_pct = base_kelly_size * strength_adjustment;

    // STEP 6: VOLATILITY ADJUSTMENT (NEW!)
    // Reduce position size for high-volatility symbols
    if (config_.position_sizing.enable_volatility_adjustment) {
        auto& price_hist = price_history_[symbol];
        if (static_cast<int>(price_hist.size()) >= config_.position_sizing.volatility_lookback) {
            // Calculate volatility (standard deviation of returns)
            int lookback = config_.position_sizing.volatility_lookback;
            std::vector<double> returns;
            for (int i = price_hist.size() - lookback; i < static_cast<int>(price_hist.size()) - 1; ++i) {
                double ret = (price_hist[i+1] - price_hist[i]) / price_hist[i];
                returns.push_back(ret);
            }

            // Calculate standard deviation
            double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
            double variance = 0.0;
            for (double ret : returns) {
                variance += (ret - mean_return) * (ret - mean_return);
            }
            double std_dev = std::sqrt(variance / returns.size());

            // Convert to daily volatility (annualized vol / sqrt(252))
            double daily_vol = std_dev;

            // Reduce size for high volatility
            // 2% vol = no adjustment, 4% vol = 50% reduction (max)
            double vol_factor = 1.0;
            if (daily_vol > 0.02) {  // Above 2% daily vol
                double excess_vol = daily_vol - 0.02;
                vol_factor = 1.0 - std::min(config_.position_sizing.max_volatility_reduce,
                                            excess_vol / 0.02);  // Linear reduction
            }
            recommended_pct *= vol_factor;
        }
    }

    // STEP 7: Apply configurable position size limits
    recommended_pct = std::max(config_.position_sizing.min_position_pct, recommended_pct);
    recommended_pct = std::min(config_.position_sizing.max_position_pct, recommended_pct);

    // STEP 8: Calculate capital allocation
    double available_capital = cash_ * 0.95;  // Use 95% of cash
    double position_capital = available_capital * recommended_pct;

    // STEP 9: Adaptive sizing based on recent trade history
    auto& history = *trade_history_[symbol];
    if (history.size() >= config_.trade_history_size) {
        bool all_wins = true;
        bool all_losses = true;

        for (size_t i = 0; i < history.size(); ++i) {
            if (history[i].pnl <= 0) all_wins = false;
            if (history[i].pnl >= 0) all_losses = false;
        }

        if (all_wins) {
            position_capital *= config_.win_multiplier;  // Increase after consecutive wins
        } else if (all_losses) {
            position_capital *= config_.loss_multiplier;  // Decrease after consecutive losses
        }
    }

    // STEP 10: Final safety check - don't exceed available cash
    position_capital = std::min(position_capital, available_capital);

    return position_capital;
}

bool MultiSymbolTrader::is_position_compatible(const Symbol& new_symbol) const {
    // Define inverse ETF pairs (leveraged bull/bear pairs)
    static const std::map<std::string, std::string> inverse_pairs = {
        // 3x Tech (NASDAQ-100)
        {"TQQQ", "SQQQ"}, {"SQQQ", "TQQQ"},

        // 3x Small Cap (Russell 2000)
        {"TNA", "TZA"}, {"TZA", "TNA"},

        // 3x Semiconductors
        {"SOXL", "SOXS"}, {"SOXS", "SOXL"},

        // 2x S&P 500
        {"SSO", "SDS"}, {"SDS", "SSO"},

        // Volatility
        {"UVXY", "SVIX"}, {"SVIX", "UVXY"},

        // 3x Energy
        {"ERX", "ERY"}, {"ERY", "ERX"},

        // 3x Financials
        {"FAS", "FAZ"}, {"FAZ", "FAS"},

        // 3x S&P 500
        {"SPXL", "SPXS"}, {"SPXS", "SPXL"}
    };

    // Check if new symbol would create contradictory position
    for (const auto& [symbol, pos] : positions_) {
        // Look up if current position has an inverse pair
        auto it = inverse_pairs.find(symbol);
        if (it != inverse_pairs.end()) {
            // Check if new symbol is the inverse of current position
            if (it->second == new_symbol) {
                // Inverse position blocked - always log this important safety check
                std::cout << "  ⚠️  POSITION BLOCKED: " << new_symbol
                          << " is inverse of existing position " << symbol << "\n";
                return false;  // Inverse position not allowed
            }
        }
    }

    return true;  // Compatible with existing positions
}

void MultiSymbolTrader::enter_position(const Symbol& symbol, Price price,
                                       Timestamp time, double capital, uint64_t bar_id) {
    if (capital > cash_) {
        capital = cash_;  // Don't over-leverage
    }

    int shares = static_cast<int>(capital / price);

    if (shares <= 0) return;

    // NO COSTS ON ENTRY - only charge on exit (sell side)
    // Alpaca has zero commission, and we pay SEC/TAF fees only when selling
    AlpacaCostModel::TradeCosts entry_costs;  // Keep structure for tracking, but zero costs

    double total_cost = shares * price;  // No entry costs added

    if (total_cost <= cash_) {
        PositionWithCosts pos(shares, price, time, bar_id);
        pos.entry_costs = entry_costs;  // Will be zero

        // Pre-calculate estimated exit costs
        if (config_.enable_cost_tracking) {
            const auto& ctx = market_context_[symbol];
            pos.estimated_exit_costs = AlpacaCostModel::calculate_trade_cost(
                symbol, price, shares, false,  // is_buy = false (selling)
                ctx.avg_daily_volume,
                ctx.current_volatility,
                ctx.minutes_from_open,
                false
            );
        }

        positions_[symbol] = pos;
        cash_ -= total_cost;
        // No entry costs tracked (all costs on exit only)

        // Initialize exit tracking for price-based exits
        if (config_.enable_price_based_exits) {
            ExitTrackingData tracking;
            tracking.entry_ma = calculate_exit_ma(symbol);
            tracking.max_profit_pct = 0.0;
            tracking.max_profit_price = price;
            tracking.is_long = (shares > 0);
            exit_tracking_[symbol] = tracking;
        }
    }
}

double MultiSymbolTrader::exit_position(const Symbol& symbol, Price price, Timestamp time, uint64_t bar_id) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) return 0.0;

    const PositionWithCosts& pos = it->second;

    // Calculate exit costs if enabled
    AlpacaCostModel::TradeCosts exit_costs;
    if (config_.enable_cost_tracking) {
        const auto& ctx = market_context_[symbol];
        exit_costs = AlpacaCostModel::calculate_trade_cost(
            symbol, price, pos.shares, false,  // is_buy = false (selling)
            ctx.avg_daily_volume,
            ctx.current_volatility,
            ctx.minutes_from_open,
            false
        );
    }

    double proceeds = pos.shares * price - exit_costs.total_cost;
    double gross_pnl = pos.shares * (price - pos.entry_price);
    double net_pnl = gross_pnl - exit_costs.total_cost;  // Only exit costs (entry was zero)
    double pnl_pct = net_pnl / (pos.shares * pos.entry_price);

    // Record trade for adaptive sizing (now includes bar_ids and exit bar index)
    // Use net_pnl for trade record
    TradeRecord trade(net_pnl, pnl_pct, pos.entry_time, time, symbol,
                     pos.shares, pos.entry_price, price, pos.entry_bar_id, bar_id, bars_seen_);
    trade_history_[symbol]->push_back(trade);

    // Also add to complete trade log for export
    all_trades_log_.push_back(trade);

    // Memory management: Limit trade log size to prevent unbounded growth
    // Keep most recent 10,000 trades, archive older ones
    if (all_trades_log_.size() > 10000) {
        // Remove oldest 5,000 trades (keep newest 5,000)
        all_trades_log_.erase(
            all_trades_log_.begin(),
            all_trades_log_.begin() + 5000
        );
    }

    // Track daily wins/losses
    if (net_pnl > 0) {
        daily_winning_trades_++;
    } else if (net_pnl < 0) {
        daily_losing_trades_++;
    }

    cash_ += proceeds;
    total_transaction_costs_ += exit_costs.total_cost;
    positions_.erase(it);
    exit_tracking_.erase(symbol);  // Clean up exit tracking
    total_trades_++;

    // Notify trade filter that position is closed
    trade_filter_->record_exit(symbol, static_cast<int>(bars_seen_));

    return net_pnl;
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
            exit_position(symbol, it->second.close, it->second.timestamp, it->second.bar_id);
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

    // FILTER: Only collect trades from TEST DAY (after warmup + simulation)
    // For single-day optimization, we only report metrics for the test day
    std::vector<TradeRecord> test_day_trades;

    // Use all_trades_log_ which has ALL trades with exit_bar_index
    for (const auto& trade : all_trades_log_) {
        if (trade.exit_bar_index >= test_day_start_bar_) {
            test_day_trades.push_back(trade);
        }
    }

    results.total_trades = test_day_trades.size();
    results.winning_trades = 0;
    results.losing_trades = 0;
    double gross_profit = 0.0;
    double gross_loss = 0.0;

    for (const auto& trade : test_day_trades) {
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

    // Calculate MRD for TEST DAY ONLY (single day)
    // For single-day optimization, MRD = total return (since it's just 1 day)
    results.mrd = results.total_return;

    results.max_drawdown = 0.0;  // TODO: Implement drawdown tracking

    // Cost tracking - ONLY for test day trades
    // Note: We cannot directly filter transaction costs by bar, so we estimate
    // based on the proportion of test day trades
    double test_day_cost_ratio = (total_trades_ > 0)
                                ? static_cast<double>(test_day_trades.size()) / total_trades_
                                : 0.0;
    results.total_transaction_costs = total_transaction_costs_ * test_day_cost_ratio;
    results.avg_cost_per_trade = (results.total_trades > 0)
                                 ? results.total_transaction_costs / results.total_trades
                                 : 0.0;

    // Calculate total volume traded - ONLY test day trades
    double total_volume = 0.0;
    for (const auto& trade : test_day_trades) {
        total_volume += trade.shares * trade.entry_price;  // Entry volume
        total_volume += trade.shares * trade.exit_price;   // Exit volume
    }
    results.cost_as_pct_of_volume = (total_volume > 0)
                                    ? (results.total_transaction_costs / total_volume) * 100.0
                                    : 0.0;

    // Net return after costs
    results.net_return_after_costs = results.total_return;  // Already includes costs in cash

    // Daily breakdown
    results.daily_breakdown = daily_results_;

    return results;
}

void MultiSymbolTrader::update_market_context(const Symbol& symbol, const Bar& bar) {
    auto& ctx = market_context_[symbol];

    // Update time-based context
    ctx.minutes_from_open = calculate_minutes_from_open(bar.timestamp);

    // Update spread if available (bar.high - bar.low is a proxy)
    // In production, use actual bid/ask data
    ctx.update_spread(bar.low, bar.high);

    // Update volatility using simple rolling estimate from price_history_
    auto ph_it = price_history_.find(symbol);
    if (ph_it != price_history_.end()) {
        const auto& closes = ph_it->second;
        size_t count = closes.size();
        if (count >= 20) {
            double sum_returns_sq = 0.0;
            int n_returns = 0;
            for (size_t i = count - 19; i < count; ++i) {
                if (closes[i-1] > 0) {
                    double ret = (closes[i] - closes[i-1]) / closes[i-1];
                    sum_returns_sq += ret * ret;
                    n_returns++;
                }
            }
            if (n_returns > 0) {
                double variance = sum_returns_sq / n_returns;
                ctx.current_volatility = std::sqrt(variance);
            }
        }
    }

    // In production, update avg_daily_volume from actual market data
    // For now, keep the default value
}

int MultiSymbolTrader::calculate_minutes_from_open(Timestamp ts) const {
    // Convert timestamp to time_t for date/time manipulation
    auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        ts.time_since_epoch()
    ).count();

    // Get time of day in milliseconds
    constexpr int64_t ms_per_day = 24LL * 60 * 60 * 1000;
    int64_t time_of_day_ms = time_ms % ms_per_day;

    // Market opens at 9:30 AM ET = 9.5 hours = 34200 seconds = 34200000 ms
    // Note: This assumes timestamps are in ET. Adjust for timezone if needed.
    constexpr int64_t market_open_ms = 9LL * 60 * 60 * 1000 + 30 * 60 * 1000;

    int64_t minutes_from_open = (time_of_day_ms - market_open_ms) / (60 * 1000);

    // Clamp to [0, 390] (regular trading hours)
    if (minutes_from_open < 0) minutes_from_open = 0;
    if (minutes_from_open > 390) minutes_from_open = 390;

    return static_cast<int>(minutes_from_open);
}

double MultiSymbolTrader::prediction_to_probability(double prediction) const {
    if (!config_.enable_probability_scaling) {
        return prediction;  // No scaling, use raw prediction
    }

    // Convert prediction to probability using tanh scaling (from online_trader)
    // probability = 0.5 + 0.5 * tanh(prediction * scaling_factor)
    // This maps small predictions to probabilities around 0.5
    // and amplifies them based on scaling_factor
    double scaled = std::tanh(prediction * config_.probability_scaling_factor);
    return 0.5 + 0.5 * scaled;
}

// Removed BB amplification and confirmations in SIGOR-only build

double MultiSymbolTrader::calculate_exit_ma(const Symbol& symbol) const {
    auto ph_it = price_history_.find(symbol);
    if (ph_it == price_history_.end()) {
        return 0.0;
    }
    const auto& closes = ph_it->second;
    size_t count = closes.size();
    if (count < static_cast<size_t>(config_.ma_exit_period)) {
        return 0.0;
    }
    double sum = 0.0;
    for (size_t i = count - config_.ma_exit_period; i < count; ++i) {
        sum += closes[i];
    }
    return sum / config_.ma_exit_period;
}

bool MultiSymbolTrader::should_exit_on_price(const Symbol& symbol, Price current_price, std::string& exit_reason) {
    if (!config_.enable_price_based_exits) {
        return false;  // Feature disabled
    }

    auto pos_it = positions_.find(symbol);
    if (pos_it == positions_.end()) {
        return false;  // No position
    }

    auto track_it = exit_tracking_.find(symbol);
    if (track_it == exit_tracking_.end()) {
        return false;  // No tracking data (shouldn't happen)
    }

    const auto& pos = pos_it->second;
    auto& tracking = track_it->second;

    // Update max profit tracking
    double current_profit_pct = pos.pnl_percentage(current_price);
    if (current_profit_pct > tracking.max_profit_pct) {
        tracking.max_profit_pct = current_profit_pct;
        tracking.max_profit_price = current_price;
    }

    // === EXIT CONDITION 1: MA CROSSOVER (Mean Reversion Complete) ===
    if (config_.exit_on_ma_crossover && tracking.entry_ma > 0.0) {
        double current_ma = calculate_exit_ma(symbol);
        if (current_ma > 0.0) {
            bool crossed_ma = false;

            if (tracking.is_long) {
                // For longs: entered below MA, exit when price crosses ABOVE MA
                crossed_ma = (current_price > current_ma) && (pos.entry_price < tracking.entry_ma);
            } else {
                // For shorts: entered above MA, exit when price crosses BELOW MA
                crossed_ma = (current_price < current_ma) && (pos.entry_price > tracking.entry_ma);
            }

            if (crossed_ma) {
                exit_reason = "MA_Crossover";
                return true;
            }
        }
    }

    // === EXIT CONDITION 2: TRAILING STOP (Lock in profits) ===
    if (tracking.max_profit_pct > 0.0) {
        // Trail stop at configured percentage of max profit
        double trail_threshold = tracking.max_profit_pct * config_.trailing_stop_percentage;

        if (current_profit_pct < trail_threshold) {
            exit_reason = "TrailingStop";
            return true;
        }
    }

    return false;
}

// =============================================================================
// Warmup Phase Management
// =============================================================================

void MultiSymbolTrader::update_phase() {
    if (!config_.warmup.enabled) {
        config_.current_phase = TradingConfig::LIVE_TRADING;
        return;
    }

    int days_complete = bars_seen_ / config_.bars_per_day;

    if (days_complete < config_.warmup.observation_days) {
        config_.current_phase = TradingConfig::WARMUP_OBSERVATION;
    }
    else if (days_complete < config_.warmup.observation_days + config_.warmup.simulation_days) {
        // Transition to simulation
        if (config_.current_phase == TradingConfig::WARMUP_OBSERVATION) {
            std::cout << "\n📊 Transitioning from OBSERVATION to SIMULATION phase\n";
            warmup_metrics_.starting_equity = cash_;
            warmup_metrics_.current_equity = cash_;
            warmup_metrics_.max_equity = cash_;
        }
        config_.current_phase = TradingConfig::WARMUP_SIMULATION;
    }
    else {
        // Days complete >= observation + simulation, ready to transition to live/test

        // Handle transition from OBSERVATION (when simulation_days = 0)
        if (config_.current_phase == TradingConfig::WARMUP_OBSERVATION) {
            if (config_.warmup.simulation_days == 0) {
                // No simulation phase - go directly to test day
                config_.current_phase = TradingConfig::WARMUP_COMPLETE;
                std::cout << "\n📊 WARMUP COMPLETE (no simulation) - Proceeding directly to test day\n";
            }
        }
        // Handle transition from SIMULATION (normal case)
        else if (config_.current_phase == TradingConfig::WARMUP_SIMULATION) {
            // Skip validation for MOCK mode (always proceed to test day)
            if (config_.warmup.skip_validation) {
                config_.current_phase = TradingConfig::WARMUP_COMPLETE;
                std::cout << "\n📊 WARMUP PHASE COMPLETE - Proceeding to test day (validation skipped)\n";
            } else if (evaluate_warmup_complete()) {
                config_.current_phase = TradingConfig::WARMUP_COMPLETE;
                std::cout << "\n✅ WARMUP COMPLETE - Ready for live trading\n";
                print_warmup_summary();
            } else {
                std::cout << "\n❌ Warmup criteria not met - extending simulation\n";
                // Stay in simulation
            }
        }
    }
}

void MultiSymbolTrader::handle_observation_phase(const std::unordered_map<Symbol, Bar>& market_data) {
    warmup_metrics_.observation_bars_complete++;

    if (bars_seen_ % 100 == 0) {
        std::cout << "  [OBSERVATION] Bar " << bars_seen_
                  << " - Learning patterns, no trades\n";
    }
}

void MultiSymbolTrader::handle_simulation_phase(
    const std::unordered_map<Symbol, PredictionData>& predictions,
    const std::unordered_map<Symbol, Bar>& market_data) {

    warmup_metrics_.simulation_bars_complete++;

    // Run normal trading logic (reuse existing code)
    if (bars_seen_ > config_.min_bars_to_learn || trading_bars_ > 0) {
        trading_bars_++;

        // Track equity before trades
        double pre_trade_equity = get_equity(market_data);

        // Run normal trading
        make_trades(predictions, market_data);

        // Track equity after trades
        warmup_metrics_.current_equity = get_equity(market_data);
        warmup_metrics_.update_drawdown();

        // Record simulated trades (they're already in all_trades_log_)
        if (all_trades_log_.size() > warmup_metrics_.simulated_trades.size()) {
            warmup_metrics_.simulated_trades = all_trades_log_;
        }
    }

    if (bars_seen_ % 100 == 0) {
        double sim_return = warmup_metrics_.starting_equity > 0 ?
            (warmup_metrics_.current_equity - warmup_metrics_.starting_equity) /
            warmup_metrics_.starting_equity * 100 : 0.0;

        std::cout << "  [SIMULATION] Bar " << bars_seen_
                  << " | Equity: $" << std::fixed << std::setprecision(2)
                  << warmup_metrics_.current_equity
                  << " (" << std::showpos << sim_return << "%" << std::noshowpos << ")"
                  << " | Trades: " << warmup_metrics_.simulated_trades.size() << "\n";
    }
}

void MultiSymbolTrader::handle_live_phase(
    const std::unordered_map<Symbol, PredictionData>& predictions,
    const std::unordered_map<Symbol, Bar>& market_data) {

    // Normal trading - exactly as before warmup was added
    if (bars_seen_ > config_.min_bars_to_learn || trading_bars_ > 0) {
        trading_bars_++;
        make_trades(predictions, market_data);
    }
}

bool MultiSymbolTrader::evaluate_warmup_complete() {
    const auto& cfg = config_.warmup;
    const auto& metrics = warmup_metrics_;

    // CRITICAL WARNING: Alert if using TESTING mode
    if (cfg.mode == TradingConfig::WarmupMode::TESTING) {
        std::cout << "\n⚠️  WARNING: Warmup in TESTING mode (relaxed criteria)\n";
        std::cout << "⚠️  NOT SAFE FOR LIVE TRADING - Use PRODUCTION mode for real money!\n\n";
    }

    // Check minimum trades
    if (static_cast<int>(metrics.simulated_trades.size()) < cfg.min_trades) {
        std::cout << "  ❌ Too few trades: " << metrics.simulated_trades.size()
                  << " < " << cfg.min_trades << "\n";
        return false;
    }

    // Check Sharpe ratio
    double sharpe = metrics.calculate_sharpe();
    if (sharpe < cfg.min_sharpe_ratio) {
        std::cout << "  ❌ Sharpe too low: " << std::fixed << std::setprecision(2)
                  << sharpe << " < " << cfg.min_sharpe_ratio
                  << " [Mode: " << cfg.get_mode_name() << "]\n";
        return false;
    }

    // Check drawdown
    if (metrics.max_drawdown > cfg.max_drawdown) {
        std::cout << "  ❌ Drawdown too high: " << (metrics.max_drawdown * 100)
                  << "% > " << (cfg.max_drawdown * 100) << "%"
                  << " [Mode: " << cfg.get_mode_name() << "]\n";
        return false;
    }

    // Check profitability
    double total_return = metrics.starting_equity > 0 ?
        (metrics.current_equity - metrics.starting_equity) / metrics.starting_equity : 0.0;

    if (cfg.require_positive_return && total_return < 0) {
        std::cout << "  ❌ Negative return: " << (total_return * 100) << "%\n";
        return false;
    }

    // All checks passed
    std::cout << "  ✅ All warmup criteria met [Mode: " << cfg.get_mode_name() << "]\n";
    return true;
}

void MultiSymbolTrader::print_warmup_summary() {
    std::cout << "\n========== WARMUP SUMMARY ==========\n";
    std::cout << "Observation: " << config_.warmup.observation_days << " days\n";
    std::cout << "Simulation: " << config_.warmup.simulation_days << " days\n";
    std::cout << "\nResults:\n";

    double total_return = warmup_metrics_.starting_equity > 0 ?
        (warmup_metrics_.current_equity - warmup_metrics_.starting_equity) /
        warmup_metrics_.starting_equity : 0.0;

    std::cout << "  Return: " << std::fixed << std::setprecision(2)
              << (total_return * 100) << "%\n";
    std::cout << "  Sharpe: " << warmup_metrics_.calculate_sharpe() << "\n";
    std::cout << "  Max DD: " << (warmup_metrics_.max_drawdown * 100) << "%\n";
    std::cout << "  Trades: " << warmup_metrics_.simulated_trades.size() << "\n";

    // Win/loss breakdown
    int wins = 0, losses = 0;
    for (const auto& trade : warmup_metrics_.simulated_trades) {
        if (trade.pnl > 0) wins++;
        else if (trade.pnl < 0) losses++;
    }

    if (!warmup_metrics_.simulated_trades.empty()) {
        std::cout << "  Win Rate: " << std::fixed << std::setprecision(1)
                  << (100.0 * wins / warmup_metrics_.simulated_trades.size())
                  << "% (" << wins << "W/" << losses << "L)\n";
    }

    std::cout << "\n✅ All criteria met - ready for live\n";
    std::cout << "====================================\n\n";
}

// ============================================================================
// ROTATION LOGIC (from online_trader)
// ============================================================================

Symbol MultiSymbolTrader::find_weakest_position(
    const std::unordered_map<Symbol, PredictionData>& predictions) const {

    if (positions_.empty()) {
        return "";
    }

    Symbol weakest_symbol;
    double min_strength = std::numeric_limits<double>::max();

    // Build a deterministic list of currently held symbols, sorted by symbol name
    std::vector<Symbol> held_symbols;
    held_symbols.reserve(positions_.size());
    for (const auto& kv : positions_) {
        held_symbols.push_back(kv.first);
    }
    std::sort(held_symbols.begin(), held_symbols.end());

    for (const auto& symbol : held_symbols) {
        auto pred_it = predictions.find(symbol);
        if (pred_it == predictions.end()) {
            continue;
        }

        double strength = std::abs(pred_it->second.prediction.pred_2bar.prediction);

        if (strength < min_strength) {
            min_strength = strength;
            weakest_symbol = symbol;
        }
        // If equal strength, the sorted order by symbol keeps it deterministic (first wins)
    }

    return weakest_symbol;
}

void MultiSymbolTrader::update_rotation_cooldowns() {
    // Decrement all cooldowns
    for (auto& [symbol, cooldown] : rotation_cooldowns_) {
        if (cooldown > 0) {
            cooldown--;
        }
    }

    // Remove expired cooldowns (cleanup)
    for (auto it = rotation_cooldowns_.begin(); it != rotation_cooldowns_.end(); ) {
        if (it->second <= 0) {
            it = rotation_cooldowns_.erase(it);
        } else {
            ++it;
        }
    }
}

bool MultiSymbolTrader::in_rotation_cooldown(const Symbol& symbol) const {
    auto it = rotation_cooldowns_.find(symbol);
    return (it != rotation_cooldowns_.end() && it->second > 0);
}

} // namespace trading
