cmake_minimum_required(VERSION 3.0.0)
project(TestProj VERSION 0.1.0)

include(CTest)
enable_testing()

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -march=native -O3 -pthread" )

add_executable(ReactorBasedVPN main.cpp Reactor.cpp ParseHttp.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
