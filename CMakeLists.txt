cmake_minimum_required(VERSION 4.1.2)

project(Argxx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    dotenv-cpp
    GIT_REPOSITORY https://github.com/laserpants/dotenv-cpp.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(dotenv-cpp)

find_package(cpr REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)

# Automatically detect C++ source and header files without having to re-run cmake manually
file(GLOB SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_executable(agent ${SOURCES} ${HEADERS})

target_link_libraries(agent cpr::cpr nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp dotenv)

# Copy .env to build directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.env)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.env ${CMAKE_CURRENT_BINARY_DIR}/.env COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/agent.yaml ${CMAKE_CURRENT_BINARY_DIR}/agent.yaml COPYONLY)
    add_custom_command(
        TARGET agent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/.env
            ${CMAKE_CURRENT_BINARY_DIR}/.env
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/agent.yaml
            ${CMAKE_CURRENT_BINARY_DIR}/agent.yaml
        COMMENT "Copying .env and agent.yaml to build directory"
    )
endif()
