cmake_minimum_required(VERSION 3.17)
project(cpptorrent)

set(CMAKE_CXX_STANDARD 17)

set(Boost_USE_STATIC_LIBS ON)
ADD_DEFINITIONS(-DBoost_USE_STATIC_LIBS=ON)

if (WIN32)
    set (BOOST_ROOT "C:/Users/xomaq/Desktop/boost_1_71_0/boost_1_71_0")
    set (BOOST_LIBRARYDIR "C:/Users/xomaq/Desktop/boost_1_71_0/boost_1_71_0")
    set (Boost_COMPILER mgw81)
endif()

find_package(Boost 1.71.0 COMPONENTS system regex thread REQUIRED)

add_library(bencode SHARED src/utils/bencode_lib.cpp src/utils/bencode_lib.h)
add_library(helpers SHARED src/utils/auxiliary.cpp src/utils/auxiliary.h)
add_library(random SHARED src/utils/random_generator.cpp src/utils/random_generator.h)
add_library(network SHARED src/Network/TrackerRequester.cpp src/Network/TrackerRequester.h src/BitTorrent/Tracker.cpp src/BitTorrent/Torrent.cpp src/BitTorrent/Peer.cpp)

target_link_libraries(helpers PUBLIC ${Boost_LIBRARIES})
target_link_libraries(network PUBLIC ${Boost_LIBRARIES} helpers bencode random)

if(WIN32)
    target_link_libraries(network PUBLIC -lws2_32 -lwsock32)
endif()

add_executable(cpptorrent src/main.cpp)

include_directories(${Boost_INCLUDE_DIR} src/Network contrib/boostorg/include/boost src/BitTorrent src/utils)
target_link_libraries(cpptorrent PUBLIC network bencode helpers random)