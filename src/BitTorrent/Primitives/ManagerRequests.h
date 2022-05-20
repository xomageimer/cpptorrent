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
    };

    struct ReadRequest : public Request {
        uint32_t begin{};
        uint32_t length{};
    };

    struct WriteRequest : public Request {
        Piece piece;
    };

    inline bool operator<(const ReadRequest &r1, const ReadRequest &r2) {
        return std::tie(r1.remote_peer, r1.piece_index, r1.begin, r1.length) <
               std::tie(r2.remote_peer, r2.piece_index, r2.begin, r2.length);
    }
} // namespace bittorrent

#endif // CPPTORRENT_MANAGERREQUESTS_H
