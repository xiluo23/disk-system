cmake_minimum_required(VERSION 3.16)

project(Crypt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(jsoncpp REQUIRED)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(
    ${Protobuf_INCLUDE_DIRS}
    proto
)

aux_source_directory(. SRC)
aux_source_directory(proto PROTO_SRC)

add_executable(server
    ${SRC}
    ${PROTO_SRC}
)

target_link_libraries(server
    PRIVATE
    muduo_net
    muduo_base
    OpenSSL::Crypto
    Threads::Threads
    JsonCpp::JsonCpp
    protobuf::libprotobuf
)