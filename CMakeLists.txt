cmake_minimum_required(VERSION 3.10)
project(mysql_test VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准和编译选项
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加编译选项
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -O2
)

# 查找必要的包
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# MySQL Connector/C++
find_package(MySQLConnector QUIET)
if(NOT MySQLConnector_FOUND)
    find_path(MYSQLCONNECTORCPP_INCLUDE_DIR
        NAMES mysql_connection.h
        PATH_SUFFIXES cppconn
        PATHS /usr/include /usr/local/include
    )
    find_library(MYSQLCONNECTORCPP_LIBRARY
        NAMES mysqlcppconn
        PATHS /usr/lib /usr/local/lib
    )
    if(NOT MYSQLCONNECTORCPP_INCLUDE_DIR OR NOT MYSQLCONNECTORCPP_LIBRARY)
        message(FATAL_ERROR "Could not find MySQL Connector/C++")
    endif()
endif()

# Proto文件生成
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/mgrMysql.proto)
get_filename_component(PROTO_FILE_NAME ${PROTO_FILES} NAME_WE)

# 服务器可执行文件
add_executable(grpc_server 
    grpc_server.cpp
    ${PROTO_FILE_NAME}.grpc.pb.cc
    ${PROTO_FILE_NAME}.pb.cc)

# 客户端可执行文件
add_executable(grpc_client 
    grpc_client.cpp
    ${PROTO_FILE_NAME}.grpc.pb.cc
    ${PROTO_FILE_NAME}.pb.cc)

# 包含目录
target_include_directories(grpc_server PRIVATE 
    ${MYSQLCONNECTORCPP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_include_directories(grpc_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(grpc_server PRIVATE 
    ${MYSQLCONNECTORCPP_LIBRARY}
    gRPC::grpc++
    protobuf::libprotobuf
    ${Boost_LIBRARIES}
    pthread
)

target_link_libraries(grpc_client PRIVATE 
    gRPC::grpc++
    protobuf::libprotobuf
    ${Boost_LIBRARIES}
    pthread
)

# 安装目标
install(TARGETS grpc_server grpc_client
    RUNTIME DESTINATION bin
)

# 复制配置文件
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.ini
               ${CMAKE_CURRENT_BINARY_DIR}/config.ini
               COPYONLY)