cmake_minimum_required(VERSION 3.17)
project(coro)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-fcoroutines)
add_definitions(-fno-exceptions)
add_definitions(-fexceptions)

find_package(Threads REQUIRED)

add_executable(coro main.cpp)

target_link_libraries(${PROJECT_NAME} Threads::Threads)
