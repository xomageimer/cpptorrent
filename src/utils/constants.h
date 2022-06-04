#ifndef CPPTORRENT_CONSTANTS_H
#define CPPTORRENT_CONSTANTS_H

#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdint>
#include <chrono>

namespace bittorrent_constants {
    const size_t MTU = 1500;
    const size_t handshake_length = 68;
    const size_t short_buff_size = 32;
    const size_t middle_buff_size = 98;
    const size_t long_buff_size = 512;

    const size_t begin_port = 6880;
    const size_t last_port = 6889;

    const size_t byte_size = 8;
    [[maybe_unused]] const size_t most_request_size = std::pow(2, 14);
    const int MAX_AVAILABLE_UNCHOKE_ONE_TIME = 35;
    const int REQUEST_MAX_QUEUE_SIZE = 150;

    const size_t END_GAME_STARTED_FROM = 92;
    static_assert(END_GAME_STARTED_FROM >= 0 && END_GAME_STARTED_FROM < 100);

    const boost::posix_time::milliseconds epsilon{boost::posix_time::milliseconds(15)};
    const boost::posix_time::milliseconds connection_waiting_time{boost::posix_time::milliseconds(2000)};
    const boost::posix_time::milliseconds announce_waiting_time{boost::posix_time::milliseconds(1000)};
    const boost::posix_time::seconds tracker_again_request_time_secs{900};
    const boost::posix_time::seconds piece_waiting_time{15};
    const boost::posix_time::minutes waiting_time{boost::posix_time::minutes(2)};
    const boost::posix_time::minutes keep_alive_time{boost::posix_time::minutes(1)};
    const std::chrono::seconds sleep_time{std::chrono::seconds(15)};

    const int MAX_CONNECT_ATTEMPTS = 4;
    const int MAX_ANNOUNCE_ATTEMPTS = 3;
} // namespace bittorrent_constants

namespace dht_constants {
    const size_t KBuckets_deep = 8;
    const size_t max_nearest_nodes = 35;
    const size_t key_size = 100;
    const size_t SHA1_SIZE_BITS = 20 * bittorrent_constants::byte_size;
    const boost::posix_time::seconds ping_wait_time{20};
    const boost::posix_time::seconds bucket_refresh_interval{900};
} // namespace dht_constants

#endif // CPPTORRENT_CONSTANTS_H
