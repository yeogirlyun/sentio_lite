#include "trading/multi_symbol_trader.h"
#include "core/bar_id_utils.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cmath>

namespace trading {

MultiSymbolTrader::MultiSymbolTrader(const std::vector<Symbol>& symbols,
                                     const TradingConfig& config)
    : symbols_(symbols),
      config_(config),
      cash_(config.initial_capital),
      bars_seen_(0),
      total_trades_(0),
      total_transaction_costs_(0.0) {

    // Initialize trade filter
    trade_filter_ = std::make_unique<TradeFilter>(config_.filter_config);

    // Initialize per-symbol components
    for (const auto& symbol : symbols_) {
        // Multi-horizon predictor (1, 5, 10 bars ahead)
        predictors_[symbol] = std::make_unique<MultiHorizonPredictor>(
            symbol, config_.horizon_config);

        // Feature extractor with 50-bar lookback
        extractors_[symbol] = std::make_unique<FeatureExtractor>();

        // Trade history for adaptive sizing
        trade_history_[symbol] = std::make_unique<TradeHistory>(config_.trade_history_size);

        // Initialize market context with defaults
        market_context_[symbol] = MarketContext(
            config_.default_avg_volume,
            config_.default_volatility,
            30  // Default 30 minutes from open
        );

        // Initialize price history for multi-bar return calculations
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

    // Check 2: Validate bar_id synchronization across all symbols
    for (const auto& [symbol, bar] : market_data) {
        // Check 2a: Valid bar_id
        if (bar.bar_id == 0) {
            throw std::runtime_error(
                "CRITICAL: Symbol " + symbol + " has invalid bar_id (0) at bar " +
                std::to_string(bars_seen_) + ". Data integrity compromised!"
            );
        }

        // Check 2b: Extract timestamp from bar_id
        int64_t bar_id_timestamp_ms = extract_timestamp_ms(bar.bar_id);

        // Check 2c: Verify bar_id timestamp matches bar's actual timestamp
        int64_t bar_timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            bar.timestamp.time_since_epoch()).count();

        if (bar_id_timestamp_ms != bar_timestamp_ms) {
            throw std::runtime_error(
                "CRITICAL: Symbol " + symbol + " bar_id timestamp (" +
                std::to_string(bar_id_timestamp_ms) + ") doesn't match bar.timestamp (" +
                std::to_string(bar_timestamp_ms) + ") at bar " + std::to_string(bars_seen_) +
                ". This indicates corrupted bar_id generation!"
            );
        }

        // Check 2d: Cross-symbol timestamp synchronization
        if (reference_timestamp_ms == -1) {
            // First valid bar - use as reference
            reference_timestamp_ms = bar_id_timestamp_ms;
            reference_symbol = symbol;
        } else {
            // Verify timestamp matches reference (all symbols must be at same time)
            if (bar_id_timestamp_ms != reference_timestamp_ms) {
                throw std::runtime_error(
                    "CRITICAL: Bar timestamp mismatch! " + reference_symbol +
                    " has timestamp " + std::to_string(reference_timestamp_ms) +
                    " but " + symbol + " has timestamp " + std::to_string(bar_id_timestamp_ms) +
                    " at bar " + std::to_string(bars_seen_) +
                    ". This indicates bar misalignment across symbols - CANNOT TRADE SAFELY!"
                );
            }
        }

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
        auto features = extractors_[symbol]->extract(bar);

        if (features.has_value()) {
            // Make multi-horizon prediction
            auto pred = predictors_[symbol]->predict(features.value());
            predictions[symbol] = {pred, features.value(), bar.close};

            // Update predictor with realized returns
            if (bars_seen_ > 1) {
                auto& history = price_history_[symbol];

                // Calculate 1-bar return
                double return_1bar = std::numeric_limits<double>::quiet_NaN();
                if (history.size() >= 2) {
                    double prev_price = history[history.size() - 2];
                    if (prev_price > 0) {
                        return_1bar = (bar.close - prev_price) / prev_price;
                    }
                }

                // Calculate 5-bar return (if available)
                double return_5bar = std::numeric_limits<double>::quiet_NaN();
                if (history.size() >= 6) {
                    double price_5bars_ago = history[history.size() - 6];
                    if (price_5bars_ago > 0) {
                        return_5bar = (bar.close - price_5bars_ago) / price_5bars_ago;
                    }
                }

                // Calculate 10-bar return (if available)
                double return_10bar = std::numeric_limits<double>::quiet_NaN();
                if (history.size() >= 11) {
                    double price_10bars_ago = history[history.size() - 11];
                    if (price_10bars_ago > 0) {
                        return_10bar = (bar.close - price_10bars_ago) / price_10bars_ago;
                    }
                }

                // Update multi-horizon predictor
                predictors_[symbol]->update(features.value(), return_1bar, return_5bar, return_10bar);
            }
        }
    }

    // Step 4: Update trade filter bars held counter
    trade_filter_->update_bars_held(static_cast<int>(bars_seen_));

    // Step 5: Update existing positions (check exit conditions with trade filter)
    update_positions(market_data, predictions);

    // Step 6: Make trading decisions (after warmup period)
    if (bars_seen_ > config_.min_bars_to_learn) {
        make_trades(predictions, market_data);
    }

    // Step 7: EOD liquidation
    if (config_.eod_liquidation &&
        bars_seen_ % config_.bars_per_day == config_.bars_per_day - 1) {
        liquidate_all(market_data, "EOD");
    }
}

void MultiSymbolTrader::make_trades(const std::unordered_map<Symbol, PredictionData>& predictions,
                                    const std::unordered_map<Symbol, Bar>& market_data) {

    // Enhanced Debug: Detailed trade analysis (every 50 bars when no positions)
    static size_t debug_counter = 0;
    if (positions_.empty() && bars_seen_ % 50 == 0) {
        std::cout << "\n[TRADE ANALYSIS] Bar " << bars_seen_ << ":\n";

        // Sort by 5-bar prediction strength
        std::vector<std::pair<Symbol, const PredictionData*>> debug_ranked;
        for (const auto& [symbol, pred] : predictions) {
            debug_ranked.emplace_back(symbol, &pred);
        }
        std::sort(debug_ranked.begin(), debug_ranked.end(),
                  [](const auto& a, const auto& b) {
                      return std::abs(a.second->prediction.pred_5bar.prediction) >
                             std::abs(b.second->prediction.pred_5bar.prediction);
                  });

        // Show top 5 with detailed rejection reasons
        for (size_t i = 0; i < std::min(size_t(5), debug_ranked.size()); ++i) {
            const auto& symbol = debug_ranked[i].first;
            const auto& pred = *debug_ranked[i].second;

            // Calculate probability
            double probability = prediction_to_probability(pred.prediction.pred_5bar.prediction);
            bool is_long = pred.prediction.pred_5bar.prediction > 0;

            // Apply BB amplification if data available
            auto bar_it = market_data.find(symbol);
            double probability_with_bb = probability;
            if (bar_it != market_data.end()) {
                probability_with_bb = apply_bb_amplification(probability, symbol, bar_it->second, is_long);
            }

            bool passes_prob = is_long ? (probability_with_bb > config_.buy_threshold)
                                       : (probability_with_bb < config_.sell_threshold);
            bool can_enter = trade_filter_->can_enter_position(
                symbol, static_cast<int>(bars_seen_), pred.prediction);

            std::cout << "  " << symbol
                      << " | 5-bar: " << std::fixed << std::setprecision(2)
                      << (pred.prediction.pred_5bar.prediction * 10000) << " bps"
                      << " | conf: " << (pred.prediction.pred_5bar.confidence * 100) << "%"
                      << " | prob: " << (probability * 100) << "%"
                      << (probability_with_bb != probability ?
                          " -> " + std::to_string(int(probability_with_bb * 100)) + "% (BB)" : "")
                      << " | thresh: " << (passes_prob ? "PASS" : "BLOCKED")
                      << " | filter: " << (can_enter ? "PASS" : "BLOCKED")
                      << "\n";
        }
    }

    // Rank symbols by 5-bar predicted return (absolute value for rotation)
    std::vector<std::pair<Symbol, double>> ranked;
    for (const auto& [symbol, pred] : predictions) {
        // Use ABSOLUTE VALUE of 5-bar prediction for ranking (strongest signals first)
        ranked.emplace_back(symbol, std::abs(pred.prediction.pred_5bar.prediction));
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Get top N symbols that pass probability threshold
    std::vector<Symbol> top_symbols;
    for (size_t i = 0; i < ranked.size(); ++i) {
        // Stop if we have enough symbols
        if (top_symbols.size() >= config_.max_positions) {
            break;
        }

        const auto& symbol = ranked[i].first;
        const auto& pred_data = predictions.at(symbol);

        // Convert prediction to probability
        double probability = prediction_to_probability(pred_data.prediction.pred_5bar.prediction);

        // Apply Bollinger Band amplification if enabled
        bool is_long = pred_data.prediction.pred_5bar.prediction > 0;
        auto bar_it = market_data.find(symbol);
        if (bar_it != market_data.end()) {
            probability = apply_bb_amplification(probability, symbol, bar_it->second, is_long);
        }

        // Check probability threshold (from online_trader)
        bool passes_probability = is_long ? (probability > config_.buy_threshold)
                                           : (probability < config_.sell_threshold);

        // Also check trade filter (optional, can be disabled by setting min_prediction_for_entry = 0)
        bool passes_filter = trade_filter_->can_enter_position(
                symbol, static_cast<int>(bars_seen_), pred_data.prediction);

        if (passes_probability && passes_filter) {
            top_symbols.push_back(symbol);
        }
    }

    // ROTATION LOGIC REMOVED: Positions are now only exited when signals deteriorate
    // This is handled in update_positions() which checks:
    // - Emergency stop loss
    // - Profit target reached
    // - Signal quality degraded
    // - Signal reversed direction
    // - Maximum hold period reached
    //
    // Removing forced rotation to "top 3" reduces excessive churning
    // and allows positions to hold until their signals truly deteriorate.

    // Enter new positions if we have capacity and sufficient cash
    for (const auto& symbol : top_symbols) {
        if (positions_.size() >= config_.max_positions) break;

        if (positions_.find(symbol) == positions_.end()) {
            const auto& pred_data = predictions.at(symbol);

            double size = calculate_position_size(symbol, pred_data);

            // Make sure we have enough cash
            if (size > cash_ * 0.95) {
                size = cash_ * 0.95;
            }

            if (size > 100) {  // Minimum position size $100
                auto it = market_data.find(symbol);
                if (it != market_data.end()) {
                    enter_position(symbol, it->second.close, it->second.timestamp, size, it->second.bar_id);

                    // Record entry with trade filter
                    trade_filter_->record_entry(
                        symbol,
                        static_cast<int>(bars_seen_),
                        pred_data.prediction.pred_5bar.prediction,
                        it->second.close
                    );

                    // Log entry with multi-horizon info
                    std::cout << "  [ENTRY] " << symbol
                             << " at $" << std::fixed << std::setprecision(2)
                             << it->second.close
                             << " | 1-bar: " << std::setprecision(4)
                             << (pred_data.prediction.pred_1bar.prediction * 100) << "%"
                             << " | 5-bar: " << (pred_data.prediction.pred_5bar.prediction * 100) << "%"
                             << " | conf: " << std::setprecision(2)
                             << (pred_data.prediction.pred_5bar.confidence * 100) << "%\n";
                }
            }
        }
    }
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
            // No prediction available, use fallback logic (emergency stop loss only)
            double pnl_pct = pos.pnl_percentage(current_price);
            if (pnl_pct <= config_.stop_loss_pct) {
                to_exit.push_back(symbol);
            }
            continue;
        }

        const auto& pred_data = pred_it->second;

        // Check if we should exit using trade filter
        // This handles: emergency stops, profit targets, max hold, signal quality, etc.
        bool should_exit = trade_filter_->should_exit_position(
            symbol,
            static_cast<int>(bars_seen_),
            pred_data.prediction,
            current_price
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

            // Determine exit reason for logging
            std::string reason = "Unknown";
            if (pnl_pct < config_.filter_config.emergency_stop_loss_pct) {
                reason = "EmergencyStop";
            } else if (pnl_pct > config_.filter_config.profit_target_multiple * 0.01) {
                reason = "ProfitTarget";
            } else if (bars_held >= config_.filter_config.max_bars_to_hold) {
                reason = "MaxHold";
            } else if (bars_held >= config_.filter_config.min_bars_to_hold) {
                reason = "SignalExit";
            } else {
                reason = "EarlyExit";
            }

            exit_position(symbol, it->second.close, it->second.timestamp, it->second.bar_id);

            // Record exit with trade filter
            trade_filter_->record_exit(symbol, static_cast<int>(bars_seen_));

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
    // KELLY CRITERION-BASED POSITION SIZING (adapted from online_trader)
    // Formula: f* = (p*b - q) / b
    // where: p = win probability, q = 1-p, b = win/loss ratio

    // STEP 1: Extract signal quality metrics
    double confidence = pred_data.prediction.pred_5bar.confidence;  // Use 5-bar confidence
    double signal_strength = std::abs(pred_data.prediction.pred_5bar.prediction);

    // STEP 2: Calculate Kelly Criterion position size
    // Use confidence as win probability
    double win_probability = std::max(0.51, std::min(0.95, confidence));  // Clamp to [51%, 95%]

    // Expected win/loss based on historical performance (can be calibrated)
    double expected_win_pct = 0.02;   // 2% average win
    double expected_loss_pct = 0.015; // 1.5% average loss
    double win_loss_ratio = expected_win_pct / expected_loss_pct;  // b = 1.33

    // Kelly formula: f* = (p*b - q) / b
    double p = win_probability;
    double q = 1.0 - p;
    double kelly_fraction = (p * win_loss_ratio - q) / win_loss_ratio;
    kelly_fraction = std::max(0.0, std::min(1.0, kelly_fraction));  // Clamp to [0, 1]

    // STEP 3: Apply fractional Kelly for safety (25% of full Kelly)
    double fractional_kelly = 0.25;  // Conservative: use 25% of full Kelly
    double base_kelly_size = kelly_fraction * fractional_kelly;

    // STEP 4: Adjust for signal strength
    // Normalize signal strength to [0, 1] range (assuming 0.5% is strong)
    double normalized_strength = std::min(1.0, signal_strength / 0.005);
    double strength_adjustment = 0.7 + (normalized_strength * 0.3);  // 70% to 100%

    // STEP 5: Calculate recommended position size as % of capital
    double recommended_pct = base_kelly_size * strength_adjustment;

    // STEP 6: Apply position size limits
    recommended_pct = std::max(0.05, recommended_pct);  // Min 5% per position
    recommended_pct = std::min(0.25, recommended_pct);  // Max 25% per position

    // STEP 7: Calculate capital allocation
    size_t active_positions = positions_.size();

    // Available capital: use 95% of cash to leave some buffer
    double available_capital = cash_ * 0.95;
    double position_capital = available_capital * recommended_pct;

    // STEP 8: Adaptive sizing based on recent trade history
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

    // STEP 9: Final safety check - don't exceed available cash
    position_capital = std::min(position_capital, available_capital);

    return position_capital;
}

void MultiSymbolTrader::enter_position(const Symbol& symbol, Price price,
                                       Timestamp time, double capital, uint64_t bar_id) {
    if (capital > cash_) {
        capital = cash_;  // Don't over-leverage
    }

    int shares = static_cast<int>(capital / price);

    if (shares <= 0) return;

    // Calculate entry costs if enabled
    AlpacaCostModel::TradeCosts entry_costs;
    if (config_.enable_cost_tracking) {
        const auto& ctx = market_context_[symbol];
        entry_costs = AlpacaCostModel::calculate_trade_cost(
            symbol, price, shares, true,  // is_buy = true
            ctx.avg_daily_volume,
            ctx.current_volatility,
            ctx.minutes_from_open,
            false  // is_short_sale = false
        );
    }

    double total_cost = shares * price + entry_costs.total_cost;

    if (total_cost <= cash_) {
        PositionWithCosts pos(shares, price, time, bar_id);
        pos.entry_costs = entry_costs;

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
        total_transaction_costs_ += entry_costs.total_cost;
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
    double net_pnl = gross_pnl - pos.entry_costs.total_cost - exit_costs.total_cost;
    double pnl_pct = net_pnl / (pos.shares * pos.entry_price + pos.entry_costs.total_cost);

    // Record trade for adaptive sizing (now includes bar_ids)
    // Use net_pnl for trade record
    TradeRecord trade(net_pnl, pnl_pct, pos.entry_time, time, symbol,
                     pos.shares, pos.entry_price, price, pos.entry_bar_id, bar_id);
    trade_history_[symbol]->push_back(trade);

    // Also add to complete trade log for export
    all_trades_log_.push_back(trade);

    cash_ += proceeds;
    total_transaction_costs_ += exit_costs.total_cost;
    positions_.erase(it);
    total_trades_++;

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

    // Cost tracking
    results.total_transaction_costs = total_transaction_costs_;
    results.avg_cost_per_trade = (results.total_trades > 0)
                                 ? total_transaction_costs_ / results.total_trades
                                 : 0.0;

    // Calculate total volume traded
    double total_volume = 0.0;
    for (const auto& trade : all_trades_log_) {
        total_volume += trade.shares * trade.entry_price;  // Entry volume
        total_volume += trade.shares * trade.exit_price;   // Exit volume
    }
    results.cost_as_pct_of_volume = (total_volume > 0)
                                    ? (total_transaction_costs_ / total_volume) * 100.0
                                    : 0.0;

    // Net return after costs
    results.net_return_after_costs = results.total_return;  // Already includes costs in cash

    return results;
}

void MultiSymbolTrader::update_market_context(const Symbol& symbol, const Bar& bar) {
    auto& ctx = market_context_[symbol];

    // Update time-based context
    ctx.minutes_from_open = calculate_minutes_from_open(bar.timestamp);

    // Update spread if available (bar.high - bar.low is a proxy)
    // In production, use actual bid/ask data
    ctx.update_spread(bar.low, bar.high);

    // Update volatility using simple rolling estimate
    // In production, use more sophisticated volatility estimation
    // For now, keep default or calculate from recent price changes
    if (extractors_[symbol]->bar_count() >= 20) {
        const auto& history = extractors_[symbol]->history();
        size_t count = history.size();
        if (count >= 20) {
            // Calculate 20-bar volatility
            double sum_returns_sq = 0.0;
            int n_returns = 0;
            for (size_t i = count - 19; i < count; ++i) {
                if (history[i-1].close > 0) {
                    double ret = (history[i].close - history[i-1].close) / history[i-1].close;
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

MultiSymbolTrader::BBands MultiSymbolTrader::calculate_bollinger_bands(
    const Symbol& symbol, const Bar& current_bar) const {

    BBands bands;

    // Get recent price history
    auto it = extractors_.find(symbol);
    if (it == extractors_.end()) {
        return bands;  // No data
    }

    const auto& history = it->second->history();
    size_t count = history.size();

    if (count < static_cast<size_t>(config_.bb_period)) {
        return bands;  // Not enough data
    }

    // Calculate SMA (middle band)
    double sum = 0.0;
    for (size_t i = count - config_.bb_period; i < count; ++i) {
        sum += history[i].close;
    }
    bands.middle = sum / config_.bb_period;

    // Calculate standard deviation
    double sum_sq = 0.0;
    for (size_t i = count - config_.bb_period; i < count; ++i) {
        double diff = history[i].close - bands.middle;
        sum_sq += diff * diff;
    }
    double std_dev = std::sqrt(sum_sq / config_.bb_period);

    // Calculate upper and lower bands
    bands.upper = bands.middle + (config_.bb_std_dev * std_dev);
    bands.lower = bands.middle - (config_.bb_std_dev * std_dev);

    return bands;
}

double MultiSymbolTrader::apply_bb_amplification(
    double probability, const Symbol& symbol,
    const Bar& bar, bool is_long) const {

    if (!config_.enable_bb_amplification) {
        return probability;  // No amplification
    }

    // Calculate BB bands
    BBands bands = calculate_bollinger_bands(symbol, bar);

    if (bands.middle == 0.0) {
        return probability;  // No valid bands
    }

    double current_price = bar.close;
    double band_width = bands.upper - bands.lower;

    if (band_width == 0.0) {
        return probability;  // Invalid band width
    }

    // Calculate proximity to bands
    // For long: boost if near lower band (oversold)
    // For short: boost if near upper band (overbought)

    if (is_long) {
        // Distance from lower band
        double distance_from_lower = (current_price - bands.lower) / band_width;

        // If within proximity threshold of lower band, amplify
        if (distance_from_lower < config_.bb_proximity_threshold) {
            double boost = config_.bb_amplification_factor * (1.0 - distance_from_lower / config_.bb_proximity_threshold);
            return std::min(0.99, probability + boost);
        }
    } else {
        // Distance from upper band
        double distance_from_upper = (bands.upper - current_price) / band_width;

        // If within proximity threshold of upper band, amplify
        if (distance_from_upper < config_.bb_proximity_threshold) {
            double boost = config_.bb_amplification_factor * (1.0 - distance_from_upper / config_.bb_proximity_threshold);
            return std::max(0.01, probability - boost);  // For shorts, reduce probability
        }
    }

    return probability;
}

} // namespace trading
