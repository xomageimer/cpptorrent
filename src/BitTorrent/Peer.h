#ifndef CPPTORRENT_PEER_H
#define CPPTORRENT_PEER_H

#include <cctype>
#include <set>
#include <string>

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY

#include "bt/Bitfield.h"

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

    protected:
        uint8_t id[20];
        uint32_t ip{};
        uint16_t port{};

    public:
        bittorrent::Bitfield bitfield_;
    };

    struct PeerImage {
        bittorrent::Peer BE_struct; // PEERS with keys: peer_id, ip, port
        std::string BE_bin; // string consisting of multiples of 6 bytes. First 4 bytes are the IP address and last 2 bytes are the port
                            // number. All in network (big endian) notation.
    };

    struct MasterPeer : public Peer, std::enable_shared_from_this<MasterPeer> {
    public:
        using IP = uint32_t;

        explicit MasterPeer(bittorrent::Torrent &tor) : torrent(tor) { MakeHandshake(); }

        void InitiateJob(boost::asio::io_service &service, std::vector<PeerImage> const &peers);

        void Subscribe(const std::shared_ptr<network::PeerClient> &new_sub);

        void Unsubscribe(IP unsub_ip);

        void RequestBlock(uint32_t index, uint32_t begin, uint32_t length);

        auto Get() { return shared_from_this(); }

        bittorrent::Torrent GetTorrent();

        std::string GetInfoHash() const;

        size_t GetApplicationPort() const;

        size_t GetTotalPiecesCount() const;

        const uint8_t *GetHandshake() const;

        bencode::Node const &GetChunkHashes() const;

        bittorrent::Bitfield &GetOwnerBitfield() { return bitfield_; }

    private:
        void MakeHandshake();

        uint8_t handshake_message[bittorrent_constants::handshake_length]{};

        friend class bittorrent::Torrent;
        bittorrent::Torrent &torrent;

        std::mutex mut_;

        std::map<IP, std::shared_ptr<network::PeerClient>> peers_subscribers_;
    };
} // namespace bittorrent

#endif // CPPTORRENT_PEER_H