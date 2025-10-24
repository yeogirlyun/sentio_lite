#include "utils/data_loader.h"
#include "core/bar_id_utils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <filesystem>

namespace trading {

std::vector<Bar> DataLoader::load(const std::string& path) {
    // Extract symbol from filename (e.g., "TQQQ.bin" -> "TQQQ")
    std::string symbol = extract_symbol_from_path(path);

    if (ends_with(path, ".csv")) {
        return load_csv(path, symbol);
    } else if (ends_with(path, ".bin")) {
        return load_binary(path, symbol);
    } else {
        throw std::runtime_error("Unsupported file format: " + path +
                               " (supported: .csv, .bin)");
    }
}

std::vector<Bar> DataLoader::load_csv(const std::string& path, const std::string& symbol) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + path);
    }

    std::vector<Bar> bars;
    std::string line;

    // Skip header
    std::getline(file, line);

    int line_num = 1;
    while (std::getline(file, line)) {
        line_num++;
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string field;
        std::vector<std::string> fields;

        while (std::getline(iss, field, ',')) {
            fields.push_back(field);
        }

        // Expect: timestamp_ms,symbol,open,high,low,close,volume
        // Or: timestamp_ms,open,high,low,close,volume (no symbol)
        if (fields.size() < 6) {
            std::cerr << "Warning: Skipping malformed line " << line_num << ": "
                     << line << std::endl;
            continue;
        }

        try {
            Bar bar;
            size_t field_idx = 0;

            // Parse timestamp (milliseconds since epoch)
            int64_t timestamp_ms = std::stoll(fields[field_idx++]);
            bar.timestamp = from_timestamp_ms(timestamp_ms);

            // Check if symbol field exists (7 fields vs 6)
            if (fields.size() >= 7) {
                // Skip symbol field for now
                field_idx++;
            }

            // Set symbol and generate bar_id
            bar.symbol = symbol;
            bar.bar_id = generate_bar_id(timestamp_ms, symbol);

            // Parse OHLCV
            bar.open = std::stod(fields[field_idx++]);
            bar.high = std::stod(fields[field_idx++]);
            bar.low = std::stod(fields[field_idx++]);
            bar.close = std::stod(fields[field_idx++]);
            bar.volume = std::stoll(fields[field_idx++]);

            bars.push_back(bar);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line " << line_num << ": " << e.what()
                     << "\nLine: " << line << std::endl;
            throw;
        }
    }

    if (bars.empty()) {
        throw std::runtime_error("No data loaded from: " + path);
    }

    std::cout << "Loaded " << bars.size() << " bars from " << path << std::endl;
    return bars;
}

std::vector<Bar> DataLoader::load_binary(const std::string& path, const std::string& symbol) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open binary file: " + path);
    }

    std::vector<Bar> bars;

    // Read count
    size_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (count == 0 || count > 100000000) {  // Sanity check
        throw std::runtime_error("Invalid binary file format: " + path);
    }

    bars.reserve(count);

    // ACTUAL FORMAT from Python data_downloader.py:
    // - uint32_t: timestamp string length
    // - char[]: timestamp string (ISO format) - skip this
    // - int64_t: ts_nyt_epoch (Unix epoch seconds) - USE THIS!
    // - double: open, high, low, close
    // - uint64_t: volume
    for (size_t i = 0; i < count; ++i) {
        Bar bar;

        // Read and skip timestamp string
        uint32_t timestamp_str_len;
        file.read(reinterpret_cast<char*>(&timestamp_str_len), sizeof(timestamp_str_len));

        if (timestamp_str_len == 0 || timestamp_str_len > 100) {
            throw std::runtime_error("Invalid timestamp string length at bar " +
                                   std::to_string(i) + ": " + std::to_string(timestamp_str_len));
        }

        std::string timestamp_str(timestamp_str_len, '\0');
        file.read(&timestamp_str[0], timestamp_str_len);

        // Read the ACTUAL timestamp (ts_nyt_epoch in seconds)
        int64_t ts_nyt_epoch;
        file.read(reinterpret_cast<char*>(&ts_nyt_epoch), sizeof(ts_nyt_epoch));
        bar.timestamp = std::chrono::system_clock::from_time_t(static_cast<time_t>(ts_nyt_epoch));

        // Set symbol and generate bar_id (timestamp in milliseconds)
        bar.symbol = symbol;
        int64_t timestamp_ms = ts_nyt_epoch * 1000;  // Convert seconds to milliseconds
        bar.bar_id = generate_bar_id(timestamp_ms, symbol);

        // Read OHLCV
        file.read(reinterpret_cast<char*>(&bar.open), sizeof(bar.open));
        file.read(reinterpret_cast<char*>(&bar.high), sizeof(bar.high));
        file.read(reinterpret_cast<char*>(&bar.low), sizeof(bar.low));
        file.read(reinterpret_cast<char*>(&bar.close), sizeof(bar.close));

        // Volume is uint64_t
        uint64_t volume;
        file.read(reinterpret_cast<char*>(&volume), sizeof(volume));
        bar.volume = static_cast<int64_t>(volume);

        bars.push_back(bar);

        if (file.fail()) {
            throw std::runtime_error("Error reading binary file at bar " +
                                   std::to_string(i) + ": " + path);
        }
    }

    std::cout << "Loaded " << bars.size() << " bars from " << path << std::endl;
    return bars;
}

