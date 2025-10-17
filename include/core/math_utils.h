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
