cmake_minimum_required(VERSION 3.17)
project(cpptorrent)

set(CMAKE_CXX_STANDARD 17)

set(Boost_USE_STATIC_LIBS ON)
ADD_DEFINITIONS(-DBoost_USE_STATIC_LIBS=ON)

if (WIN32)
    set(BOOST_ROOT "C:/Users/xomaq/Desktop/boost_1_71_0/boost_1_71_0")
    set(BOOST_LIBRARYDIR "C:/Users/xomaq/Desktop/boost_1_71_0/boost_1_71_0")
    set(Boost_COMPILER mgw81)
endif ()

find_package(Boost 1.71.0 COMPONENTS system regex thread REQUIRED)

#add_compile_definitions(FILE_LOGGER)

add_library(bencode SHARED src/utils/bencode_lib.cpp src/utils/bencode_lib.h)
add_library(helpers SHARED src/utils/constants.h
        src/utils/auxiliary.cpp src/utils/auxiliary.h
        src/utils/random_generator.cpp src/utils/random_generator.h
        src/utils/logger.cpp src/utils/logger.h
        src/utils/AsyncWorker.h src/utils/AsyncWorker.cpp
        src/utils/move_function.h)

add_library(bittorrent STATIC
        src/Network/Primitives/Socket.h
        src/Network/Primitives/Message.h src/Network/Primitives/Message.cpp

        src/Network/TrackerRequester.h src/Network/TrackerRequester.cpp
        src/Network/PeerClient.h src/Network/PeerClient.cpp src/Network/Listener.h src/Network/Listener.cpp

        src/BitTorrent/Primitives/Bitfield.h src/BitTorrent/Primitives/Bitfield.cpp
        src/BitTorrent/Primitives/Piece.h src/BitTorrent/Primitives/Piece.cpp
        src/BitTorrent/Primitives/ManagerRequests.h
        src/BitTorrent/Primitives/BittorrentStrategy.h src/BitTorrent/Primitives/BittorrentStrategy.cpp

        src/BitTorrent/Tracker.cpp src/BitTorrent/Torrent.cpp src/BitTorrent/Peer.cpp
        src/BitTorrent/TorrentFilesManager.cpp)

target_link_libraries(helpers PUBLIC ${Boost_LIBRARIES})
target_link_libraries(bittorrent PUBLIC ${Boost_LIBRARIES} helpers bencode)

if (WIN32)
    target_link_libraries(bittorrent PUBLIC -lws2_32 -lwsock32)
endif ()

add_executable(cpptorrent src/main.cpp)

include_directories(${Boost_INCLUDE_DIR} src/Network contrib/boostorg/include/boost src/BitTorrent src/utils dependence/BigInteger-master/src/header)
target_link_libraries(cpptorrent PUBLIC bittorrent bencode helpers)