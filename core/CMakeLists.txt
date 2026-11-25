find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(utp_core STATIC 
    src/uri_parser.cpp 
    src/engine.cpp 
    src/chunk_planner.cpp
    src/state_manager.cpp
    src/sync_engine.cpp
    src/multi_engine.cpp
    src/routing_rules.cpp
)

target_include_directories(utp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(utp_core PUBLIC cxx_std_20)
target_link_libraries(utp_core PUBLIC SQLite::SQLite3 OpenSSL::Crypto Threads::Threads)
if(WIN32)
    target_link_libraries(utp_core PUBLIC ws2_32 advapi32 shell32)
endif()
