#ifndef CPPTORRENT_CONSTANTS_H
#define CPPTORRENT_CONSTANTS_H

#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdint>

namespace bittorrent_constants {
    const size_t MTU = 1500;
    const size_t handshake_length = 68;
    const size_t short_buff_size = 32;
    const size_t middle_buff_size = 98;
    const size_t long_buff_size = 512;

    const size_t begin_port = 6880;
    const size_t last_port = 6889;
    const size_t tracker_again_request_time_secs = 900;

    const size_t byte_size = 8;

    const boost::posix_time::milliseconds epsilon{boost::posix_time::milliseconds(15)};
    const boost::posix_time::milliseconds connection_waiting_time{boost::posix_time::milliseconds(2000)};
    const boost::posix_time::milliseconds announce_waiting_time{boost::posix_time::milliseconds(1000)};
    const int MAX_CONNECT_ATTEMPTS = 4;
    const int MAX_ANNOUNCE_ATTEMPTS = 3;
}

#endif //CPPTORRENT_CONSTANTS_H
