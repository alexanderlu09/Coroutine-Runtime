cmake_minimum_required(VERSION 3.24)
project(coro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(CORO_BUILD_TESTS    "Build the test suite" ON)
option(CORO_BUILD_EXAMPLES "Build the examples"   ON)

# ---------------------------------------------------------------------------
# The runtime library.
# Add new .cpp files here as you write them.
# ---------------------------------------------------------------------------
add_library(coro
  src/clock.cpp
  src/scheduler.cpp
)
target_include_directories(coro PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(coro PRIVATE
  -Wall -Wextra -Wpedantic
  -Wshadow                 # a shadowed variable in a scheduler is a very bad day
  -Wconversion
)

find_package(Threads REQUIRED)
target_link_libraries(coro PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# Sanitizers. Driven by the presets in CMakePresets.json; ASAN and TSAN are
# mutually exclusive, which is why they are separate build directories rather
# than one build with flags toggled.
# ---------------------------------------------------------------------------
set(CORO_SANITIZER "" CACHE STRING "One of: address, thread, undefined, or empty")

if(CORO_SANITIZER)
  message(STATUS "Building with -fsanitize=${CORO_SANITIZER}")
  add_compile_options(-fsanitize=${CORO_SANITIZER} -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=${CORO_SANITIZER})
endif()

if(CORO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(CORO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
