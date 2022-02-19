#ifndef CPPTORRENT_PEER_H
#define CPPTORRENT_PEER_H

#include <string>
#include <cctype>

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY

#include "bencode_lib.h"

namespace bittorrent {
    struct Torrent;
}

namespace network {
    struct PeerClient;
}

namespace bittorrent {
    struct Peer {
    public:
        Peer();
        Peer(uint32_t ip_address, uint16_t port_number);
        explicit Peer(size_t key_arg);

        [[nodiscard]] size_t GetKey() const { return key; }

        [[nodiscard]] virtual size_t GetPort() const { return port; }
        [[nodiscard]] size_t GetIP() const { return ip; }
    protected:
        size_t key;
        uint32_t ip;
        size_t port;
    };
    struct PeerImage{
        bittorrent::Peer BE_dict; // PEERS with keys: peer_id, ip, port
        std::string BE_bin; // string consisting of multiples of 6 bytes. First 4 bytes are the IP address and last 2 bytes are the port number. All in network (big endian) notation.
    };

    struct MasterPeer : public Peer, std::enable_shared_from_this<MasterPeer> {
    public:
        explicit MasterPeer(bittorrent::Torrent & tor) : torrent(tor) {}

        void InitiateJob(boost::asio::io_service & service, std::vector<PeerImage> const & peers);
        auto Get() { return shared_from_this(); }
        size_t GetApplicationPort() const;
        bencode::Node const & GetChunkHashes() const;
    private:
        bittorrent::Torrent & torrent;
        std::shared_ptr<network::PeerClient> new_client;
    };
}

#endif //CPPTORRENT_PEER_H
