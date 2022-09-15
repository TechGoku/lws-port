cmake_minimum_required(VERSION 3.10)

find_package(nlohmann_json 3.7.0 REQUIRED)
set(beldex-lws-rpc-sources light_wallet.cpp)
set(beldex-lws-rpc-header light_wallet.h)
add_library(beldex-lws-rpc ${beldex-lws-rpc-header} ${beldex-lws-rpc-sources})

target_link_libraries(beldex-lws-rpc 
        PRIVATE
        beldex-lws-db
        beldex-lws-common
        beldex-lws-wire
        beldex-lws-wire-json
        beldex-lws-util
        nlohmann_json::nlohmann_json
)