#ifndef CPPTORRENT_PEER_H
#define CPPTORRENT_PEER_H

#include <cctype>
#include <unordered_map>
#include <string>
#include <shared_mutex>

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY

#include "Primitives/Bitfield.h"

#include "constants.h"
#include "bencode_lib.h"
#include "logger.h"

namespace bittorrent {
    struct Torrent;
}

namespace network {
    struct PeerClient;

    struct Listener;
} // namespace network

namespace bittorrent {
    struct Peer {
    public:
        Peer();

        Peer(uint32_t ip_address, uint16_t port_number);

        Peer(uint32_t ip_address, uint16_t port_number, const uint8_t *key);

        [[maybe_unused]] explicit Peer(const uint8_t *key_arg);

        [[nodiscard]] const uint8_t *GetID() const { return id; }

        [[nodiscard]] virtual uint16_t GetPort() const { return port; }

        [[nodiscard]] size_t GetIP() const { return ip; }

        [[nodiscard]] bittorrent::Bitfield &GetBitfield() { return bitfield_; }

        [[nodiscard]] const bittorrent::Bitfield &GetBitfield() const { return bitfield_; }

    protected:
        uint8_t id[20];
        uint32_t ip{};
        uint16_t port{};

        bittorrent::Bitfield bitfield_{0};
    };

    struct PeerImage {
        bittorrent::Peer BE_struct; // PEERS with keys: peer_id, ip, port
        std::string BE_bin; // string consisting of multiples of 6 bytes. First 4 bytes are the IP address and last 2 bytes are the port
                            // number. All in network (big endian) notation.
    };

    struct MasterPeer : public Peer, std::enable_shared_from_this<MasterPeer> {
    public:
        using IP = uint32_t;

        explicit MasterPeer(bittorrent::Torrent &tor);

        void InitiateJob(boost::asio::io_service &service, std::vector<PeerImage> const &peers);

        void Subscribe(const std::shared_ptr<network::PeerClient> &new_sub);

        void Unsubscribe(IP unsub_ip);

        void SendHaveToAll(size_t piece_num);

        void MarkRequestedPiece(size_t piece_id);

        void UnmarkRequestedPiece(size_t piece_id);

        bool IsPieceRequested(size_t piece_id) const;

        void MarkUploadedPiece(size_t piece_id);

        void UnmarkUploadedPiece(size_t piece_id);

        bool IsPieceUploaded(size_t piece_id) const;

        bool IsPieceDone(size_t piece_id) const;

        auto Get() { return shared_from_this(); }

        [[nodiscard]] bittorrent::Torrent &GetTorrent();

        [[nodiscard]] std::string GetInfoHash() const;

        [[nodiscard]] size_t GetApplicationPort() const;

        [[nodiscard]] size_t GetTotalPiecesCount() const;

        [[nodiscard]] const uint8_t *GetHandshake() const;

        [[nodiscard]] bencode::Node const &GetChunkHashes() const;

        bool CanUnchokePeer(size_t peer_ip) const;

        [[nodiscard]] bittorrent::Bitfield &GetBitfield();

        [[nodiscard]] const bittorrent::Bitfield &GetBitfield() const;

    private:
        void MakeHandshake();

        uint8_t handshake_message_[bittorrent_constants::handshake_length]{};

        friend class bittorrent::Torrent;
        bittorrent::Torrent &torrent_;

        mutable std::shared_mutex mut_;
        mutable std::shared_mutex bitfield_mut_;

        std::unordered_map<IP, std::shared_ptr<network::PeerClient>> peers_subscribers_;

        std::set<size_t> uploaded_pieces_; // запрошенные куски ОТ нас!
        std::set<size_t> requested_pieces_; // запрошенные куски ДЛЯ нас!

        size_t available_unchoke_count_ = bittorrent_constants::MAX_AVAILABLE_UNCHOKE_ONE_TIME;
    };
} // namespace bittorrent

#endif // CPPTORRENT_PEER_H