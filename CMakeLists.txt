cmake_minimum_required(VERSION 3.8.2)
project(reverse-tunnel)

set(CMAKE_CXX_STANDARD 17)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

SET(CONAN_LIBS_BOOST boost_program_options boost_system)
conan_basic_setup()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")

add_executable(reverse-tunnel main.cpp)
target_link_libraries(reverse-tunnel ${CONAN_LIBS})
