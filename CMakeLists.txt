cmake_minimum_required(VERSION 3.20)
project(utp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add core library
add_subdirectory(core)

# Add providers
add_subdirectory(providers)

# Add cli
add_subdirectory(cli)

# Testing setup
enable_testing()
add_subdirectory(tests/unit)
