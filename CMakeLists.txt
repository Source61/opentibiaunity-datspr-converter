cmake_minimum_required(VERSION 3.16)
project(opentibiaunity_datspr_converter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Newer protobuf releases ship a CMake package config (and need it for their abseil
# dependency); older distro builds such as Debian's 3.21 only work through FindProtobuf.
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()
find_package(LibLZMA REQUIRED)
find_package(PNG REQUIRED)
find_package(nlohmann_json 3 REQUIRED)
find_package(Threads REQUIRED)

# proto/*.proto is the OpenTibiaUnity output schema, proto/tibia/*.proto the Tibia 11+
# input schema. Both compile against proto/ as the import root so their file names in
# the protobuf descriptor pool stay distinct ("shared.proto" vs "tibia/shared.proto").
set(PROTO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(PROTO_FILES appearances.proto shared.proto tibia/appearances.proto tibia/shared.proto)
set(PROTO_SRCS)
foreach(proto ${PROTO_FILES})
    string(REGEX REPLACE "\\.proto$" "" stem ${proto})
    list(APPEND PROTO_SRCS ${PROTO_GEN}/${stem}.pb.cc)
    set(proto_outputs ${PROTO_GEN}/${stem}.pb.cc ${PROTO_GEN}/${stem}.pb.h)
    add_custom_command(
        OUTPUT ${proto_outputs}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_GEN}
        COMMAND protobuf::protoc -I ${PROTO_ROOT} --cpp_out=${PROTO_GEN} ${PROTO_ROOT}/${proto}
        DEPENDS ${PROTO_ROOT}/${proto} protobuf::protoc
        COMMENT "Generating C++ from ${proto}"
        VERBATIM)
endforeach()

add_executable(opentibiaunity_datspr_converter
    src/main.cpp
    src/thing_type.cpp
    src/content_data.cpp
    src/content_sprites.cpp
    src/legacy_converter.cpp
    src/protobuf_converter.cpp
    src/image.cpp
    src/cip_lzma.cpp
    src/output.cpp
    ${PROTO_SRCS})

target_include_directories(opentibiaunity_datspr_converter PRIVATE src ${PROTO_GEN})
target_link_libraries(opentibiaunity_datspr_converter PRIVATE
    protobuf::libprotobuf LibLZMA::LibLZMA PNG::PNG nlohmann_json::nlohmann_json Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${PROTO_SRCS} PROPERTIES COMPILE_OPTIONS "-w")
    target_compile_options(opentibiaunity_datspr_converter PRIVATE -Wall -Wextra)
endif()
