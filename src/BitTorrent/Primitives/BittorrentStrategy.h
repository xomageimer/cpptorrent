#ifndef CPPTORRENT_BITTORRENTSTRATEGY_H
#define CPPTORRENT_BITTORRENTSTRATEGY_H

#include <chrono>
#include <optional>
#include <cctype>

#include "Piece.h"
#include "Bitfield.h"
#include "Peer.h"

#include "logger.h"

namespace network {
    struct Torrent;
    struct PeerClient;
} // namespace network

namespace bittorrent {
    // default strategy
    struct BittorrentStrategy {
        BittorrentStrategy();

        virtual std::optional<size_t> ChoosePiece(MasterPeer &peer_owner, const Peer &peer_neigh);

        void OnBlockReadyToSend(std::shared_ptr<network::PeerClient> peer, uint32_t pieceIdx, uint32_t offset, Block block);

        virtual void OnPieceDownloaded(size_t id, [[maybe_unused]] Torrent &torrent);

        virtual void OnChoked([[maybe_unused]] std::shared_ptr<network::PeerClient>); // do_nothing

        virtual void OnUnchoked([[maybe_unused]] std::shared_ptr<network::PeerClient>);

        virtual void OnInterested([[maybe_unused]] std::shared_ptr<network::PeerClient>);

        virtual void OnNotInterested([[maybe_unused]] std::shared_ptr<network::PeerClient>);

        virtual void OnHave(std::shared_ptr<network::PeerClient>, uint32_t);

        virtual void OnBitfield([[maybe_unused]] std::shared_ptr<network::PeerClient>);

        virtual void OnRequest([[maybe_unused]] std::shared_ptr<network::PeerClient>, uint32_t, uint32_t, uint32_t);

        virtual void OnPieceBlock([[maybe_unused]] std::shared_ptr<network::PeerClient>, uint32_t, uint32_t, const uint8_t *, uint32_t);

        virtual void OnCancel([[maybe_unused]] std::shared_ptr<network::PeerClient>, uint32_t, uint32_t, uint32_t);

        virtual void OnPort([[maybe_unused]] std::shared_ptr<network::PeerClient>, uint32_t) { return; };

    protected:
        std::chrono::steady_clock::time_point start_;

        std::mutex out_mutex;
    };

    struct OptimalStrategy : public BittorrentStrategy {
        std::optional<size_t> ChoosePiece(MasterPeer &peer_owner, const Peer &peer_neigh) override;

        void OnPieceDownloaded(size_t id, [[maybe_unused]] Torrent &torrent) override;

        void OnPieceBlock([[maybe_unused]] std::shared_ptr<network::PeerClient>, uint32_t, uint32_t, const uint8_t *, uint32_t) override;

    private:
        bool end_game_ = false;
    };
} // namespace bittorrent

#endif // CPPTORRENT_BITTORRENTSTRATEGY_H