void DataLoader::save_binary(const std::vector<Bar>& bars, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create binary file: " + path);
    }

    // Write count
    size_t count = bars.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Write bars
    for (const auto& bar : bars) {
        // Write timestamp
        int64_t timestamp_ms = to_timestamp_ms(bar.timestamp);
        file.write(reinterpret_cast<const char*>(&timestamp_ms), sizeof(timestamp_ms));

        // Write empty symbol (for compatibility)
        size_t symbol_len = 0;
        file.write(reinterpret_cast<const char*>(&symbol_len), sizeof(symbol_len));

        // Write OHLCV
        file.write(reinterpret_cast<const char*>(&bar.open), sizeof(bar.open));
        file.write(reinterpret_cast<const char*>(&bar.high), sizeof(bar.high));
        file.write(reinterpret_cast<const char*>(&bar.low), sizeof(bar.low));
        file.write(reinterpret_cast<const char*>(&bar.close), sizeof(bar.close));
        file.write(reinterpret_cast<const char*>(&bar.volume), sizeof(bar.volume));
    }

    std::cout << "Saved " << bars.size() << " bars to " << path << std::endl;
}

std::unordered_map<Symbol, std::vector<Bar>>
DataLoader::load_multi_symbol(const std::unordered_map<Symbol, std::string>& paths) {
    std::unordered_map<Symbol, std::vector<Bar>> data;

    for (const auto& [symbol, path] : paths) {
        std::cout << "Loading " << symbol << " from " << path << "..." << std::endl;
        data[symbol] = load(path);
    }

    return data;
}

std::unordered_map<Symbol, std::vector<Bar>>
DataLoader::load_from_directory(const std::string& directory,
                               const std::vector<Symbol>& symbols,
                               const std::string& extension) {
    std::unordered_map<Symbol, std::string> paths;

    for (const auto& symbol : symbols) {
        std::string path = directory + "/" + symbol + extension;
        if (!std::filesystem::exists(path)) {
            // Try uppercase
            std::string upper_symbol = symbol;
            for (char& c : upper_symbol) c = std::toupper(c);
            path = directory + "/" + upper_symbol + extension;

            if (!std::filesystem::exists(path)) {
                // Try with _RTH_NH suffix (Regular Trading Hours, No Holidays)
                path = directory + "/" + upper_symbol + "_RTH_NH" + extension;

                if (!std::filesystem::exists(path)) {
                    throw std::runtime_error("Data file not found for symbol: " +
                                           symbol + " (tried: " + path + ")");
                }
            }
        }
        paths[symbol] = path;
    }

    return load_multi_symbol(paths);
}

std::string DataLoader::extract_symbol_from_path(const std::string& path) {
    // Extract filename from path (e.g., "/path/to/TQQQ.bin" -> "TQQQ.bin")
    std::filesystem::path p(path);
    std::string filename = p.filename().string();

    // Remove extension (e.g., "TQQQ.bin" -> "TQQQ")
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return filename.substr(0, dot_pos);
    }

    return filename;
}

bool DataLoader::ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace trading
