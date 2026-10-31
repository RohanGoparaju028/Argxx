cmake_minimum_required(VERSION 4.1.2)

project(Argxx)

set(CMAKE_CXX_STANDARD ON)
set(CMAKE_CXX_STANDARD 11)

find_package(cpr REQUIRED)

add_executable(agent main.cpp)

target_link_libraries(agent cpr::cpr)
