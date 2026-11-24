add_executable(utp main.cpp tui.cpp stream_server.cpp)
target_include_directories(utp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
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
    utp_providers_http
)
if(WIN32)
    target_link_libraries(utp PRIVATE ws2_32)
endif()
