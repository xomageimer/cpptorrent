#ifndef CPPTORRENT_PEER_H
#define CPPTORRENT_PEER_H

#include <random>
#include <cctype>

namespace bittorrent {
    struct Peer {
    public:
        Peer();
        explicit Peer(size_t key_arg);
        [[nodiscard]] size_t GetKey() const { return key; }
    private:
        size_t key;
    };
}


#endif //CPPTORRENT_PEER_H
