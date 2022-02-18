#include "Peer.h"

#include "random_generator.h"

bittorrent::Peer::Peer() : key(random_generator::Random().GetNumber<size_t>()) {}
bittorrent::Peer::Peer(size_t key_arg) : key(key_arg) {}
bittorrent::Peer::Peer(uint32_t ip_address, uint16_t port_number)  : Peer() {
    ip = ip_address;
    port = port_number;
}
