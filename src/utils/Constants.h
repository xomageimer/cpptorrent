#ifndef CPPTORRENT_CONSTANTS_H
#define CPPTORRENT_CONSTANTS_H

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
}

#endif //CPPTORRENT_CONSTANTS_H
