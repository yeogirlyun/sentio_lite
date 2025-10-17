#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace utils {

/**
 * Simple configuration file reader
 *
 * Reads symbol list from config/symbols.conf
 * Format: One symbol per line, lines starting with # are comments
 */
class ConfigReader {
public:
    /**
     * Load symbols from configuration file
     * @param config_path Path to symbols.conf (default: config/symbols.conf)
     * @return Vector of symbol strings
     * @throws std::runtime_error if file not found or empty
     */
    static std::vector<std::string> load_symbols(const std::string& config_path = "config/symbols.conf") {
        std::vector<std::string> symbols;

        std::ifstream file(config_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open config file: " + config_path);
        }

        std::string line;
        int line_number = 0;

        while (std::getline(file, line)) {
            line_number++;

            // Trim whitespace
            line = trim(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Validate symbol (alphanumeric and dots only)
            if (!is_valid_symbol(line)) {
                throw std::runtime_error(
                    "Invalid symbol '" + line + "' at line " + std::to_string(line_number) +
                    " in " + config_path
                );
            }

            symbols.push_back(line);
        }

        file.close();

        if (symbols.empty()) {
            throw std::runtime_error("No symbols found in config file: " + config_path);
        }

        return symbols;
    }

private:
    /**
     * Trim whitespace from both ends of string
     */
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }

    /**
     * Check if symbol contains only valid characters
     * Valid: A-Z, a-z, 0-9, dot (.)
     */
    static bool is_valid_symbol(const std::string& symbol) {
        if (symbol.empty()) {
            return false;
        }

        for (char c : symbol) {
            if (!std::isalnum(c) && c != '.') {
                return false;
            }
        }

        return true;
    }
};

} // namespace utils
