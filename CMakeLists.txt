cmake_minimum_required(VERSION 3.11)
project(Peer VERSION 0.0.1 LANGUAGES CXX)

Include(FetchContent)
Include(CTest)

#
# For getting Catch2 sources
#
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1 # or a later release
)

#
# Fmt library
#
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt
  GIT_TAG        11.1.1)

FetchContent_MakeAvailable(Catch2 fmt)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


#
# Peer binary
#
add_executable(Peer
    src/App.cpp
    src/ConfigParser.cpp
    src/main.cpp
    src/MiddleWare.cpp
    src/UdpSocket.cpp
    )

# Increase Warnings, treat warnings as errors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

# Enable AddressSanitizer for Debug builds on clang or gcc only
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
        set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address,undefined")
    endif()
endif()

target_link_libraries(Peer PRIVATE fmt::fmt)

#
# Tests
#
add_executable(PeerTest 
    test/MiddleWareTest.cpp
    src/MiddleWare.cpp
    )

# for coverage: ensure tests are executed in debug mode
#set(CMAKE_BUILD_TYPE "Debug")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(PeerTest PRIVATE --coverage)
    target_link_options(PeerTest PRIVATE --coverage)
endif()

target_include_directories(PeerTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(PeerTest PRIVATE Catch2::Catch2WithMain fmt::fmt)
add_test(NAME PeerTest COMMAND PeerTest)

# Custom target to run tests and generate coverage report 
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
add_custom_target(coverage 
    COMMAND ${CMAKE_CTEST_COMMAND} -T test --output-on-failure 
    COMMAND gcovr -r ${CMAKE_SOURCE_DIR} --xml -o coverage.xml 
    COMMAND gcovr -r ${CMAKE_SOURCE_DIR} --html --html-details -o coverage.html
    DEPENDS PeerTest )
endif()