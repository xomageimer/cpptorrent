#ifndef CPPTORRENT_PEER_H
#define CPPTORRENT_PEER_H

#include <string>
#include <cctype>

namespace bittorrent {
    struct Torrent;
}

namespace bittorrent {
    struct Peer {
    public:
        Peer();
        Peer(uint32_t ip_address, uint16_t port_number);
        explicit Peer(size_t key_arg);

        [[nodiscard]] size_t GetKey() const { return key; }
        [[nodiscard]] size_t GetPort() const { return port; }
        [[nodiscard]] size_t GetIP() const { return ip; }
    protected:
        size_t key;
        uint32_t ip;
        size_t port;
    };

    struct MasterPeer : public Peer {
    public:
        explicit MasterPeer(bittorrent::Torrent & tor) : torrent(tor) {}
    private:
        bittorrent::Torrent & torrent;
    };
}

#endif //CPPTORRENT_PEER_H
