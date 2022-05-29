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

    const boost::posix_time::milliseconds epsilon{boost::posix_time::milliseconds(15)};
    const boost::posix_time::milliseconds connection_waiting_time{boost::posix_time::milliseconds(2000)};
    const boost::posix_time::milliseconds announce_waiting_time{boost::posix_time::milliseconds(1000)};
    const boost::posix_time::seconds tracker_again_request_time_secs{900};
    const boost::posix_time::seconds piece_waiting_time{60};
    const boost::posix_time::minutes waiting_time{boost::posix_time::minutes(2)};
    const boost::posix_time::minutes keep_alive_time {boost::posix_time::minutes(1)};
    const std::chrono::minutes sleep_time {std::chrono::minutes(3)};

    const int MAX_CONNECT_ATTEMPTS = 4;
    const int MAX_ANNOUNCE_ATTEMPTS = 3;
} // namespace bittorrent_constants

#endif // CPPTORRENT_CONSTANTS_H
