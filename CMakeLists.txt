cmake_minimum_required(VERSION 3.16)
project(accel_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SERVER_PORT "50051")
set(SERVER_BINARY_PATH "${CMAKE_BINARY_DIR}/server/server")
set(NODE_A_BINARY_PATH "${CMAKE_BINARY_DIR}/node_a/node_a")
set(NODE_B_BINARY_PATH "${CMAKE_BINARY_DIR}/node_b/node_b")
set(PROJECT_ROOT_PATH "${CMAKE_SOURCE_DIR}")
set(SERVICE_USER "$ENV{USER}")
set(SERVICE_GROUP "$ENV{USER}")
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILES "${PROTO_DIR}/accel.proto")
set(DEPLOY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deploy")
set(GEN_CERTS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/gen_certs.sh")
set(CERTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/certs")
set(GENERAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/general")
set(GENERAL_CPP "${GENERAL_DIR}/general.cpp")

configure_file(
    "${DEPLOY_DIR}/server.service.in"
    "${CMAKE_CURRENT_BINARY_DIR}/server/accel_server.service"
    @ONLY
)
configure_file(
    "${DEPLOY_DIR}/node_a.service.in"
    "${CMAKE_CURRENT_BINARY_DIR}/node_a/accel_node_a.service"
    @ONLY
)
configure_file(
    "${DEPLOY_DIR}/node_b.service.in"
    "${CMAKE_CURRENT_BINARY_DIR}/node_b/accel_node_b.service"
    @ONLY
)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(absl REQUIRED)
find_package(spdlog REQUIRED)

add_library(accel_proto STATIC)

get_target_property(GRPC_PLUGIN gRPC::grpc_cpp_plugin LOCATION)

protobuf_generate(
    TARGET accel_proto
    LANGUAGE cpp
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS ${PROTO_DIR}
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/accel.grpc.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/accel.grpc.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        --plugin=protoc-gen-grpc=${GRPC_PLUGIN}
        -I ${PROTO_DIR}
        ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    COMMENT "Generating proto"
)
target_sources(accel_proto PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/accel.grpc.pb.cc)

add_custom_command(
    OUTPUT
        "${CERTS_DIR}/ca.crt"
        "${CERTS_DIR}/server.crt"
        "${CERTS_DIR}/server.key"
        "${CERTS_DIR}/client.crt"
        "${CERTS_DIR}/client.key"
    COMMAND bash ${GEN_CERTS_SCRIPT} > /dev/null
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${GEN_CERTS_SCRIPT}
    COMMENT "Generating certificates"
)
add_custom_target(generate_certs DEPENDS "${CERTS_DIR}/ca.crt")

target_include_directories(accel_proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROTO_DIR}
)

target_link_libraries(accel_proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)

add_subdirectory(server)
add_subdirectory(node_a)
add_subdirectory(node_b)
