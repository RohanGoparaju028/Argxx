cmake_minimum_required(VERSION 4.1.2)

project(Argxx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(cpr REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(agent main.cpp GithubAccount.cpp)

target_link_libraries(agent cpr::cpr nlohmann_json::nlohmann_json)

# Copy .env to build directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.env)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.env ${CMAKE_CURRENT_BINARY_DIR}/.env COPYONLY)
    add_custom_command(
        TARGET agent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/.env
            ${CMAKE_CURRENT_BINARY_DIR}/.env
        COMMENT "Copying .env to build directory"
    )
endif()
