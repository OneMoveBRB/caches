cmake_minimum_required(VERSION 3.20)
project(caches LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CACHES_BUILD_TESTS        "Build unit tests"                  ON )
option(CACHES_WARNINGS           "Enable compiler warnings"          ON )
option(CACHES_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(cmake/warnings.cmake)

# ---------------------------------------------------------------------------
# Library (header-only)
# ---------------------------------------------------------------------------
add_library(caches INTERFACE)
target_include_directories(caches INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(CACHES_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
