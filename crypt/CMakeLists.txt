cmake_minimum_required(VERSION 3.19)
project(test2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)
find_package(protobuf CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

qt_standard_project_setup()

qt_add_executable(test2
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    Response.pb.h
    Response.pb.cc
    Request.pb.h
    Request.pb.cc
    myrsa.h myrsa.cpp
    myaes.h myaes.cpp
    openssl_applink.cpp
    secmng.h secmng.cpp
    index.h index.cpp index.ui
    Login.pb.h Login.pb.cc
    File.pb.cc File.pb.h
    codec.h codec.cpp
)

target_link_libraries(test2 PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    protobuf::libprotobuf
    OpenSSL::Crypto
    OpenSSL::SSL
)

install(TARGETS test2
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET test2
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})