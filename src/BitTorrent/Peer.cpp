#include "Peer.h"

#include "Torrent.h"
#include "random_generator.h"

bittorrent::Peer::Peer() : key(random_generator::Random().GetNumber<size_t>()) {}
[[maybe_unused]] bittorrent::Peer::Peer(size_t key_arg) : key(key_arg) {}
bittorrent::Peer::Peer(uint32_t ip_address, uint16_t port_number)  : Peer() {
    ip = ip_address;
    port = port_number;
}

void bittorrent::MasterPeer::InitiateJob(boost::asio::io_service &service, const std::vector<PeerImage> &peers) {

}

bencode::Node const &bittorrent::MasterPeer::GetChunkHashes() const {
    return {};
}

size_t bittorrent::MasterPeer::GetApplicationPort() const {
    return torrent.GetPort();
}
