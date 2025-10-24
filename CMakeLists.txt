cmake_minimum_required(VERSION 3.16)
project(SentioLite VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# C++ Standard & Build Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# Compiler Flags
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

    # Optional: AddressSanitizer for debug builds
    option(ENABLE_ASAN "Enable AddressSanitizer in debug builds" OFF)
    if(ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
    endif()
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

# ============================================================================
# Dependencies
# ============================================================================
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(Threads REQUIRED)

# ============================================================================
# Include Directories
# ============================================================================
include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# Source Files - CORRECTED AND VALIDATED
# ============================================================================
set(CORE_SOURCES
    # Predictor components (robust online learning)
    src/predictor/ewrls_predictor.cpp          # Core EWRLS with numerical stability
    src/predictor/online_predictor.cpp         # Wrapper for 54-feature enforcement
    src/predictor/feature_extractor.cpp        # 54-feature extractor (8 time + 34 technical + 12 regime)
    src/predictor/regime_features.cpp          # Fast regime detection (k-means clustering)
    src/predictor/multi_horizon_predictor.cpp  # Multi-horizon prediction (1, 5, 10 bars)

    # Features (backward compatibility wrapper)
    src/features/unified_features.cpp          # Wrapper around FeatureExtractor

    # Trading engine
    src/trading/multi_symbol_trader.cpp        # Multi-symbol rotation trading
    src/trading/alpaca_cost_model.cpp          # Alpaca transaction cost model
    src/trading/trade_filter.cpp               # Trade frequency and holding period management

    # Utils
    src/utils/data_loader.cpp                  # Binary/CSV data loading
)

# Validate that all source files exist
foreach(SOURCE ${CORE_SOURCES})
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${SOURCE}")
        message(FATAL_ERROR "Missing required source file: ${SOURCE}")
    endif()
endforeach()

# ============================================================================
# Libraries
# ============================================================================

# Core library (static)
add_library(sentio_core STATIC ${CORE_SOURCES})
target_link_libraries(sentio_core PUBLIC
    Eigen3::Eigen
    Threads::Threads
)

# Set include directories for the library
target_include_directories(sentio_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ============================================================================
# Executables
# ============================================================================

# Main trading system
add_executable(sentio_lite src/main.cpp)
target_link_libraries(sentio_lite PRIVATE
    sentio_core
    Eigen3::Eigen
    Threads::Threads
)

# Alpaca cost model demonstration (optional)
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    if(EXISTS "${CMAKE_SOURCE_DIR}/examples/alpaca_cost_demo.cpp")
        add_executable(alpaca_cost_demo examples/alpaca_cost_demo.cpp)
        target_link_libraries(alpaca_cost_demo PRIVATE
            sentio_core
            Threads::Threads
        )
        message(STATUS "Building example: alpaca_cost_demo")
    else()
        message(STATUS "Example alpaca_cost_demo.cpp not found, skipping")
    endif()
endif()

# ============================================================================
# Testing (Optional but recommended)
# ============================================================================
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(WARNING "BUILD_TESTS enabled but tests/ directory not found")
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS sentio_lite
    RUNTIME DESTINATION bin
)

install(TARGETS sentio_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# ============================================================================
# Configuration Summary
# ============================================================================
message(STATUS "========================================")
message(STATUS "Sentio Lite Configuration")
message(STATUS "========================================")
message(STATUS "Project:         ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Eigen3 version:  ${EIGEN3_VERSION}")

# Print source file count
list(LENGTH CORE_SOURCES NUM_SOURCES)
message(STATUS "Source files:    ${NUM_SOURCES}")

message(STATUS "========================================")
message(STATUS "Features:")
message(STATUS "  - 33 Features (8 time + 25 technical)")
message(STATUS "  - Multi-Horizon Prediction (1, 5, 10 bars)")
message(STATUS "  - EWRLS with numerical stability")
message(STATUS "  - Condition number monitoring")
message(STATUS "  - Adaptive regularization")
message(STATUS "  - Gradient clipping")
message(STATUS "  - Multi-Symbol Rotation Trading")
message(STATUS "  - Trade frequency management")
message(STATUS "  - Holding period constraints (min 5 bars)")
message(STATUS "  - Dynamic exit logic")
message(STATUS "  - Alpaca Cost Model")
message(STATUS "  - Binary & CSV Data Support")
message(STATUS "  - Adaptive Position Sizing")
message(STATUS "========================================")
message(STATUS "Build options:")
message(STATUS "  BUILD_EXAMPLES:  ${BUILD_EXAMPLES}")
message(STATUS "  BUILD_TESTS:     ${BUILD_TESTS}")
if(ENABLE_ASAN)
    message(STATUS "  ASAN enabled:    YES (Debug only)")
endif()
message(STATUS "========================================")
