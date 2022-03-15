#ifndef CPPTORRENT_MESSAGE_H
#define CPPTORRENT_MESSAGE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "Constants.h"

namespace bittorrent {
    enum class MESSAGE_TYPE : uint8_t {
        choke = 0,
        unchoke = 1,
        interested = 2,
        not_interested = 3,
        have = 4,
        bitfield = 5,
        request = 6,
        piece_block = 7,
        cancel = 8,
        port = 9
    };
    struct Message {
    public:
        static const inline int max_body_length = bittorrent_constants::MTU;
        static const inline int header_length = 4;
        static const inline int id_length = 1;

    private:
        uint8_t data_[header_length + id_length + max_body_length];
        MESSAGE_TYPE m_type_;
    };
}// namespace bittorrent


#endif//CPPTORRENT_MESSAGE_H
