/**
 * Comprehensive Detector Testing Suite
 *
 * Tests all proposed detectors on historical data and generates
 * integration recommendations based on performance metrics
 */

#include "detector_backtest_framework.h"
#include "squeeze_detector.h"
#include "donchian_detector.h"
#include "rsi2_detector.h"
#include "vwap_bands_detector.h"
#include <algorithm>
#include <numeric>

struct DetectorRanking {
    std::string name;
    double overall_score;
    int criteria_passed;
    BacktestMetrics metrics;
    std::string integration_recommendation;
};

class DetectorEvaluator {
private:
    std::vector<Symbol> test_symbols = {
        "TQQQ", "SQQQ", "TNA", "TZA",
        "SOXL", "SOXS", "SPXL", "SPXS"
    };

    std::vector<DetectorRanking> rankings;

    double calculate_overall_score(const BacktestMetrics& m) {
        // Weighted scoring: Sharpe (40%), Win Rate (30%), Profit Factor (20%), Drawdown (10%)
        double sharpe_score = std::min(m.sharpe_ratio / 2.0, 1.0) * 0.40;
        double winrate_score = std::min(m.win_rate / 60.0, 1.0) * 0.30;
        double pf_score = std::min(m.profit_factor / 2.0, 1.0) * 0.20;
        double dd_score = (1.0 - std::min(m.max_drawdown_pct / 20.0, 1.0)) * 0.10;

        return (sharpe_score + winrate_score + pf_score + dd_score) * 100.0;
    }

    int count_criteria_passed(const BacktestMetrics& m) {
        int passes = 0;
        if (m.win_rate >= 52.0) passes++;
        if (m.sharpe_ratio >= 1.0) passes++;
        if (m.max_drawdown_pct <= 15.0) passes++;
        if (m.profit_factor >= 1.3) passes++;
        return passes;
    }

    std::string get_recommendation(int criteria_passed, double overall_score) {
        if (criteria_passed >= 3 && overall_score >= 70.0) {
            return "STRONG - Immediate integration recommended";
        } else if (criteria_passed >= 2 && overall_score >= 55.0) {
            return "MODERATE - Parameter optimization needed";
        } else if (criteria_passed >= 2) {
            return "WEAK - Consider with reservations";
        } else {
            return "REJECT - Does not meet minimum criteria";
        }
    }

public:
    template<typename DetectorType>
    void test_detector(const std::string& name, DetectorType& detector) {
        std::cout << "\n\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  Testing: " << std::left << std::setw(48) << name << "║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

        DetectorBacktester backtester;

        // Load data
        std::cout << "Loading historical data...\n";
        if (!backtester.load_data(test_symbols)) {
            std::cerr << "Failed to load data for " << name << "\n";
            return;
        }

        // Run backtest on each symbol
        std::cout << "\nRunning backtest across " << test_symbols.size() << " symbols...\n";
        for (const auto& symbol : test_symbols) {
            backtester.run_backtest(detector, symbol, 1000, 5000);  // Sample range
        }

        // Get results
        const auto& metrics = backtester.get_metrics();
        metrics.print_summary(name);

        // Export trades
        std::string trades_file = "results/" + name + "_trades.csv";
        backtester.export_trades(trades_file);

        // Rank
        DetectorRanking ranking;
        ranking.name = name;
        ranking.metrics = metrics;
        ranking.criteria_passed = count_criteria_passed(metrics);
        ranking.overall_score = calculate_overall_score(metrics);
        ranking.integration_recommendation = get_recommendation(
            ranking.criteria_passed, ranking.overall_score
        );

        rankings.push_back(ranking);
    }

