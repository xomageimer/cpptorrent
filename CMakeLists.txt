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

add_library(bencode SHARED utils/bencode_lib.cpp utils/bencode_lib.h)
add_library(helpers SHARED utils/auxiliary.cpp utils/auxiliary.h)
add_library(random SHARED utils/random_generator.cpp utils/random_generator.h)
add_library(network SHARED Network/TrackerRequester.cpp Network/TrackerRequester.h BitTorrent/Tracker.cpp BitTorrent/Torrent.cpp  BitTorrent/Peer.cpp)

target_link_libraries(helpers PUBLIC ${Boost_LIBRARIES})
target_link_libraries(network PUBLIC ${Boost_LIBRARIES} helpers bencode random)

if(WIN32)
    target_link_libraries(network PUBLIC -lws2_32 -lwsock32)
endif()

add_executable(cpptorrent src/main.cpp)

include_directories(${Boost_INCLUDE_DIR} Network contrib/boostorg/include/boost BitTorrent utils)
target_link_libraries(cpptorrent PUBLIC network bencode helpers random)