add_executable(utp main.cpp tui.cpp)
target_link_libraries(utp PRIVATE 
    utp_core 
    utp_providers_local 
    utp_providers_gdrive 
    utp_providers_onedrive 
    utp_providers_s3 
    utp_providers_azure
    utp_providers_dropbox
    utp_providers_webdav
    utp_providers_icloud
)
if(WIN32)
    target_link_libraries(utp PRIVATE ws2_32)
endif()
