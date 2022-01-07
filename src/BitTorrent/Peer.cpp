#include "Peer.h"

#include "random_generator.h"

bittorrent::Peer::Peer() : key(random_generator::Random().GetNumber<size_t>()) {}
bittorrent::Peer::Peer(size_t key_arg) : key(key_arg) {}
