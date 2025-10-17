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
    static std::vector<Bar> load_csv(const std::string& path, const std::string& symbol = "");
    static std::vector<Bar> load_binary(const std::string& path, const std::string& symbol = "");
    static bool ends_with(const std::string& str, const std::string& suffix);
    static std::string extract_symbol_from_path(const std::string& path);
};

} // namespace trading
