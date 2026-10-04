add_library(utp_providers_interface INTERFACE)

target_include_directories(utp_providers_interface
    INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:.>
)

target_compile_features(utp_providers_interface INTERFACE cxx_std_20)

add_library(utp_providers_local STATIC local/local_provider.cpp)
target_link_libraries(utp_providers_local PUBLIC utp_providers_interface utp_core)
target_compile_features(utp_providers_local PUBLIC cxx_std_20)
