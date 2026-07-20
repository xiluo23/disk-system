cmake_minimum_required(VERSION 3.16)

project(Crypt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(spdlog REQUIRED)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
include_directories(/usr/include/mysql)

add_subdirectory(SecMngServer)
add_subdirectory(LoginServer)
add_subdirectory(FileServer)