    void print_final_rankings() {
        std::cout << "\n\n";
        std::cout << "═══════════════════════════════════════════════════════════════\n";
        std::cout << "  FINAL DETECTOR RANKINGS & INTEGRATION RECOMMENDATIONS\n";
        std::cout << "═══════════════════════════════════════════════════════════════\n\n";

        // Sort by overall score
        std::sort(rankings.begin(), rankings.end(),
            [](const DetectorRanking& a, const DetectorRanking& b) {
                return a.overall_score > b.overall_score;
            });

        std::cout << std::left << std::setw(35) << "Detector"
                  << std::right << std::setw(8) << "Score"
                  << std::setw(8) << "Pass"
                  << std::setw(10) << "Sharpe"
                  << std::setw(10) << "WinRate%"
                  << "\n";
        std::cout << std::string(70, '─') << "\n";

        for (const auto& r : rankings) {
            std::cout << std::left << std::setw(35) << r.name
                      << std::right << std::setw(8) << std::fixed << std::setprecision(1) << r.overall_score
                      << std::setw(8) << r.criteria_passed << "/4"
                      << std::setw(10) << std::fixed << std::setprecision(2) << r.metrics.sharpe_ratio
                      << std::setw(10) << std::fixed << std::setprecision(1) << r.metrics.win_rate
                      << "\n";
        }

        std::cout << "\n";
        std::cout << "INTEGRATION PLAN:\n";
        std::cout << std::string(70, '─') << "\n";

        int tier1 = 0, tier2 = 0, tier3 = 0;

        for (const auto& r : rankings) {
            std::cout << "\n" << r.name << ":\n";
            std::cout << "  Status: " << r.integration_recommendation << "\n";

            if (r.integration_recommendation.find("STRONG") != std::string::npos) {
                std::cout << "  → Add to SIGOR immediately with weight 1.0\n";
                std::cout << "  → Expected to improve Sharpe by ~" << (r.metrics.sharpe_ratio * 0.15)
                          << "\n";
                tier1++;
            } else if (r.integration_recommendation.find("MODERATE") != std::string::npos) {
                std::cout << "  → Optimize parameters, then add with weight 0.8\n";
                std::cout << "  → Run parameter sweep on: [list key parameters]\n";
                tier2++;
            } else if (r.integration_recommendation.find("WEAK") != std::string::npos) {
                std::cout << "  → Consider for Phase 2 integration\n";
                std::cout << "  → Needs significant enhancement\n";
                tier3++;
            } else {
                std::cout << "  → Do not integrate\n";
            }
        }

        std::cout << "\n";
        std::cout << "SUMMARY:\n";
        std::cout << "  Tier 1 (Immediate):  " << tier1 << " detectors\n";
        std::cout << "  Tier 2 (Optimize):   " << tier2 << " detectors\n";
        std::cout << "  Tier 3 (Phase 2):    " << tier3 << " detectors\n";
        std::cout << "  Rejected:            " << (rankings.size() - tier1 - tier2 - tier3) << " detectors\n";

        std::cout << "\n═══════════════════════════════════════════════════════════════\n\n";
    }

    void generate_integration_code() {
        std::cout << "INTEGRATION CODE TEMPLATE:\n";
        std::cout << "─────────────────────────────────────────────────\n\n";

        for (const auto& r : rankings) {
            if (r.integration_recommendation.find("STRONG") != std::string::npos ||
                r.integration_recommendation.find("MODERATE") != std::string::npos) {

                std::cout << "// Add to sigor_strategy.h:\n";
                std::cout << "std::unique_ptr<" << r.name << "Detector> "
                          << r.name << "_det;\n\n";

                std::cout << "// Add to detector initialization:\n";
                std::cout << r.name << "_det = std::make_unique<" << r.name
                          << "Detector>();\n\n";

                std::cout << "// Add to update loop:\n";
                std::cout << r.name << "_det->update(bar, prev_bar, history);\n";
                std::cout << "double " << r.name << "_signal = " << r.name
                          << "_det->get_signal();\n";
                std::cout << "double " << r.name << "_conf = " << r.name
                          << "_det->get_confidence();\n\n";

                std::cout << "// Add to fusion (with suggested weight):\n";
                double weight = (r.criteria_passed >= 3) ? 1.0 : 0.8;
                std::cout << "fusion_score += " << r.name << "_signal * "
                          << r.name << "_conf * " << std::fixed << std::setprecision(1)
                          << weight << ";\n";
                std::cout << "total_weight += " << weight << ";\n\n";
                std::cout << "─────────────────────────────────────────────────\n\n";
            }
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  SIGOR Detector Evaluation Suite                              ║\n";
    std::cout << "║  Testing proposed detectors for integration recommendations   ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";

    DetectorEvaluator evaluator;

    // Test each detector
    {
        std::cout << "\n[1/4] TTM Squeeze/Expansion Detector\n";
        SqueezeDetector squeeze_det;
        evaluator.test_detector("TTM_Squeeze_Expansion", squeeze_det);
    }

    {
        std::cout << "\n[2/4] Donchian/Prior-Day Breakout Detector\n";
        DonchianDetector donchian_det;
        evaluator.test_detector("Donchian_Breakout", donchian_det);
    }

    {
        std::cout << "\n[3/4] RSI(2) Pullback Detector\n";
        RSI2Detector rsi2_det;
        evaluator.test_detector("RSI2_Pullback", rsi2_det);
    }

    {
        std::cout << "\n[4/4] VWAP Bands Mean-Reversion Detector\n";
        VWAPBandsDetector vwap_bands_det;
        evaluator.test_detector("VWAP_Bands_Reversion", vwap_bands_det);
    }

    // Print rankings and recommendations
    evaluator.print_final_rankings();
    evaluator.generate_integration_code();

    std::cout << "\nDetector evaluation complete.\n";
    std::cout << "Review results above for integration decisions.\n\n";

    return 0;
}
