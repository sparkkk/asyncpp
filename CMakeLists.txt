cmake_minimum_required (VERSION 3.13)

project (asyncpp-test)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

file(
    GLOB source_files
    "${CMAKE_SOURCE_DIR}/test/*.cpp"
)

add_executable(asyncpp-test ${source_files})

target_include_directories(
    asyncpp-test PRIVATE	
    ${CMAKE_SOURCE_DIR}
)

target_compile_options(
    asyncpp-test PRIVATE 
    -std=c++11
)

target_link_libraries(
    asyncpp-test
    stdc++
)

#########################################

