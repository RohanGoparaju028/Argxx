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
# Prepare Folder.cpp's directories and copy existing contents on every build.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/copy_agent_folders.cmake" CONTENT "
file(MAKE_DIRECTORY
    \"${CMAKE_CURRENT_BINARY_DIR}/.Argxx/.graph\"
    \"${CMAKE_CURRENT_BINARY_DIR}/.Argxx/.issue\"
)
if(IS_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/.Argxx\" AND
   NOT \"${CMAKE_CURRENT_SOURCE_DIR}\" STREQUAL \"${CMAKE_CURRENT_BINARY_DIR}\")
    file(COPY \"${CMAKE_CURRENT_SOURCE_DIR}/.Argxx\"
         DESTINATION \"${CMAKE_CURRENT_BINARY_DIR}\")
endif()
")
add_custom_target(copy_agent_folders ALL
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/copy_agent_folders.cmake"
    COMMENT "Preparing and copying .Argxx folders to the build directory"
    VERBATIM
)

add_executable(agent ${SOURCES} ${HEADERS})
add_dependencies(agent copy_agent_folders)

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

option(ARGXX_BUILD_TESTS "Build offline agent interaction checks" OFF)
if(ARGXX_BUILD_TESTS)
    enable_testing()
    add_executable(agent_interaction_tests tests/AgentInteractionTests.cpp RepositoryContext.cpp GithubAccount.cpp)
    target_link_libraries(agent_interaction_tests PRIVATE cpr::cpr nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp dotenv)
    add_test(NAME agent_interaction_tests COMMAND agent_interaction_tests)
    set_tests_properties(agent_interaction_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
