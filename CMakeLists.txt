cmake_minimum_required(VERSION 3.11)
project(Peer VERSION 0.0.1 LANGUAGES CXX)

#
# For getting Catch2 sources
#
Include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#
# peer binary
#
add_executable(peer
    src/App.cpp
    src/ConfigParser.cpp
    src/main.cpp
    src/MiddleWare.cpp
    src/UdpSocket.cpp
    )    

#
# Tests
#
add_executable(MiddleWareTest 
    test/MiddleWareTest.cpp
    src/MiddleWare.cpp
    )

target_include_directories(MiddleWareTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(MiddleWareTest PRIVATE Catch2::Catch2WithMain)
add_test(NAME MiddleWareTest COMMAND MiddleWareTest)
