#ifndef CPPTORRENT_PEER_H
#define CPPTORRENT_PEER_H

#include <string>
#include <random>
#include <cctype>

namespace bittorrent {
    struct Peer {
    public:
        Peer();
        Peer(uint32_t ip_address, uint16_t port_number);
        explicit Peer(size_t key_arg);

        [[nodiscard]] size_t GetKey() const { return key; }
        [[nodiscard]] size_t GetPort() const { return port; }
        [[nodiscard]] size_t GetIP() const { return ip; }
    private:
        size_t key;
        uint32_t ip;
        size_t port;
    };


}


#endif //CPPTORRENT_PEER_H
