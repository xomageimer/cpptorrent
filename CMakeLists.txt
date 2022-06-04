cmake_minimum_required(VERSION 3.21)
project(cpptorrent)

set(CMAKE_CXX_STANDARD 17)

set(Boost_USE_STATIC_LIBS ON)
ADD_DEFINITIONS(-DBoost_USE_STATIC_LIBS=ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if (WIN32)
    set(BOOST_ROOT "C:/Users/xomaq/Desktop/boost_1_71_0/boost_1_71_0")
    set(BOOST_LIBRARYDIR "C:/Users/xomaq/Desktop/boost_1_71_0/boost_1_71_0")
    set(Boost_COMPILER mgw81)
endif ()

find_package(Boost 1.71.0 COMPONENTS system regex thread REQUIRED)

add_compile_definitions(FILE_LOGGER)

add_library(bencode SHARED src/utils/bencode_lib.cpp src/utils/bencode_lib.h)
add_library(helpers SHARED src/utils/constants.h
        src/utils/auxiliary.cpp src/utils/auxiliary.h
        src/utils/random_generator.cpp src/utils/random_generator.h
        src/utils/logger.cpp src/utils/logger.h
        src/utils/async_worker.h src/utils/async_worker.cpp
        src/utils/bitfield.h src/utils/bitfield.cpp
        src/utils/move_function.h)

add_library(network STATIC
        src/Network/Primitives/Socket.h
        src/Network/Primitives/Message.h src/Network/Primitives/Message.cpp)

add_library(bittorrent STATIC
        src/Network/BitTorrent/TrackerRequester.h src/Network/BitTorrent/TrackerRequester.cpp
        src/Network/BitTorrent/PeerClient.h src/Network/BitTorrent/PeerClient.cpp src/Network/Listener.h src/Network/Listener.cpp

        src/BitTorrent/Primitives/Piece.h src/BitTorrent/Primitives/Piece.cpp
        src/BitTorrent/Primitives/ManagerRequests.h
        src/BitTorrent/Primitives/BittorrentStrategy.h src/BitTorrent/Primitives/BittorrentStrategy.cpp

        src/BitTorrent/Tracker.cpp src/BitTorrent/Torrent.cpp src/BitTorrent/Peer.cpp
        src/BitTorrent/TorrentFilesManager.cpp)

add_subdirectory(contrib/json)

add_library(dht STATIC
        src/Network/DHT/Server.h src/Network/DHT/Server.cpp
        src/Network/DHT/NodeClient.h src/Network/DHT/NodeClient.cpp

        src/DHT/Node.h src/DHT/Node.cpp
        src/DHT/Kbucket.h src/DHT/Kbucket.cpp
        src/DHT/RouteTable.h src/DHT/RouteTable.cpp)
target_link_libraries(dht PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(helpers PUBLIC ${Boost_LIBRARIES})
target_link_libraries(bittorrent PUBLIC ${Boost_LIBRARIES} network helpers bencode)
target_link_libraries(dht PUBLIC ${Boost_LIBRARIES} network helpers bencode)

if (WIN32)
    target_link_libraries(bittorrent PUBLIC -lws2_32 -lwsock32)
    target_link_libraries(dht PUBLIC -lws2_32 -lwsock32)
endif ()

add_executable(cpptorrent src/main_bittorrent.cpp)
add_executable(cppdht src/main_dht.cpp)

include_directories(${Boost_INCLUDE_DIR} src/Network src/BitTorrent src/DHT src/utils)

target_link_libraries(cpptorrent PUBLIC bittorrent helpers)

target_link_libraries(cppdht PUBLIC dht helpers)