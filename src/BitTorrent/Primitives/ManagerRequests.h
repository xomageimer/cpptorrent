#ifndef CPPTORRENT_MANAGERREQUESTS_H
#define CPPTORRENT_MANAGERREQUESTS_H

#include <memory>

#include "Primitives/Piece.h"

namespace network {
    struct PeerClient;
} // namespace network

namespace bittorrent {
    struct Request {
        std::shared_ptr<network::PeerClient> remote_peer;
        uint32_t piece_index;
        uint32_t begin;
    };

    struct ReadRequest : public Request {
        uint32_t length{};
    };

    struct WriteRequest : public Request {
        Block block;
    };
} // namespace bittorrent

#endif // CPPTORRENT_MANAGERREQUESTS_H
