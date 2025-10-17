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
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
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
# Source Files
# ============================================================================
set(CORE_SOURCES
    # Predictor
    src/predictor/online_predictor.cpp
    src/predictor/feature_extractor.cpp

    # Trading
    src/trading/multi_symbol_trader.cpp

    # Utils
    src/utils/data_loader.cpp
)

# ============================================================================
# Libraries
# ============================================================================

# Core library (static)
add_library(sentio_core STATIC ${CORE_SOURCES})
target_link_libraries(sentio_core PUBLIC
    Eigen3::Eigen
    Threads::Threads
)

# ============================================================================
# Executables
# ============================================================================

# Main trading system
add_executable(sentio_lite src/main.cpp)
target_link_libraries(sentio_lite PRIVATE
    sentio_core
    Threads::Threads
)

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

# ============================================================================
# Configuration Summary
# ============================================================================
message(STATUS "========================================")
message(STATUS "Sentio Lite Configuration")
message(STATUS "========================================")
message(STATUS "Project:        ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:   ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Eigen3 version: ${EIGEN3_VERSION}")

# Print source file count
list(LENGTH CORE_SOURCES NUM_SOURCES)
message(STATUS "Source files:   ${NUM_SOURCES}")

message(STATUS "========================================")
message(STATUS "Features:")
message(STATUS "  - 25 Technical Indicators")
message(STATUS "  - EWRLS Online Learning")
message(STATUS "  - Multi-Symbol Trading")
message(STATUS "  - Binary & CSV Data Support")
message(STATUS "  - Adaptive Position Sizing")
message(STATUS "  - Stop-Loss & Profit Targets")
message(STATUS "========================================")
