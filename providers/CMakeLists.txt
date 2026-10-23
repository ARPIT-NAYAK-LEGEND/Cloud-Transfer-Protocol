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

find_package(CURL REQUIRED)

# Common lib
add_library(utp_providers_common STATIC common/http_client.cpp)
target_link_libraries(utp_providers_common PUBLIC CURL::libcurl)
target_compile_features(utp_providers_common PUBLIC cxx_std_20)

# Google Drive
add_library(utp_providers_gdrive STATIC google_drive/gdrive_provider.cpp)
target_link_libraries(utp_providers_gdrive PUBLIC utp_providers_interface utp_core utp_providers_common CURL::libcurl)
target_compile_features(utp_providers_gdrive PUBLIC cxx_std_20)

# OneDrive
add_library(utp_providers_onedrive STATIC onedrive/onedrive_provider.cpp)
target_link_libraries(utp_providers_onedrive PUBLIC utp_providers_interface utp_core utp_providers_common CURL::libcurl)
target_compile_features(utp_providers_onedrive PUBLIC cxx_std_20)

# S3
add_library(utp_providers_s3 STATIC s3/s3_provider.cpp)
target_link_libraries(utp_providers_s3 PUBLIC utp_providers_interface utp_core utp_providers_common CURL::libcurl)
target_compile_features(utp_providers_s3 PUBLIC cxx_std_20)

# Azure Blob
add_library(utp_providers_azure STATIC azure/azure_provider.cpp)
target_link_libraries(utp_providers_azure PUBLIC utp_providers_interface utp_core utp_providers_common CURL::libcurl)
target_compile_features(utp_providers_azure PUBLIC cxx_std_20)

# Dropbox
add_library(utp_providers_dropbox STATIC dropbox/dropbox_provider.cpp)
target_link_libraries(utp_providers_dropbox PUBLIC utp_providers_interface utp_core utp_providers_common CURL::libcurl)
target_compile_features(utp_providers_dropbox PUBLIC cxx_std_20)

# WebDAV
add_library(utp_providers_webdav STATIC webdav/webdav_provider.cpp)
target_link_libraries(utp_providers_webdav PUBLIC utp_providers_interface utp_core utp_providers_common CURL::libcurl)
target_compile_features(utp_providers_webdav PUBLIC cxx_std_20)

# iCloud
add_library(utp_providers_icloud STATIC icloud/icloud_provider.cpp)
target_link_libraries(utp_providers_icloud PUBLIC utp_providers_interface utp_core utp_providers_common CURL::libcurl)
target_compile_features(utp_providers_icloud PUBLIC cxx_std_20)

