cmake_minimum_required(VERSION 3.18)

project(nlane
    VERSION 0.0.1
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Folder definitions
set(NLANE_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(NLANE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(NLANE_LIB_DIR "${CMAKE_SOURCE_DIR}/lib")



add_subdirectory(${NLANE_LIB_DIR})

file(GLOB_RECURSE NLANE_SRC_FILES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")

add_library(nlane_lib STATIC ${NLANE_SRC_FILES})
target_include_directories(nlane_lib PUBLIC "${CMAKE_SOURCE_DIR}/include")

add_executable(nlane_test "${CMAKE_SOURCE_DIR}/test/main.cpp")
target_link_libraries(nlane_test PRIVATE nlane_lib gtest_main)