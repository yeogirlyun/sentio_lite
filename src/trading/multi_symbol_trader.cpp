#include "trading/multi_symbol_trader.h"
#include "core/bar_id_utils.h"
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
        // Predictor with 33 features (8 time + 25 technical)
        predictors_[symbol] = std::make_unique<OnlinePredictor>(33, config_.lambda);

        // Feature extractor with 50-bar lookback
        extractors_[symbol] = std::make_unique<FeatureExtractor>();

        // Trade history for adaptive sizing
        trade_history_[symbol] = std::make_unique<TradeHistory>(config_.trade_history_size);
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

    // Debug: Log predictions if no positions (every 100 bars)
    static size_t debug_counter = 0;
    if (positions_.empty() && debug_counter++ % 100 == 0) {
        std::cout << "  [DEBUG] Top predictions at bar " << bars_seen_ << ":\n";
        std::vector<std::pair<Symbol, double>> debug_ranked;
        for (const auto& [symbol, pred] : predictions) {
            debug_ranked.emplace_back(symbol, pred.predicted_return);
        }
        std::sort(debug_ranked.begin(), debug_ranked.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        for (size_t i = 0; i < std::min(size_t(3), debug_ranked.size()); ++i) {
            std::cout << "    " << debug_ranked[i].first << ": "
                     << std::fixed << std::setprecision(6)
                     << debug_ranked[i].second << " (threshold: "
                     << config_.min_prediction_threshold << ")\n";
        }
    }

    // Rank symbols by predicted return
    std::vector<std::pair<Symbol, double>> ranked;
    for (const auto& [symbol, pred] : predictions) {
        ranked.emplace_back(symbol, pred.predicted_return);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Get top N symbols that meet threshold
    std::vector<Symbol> top_symbols;
    for (size_t i = 0; i < std::min(ranked.size(), config_.max_positions); ++i) {
        // IMPORTANT: Only add if prediction exceeds threshold
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
            exit_position(symbol, it->second.close, it->second.timestamp, it->second.bar_id);
        }
    }

    // Enter new positions if we have capacity and sufficient cash
    for (const auto& [symbol, pred_return] : ranked) {
        if (positions_.size() >= config_.max_positions) break;

        if (positions_.find(symbol) == positions_.end() &&
            pred_return > config_.min_prediction_threshold) {

            double size = calculate_position_size(symbol);

            // Make sure we have enough cash
            if (size > cash_ * 0.95) {
                size = cash_ * 0.95;
            }

            if (size > 100) {  // Minimum position size $100
                auto it = market_data.find(symbol);
                if (it != market_data.end()) {
                    enter_position(symbol, it->second.close, it->second.timestamp, size, it->second.bar_id);

                    // Log entry
                    std::cout << "  [ENTRY] " << symbol
                             << " at $" << std::fixed << std::setprecision(2)
                             << it->second.close
                             << " (pred: " << std::setprecision(4)
                             << (pred_return * 100) << "%)\n";
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
            exit_position(symbol, it->second.close, it->second.timestamp, it->second.bar_id);
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
                                       Timestamp time, double capital, uint64_t bar_id) {
    if (capital > cash_) {
        capital = cash_;  // Don't over-leverage
    }

    int shares = static_cast<int>(capital / price);
    if (shares > 0 && capital <= cash_) {
        positions_[symbol] = Position(shares, price, time, bar_id);
        cash_ -= shares * price;
    }
}

double MultiSymbolTrader::exit_position(const Symbol& symbol, Price price, Timestamp time, uint64_t bar_id) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) return 0.0;

    const Position& pos = it->second;
    double proceeds = pos.shares * price;
    double pnl = proceeds - (pos.shares * pos.entry_price);
    double pnl_pct = pnl / (pos.shares * pos.entry_price);

    // Record trade for adaptive sizing (now includes bar_ids)
    TradeRecord trade(pnl, pnl_pct, pos.entry_time, time, symbol,
                     pos.shares, pos.entry_price, price, pos.entry_bar_id, bar_id);
    trade_history_[symbol]->push_back(trade);

    // Also add to complete trade log for export
    all_trades_log_.push_back(trade);

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

    return results;
}

} // namespace trading
