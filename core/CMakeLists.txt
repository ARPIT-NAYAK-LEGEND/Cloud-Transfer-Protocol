add_library(utp_core STATIC src/uri_parser.cpp src/engine.cpp)

target_include_directories(utp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(utp_core PUBLIC cxx_std_20)
