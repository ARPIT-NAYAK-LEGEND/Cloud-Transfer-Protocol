find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

add_executable(utpd main.cpp)
target_link_libraries(utpd PRIVATE 
    utp_core 
    utp_providers_local 
    utp_providers_http
    utp_providers_gdrive 
    utp_providers_onedrive 
    utp_providers_s3 
    utp_providers_azure
    utp_providers_dropbox
    utp_providers_webdav
    utp_providers_icloud
    OpenSSL::SSL
    OpenSSL::Crypto
)
if(WIN32)
    target_link_libraries(utpd PRIVATE ws2_32)
endif()
