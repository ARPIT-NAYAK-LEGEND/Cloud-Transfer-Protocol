cmake_minimum_required(VERSION 3.20)
project(utp VERSION 2.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
find_package(CURL REQUIRED)

# Add core library
add_subdirectory(core)

# Add providers
add_subdirectory(providers)

# Add cli
add_subdirectory(cli)

# Add API Server
add_subdirectory(server)

# Testing setup
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unit)
endif()
