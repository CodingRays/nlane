cmake_minimum_required(VERSION 3.18)

project(nlane
    VERSION 0.0.1
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

file(GLOB_RECURSE NLANE_SRC RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")

add_library(nlane_lib STATIC ${NLANE_SRC})
target_include_directories(nlane_lib PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_executable(nlane_test "${CMAKE_SOURCE_DIR}/test/main.cpp")
target_link_libraries(nlane_test PRIVATE nlane_lib)