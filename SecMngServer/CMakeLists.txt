aux_source_directory(. SERVER_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/crypto CRYPTO_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/proto PROTO_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/shm SHM_SRC)

add_executable(SecMngServer
    ${SERVER_SRC}
    ${CRYPTO_SRC}
    ${PROTO_SRC}
    ${SHM_SRC}
)

target_include_directories(SecMngServer PRIVATE
    ${PROJECT_SOURCE_DIR}/crypto
    ${PROJECT_SOURCE_DIR}/proto
    ${PROJECT_SOURCE_DIR}/shm
)

target_link_libraries(SecMngServer PRIVATE
    muduo_net
    muduo_base
    OpenSSL::Crypto
    Threads::Threads
    JsonCpp::JsonCpp
    protobuf::libprotobuf
    spdlog::spdlog
)