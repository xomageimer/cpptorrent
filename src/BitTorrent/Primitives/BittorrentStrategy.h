#ifndef CPPTORRENT_BITTORRENTSTRATEGY_H
#define CPPTORRENT_BITTORRENTSTRATEGY_H

#include <chrono>
#include <optional>
#include <cctype>

#include "Piece.h"
#include "Bitfield.h"
#include "Peer.h"

namespace network {
    struct Torrent;
    struct PeerClient;
}

namespace bittorrent {
    // default strategy
    struct BittorrentStrategy {
        BittorrentStrategy();

        virtual std::optional<size_t> ChoosePiece(const Peer &peer_owner, const Peer &peer_neigh);

        virtual void OnPieceDownloaded(size_t total_piece_count, size_t pieces_already_downloaded, [[maybe_unused]] Torrent &torrent);

        virtual void OnChoked(std::shared_ptr<network::PeerClient>);

        virtual void OnUnchoked(std::shared_ptr<network::PeerClient>);

        virtual void OnInterested(std::shared_ptr<network::PeerClient>);

        virtual void OnNotInterested(std::shared_ptr<network::PeerClient>);

        virtual void OnHave(std::shared_ptr<network::PeerClient>);

        virtual void OnBittorrent(std::shared_ptr<network::PeerClient>);

        virtual void OnBitfield(std::shared_ptr<network::PeerClient>);

        virtual void OnRequest(std::shared_ptr<network::PeerClient>);

        virtual void OnPieceBlock(std::shared_ptr<network::PeerClient>);

        virtual void OnCancel(std::shared_ptr<network::PeerClient>);

        virtual void OnPort(std::shared_ptr<network::PeerClient>);

    protected:
        std::chrono::steady_clock::time_point start_;
    };
} // namespace bittorrent

#endif // CPPTORRENT_BITTORRENTSTRATEGY_H
