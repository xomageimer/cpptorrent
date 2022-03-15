#include "Peer.h"
#include "Listener.h"
#include "PeerClient.h"
#include "Torrent.h"
#include "auxiliary.h"
#include "random_generator.h"

#include <memory>

bittorrent::Peer::Peer() {
    std::memcpy(&id[0], "-CP2060-", 8); // cpptorrent
    for (size_t i = 0; i < 12; i++) {
        id[8 + i] = random_generator::Random().GetNumber<uint8_t>();
    }
}
[[maybe_unused]] bittorrent::Peer::Peer(const uint8_t *key_arg) {
    for (size_t i = 0; i < 20; i++)
        id[i] = key_arg[i];
}
bittorrent::Peer::Peer(uint32_t ip_address, uint16_t port_number) : Peer() {
    ip = ip_address;
    port = port_number;
}
bittorrent::Peer::Peer(uint32_t ip_address, uint16_t port_number, const uint8_t *key) : Peer(key) {
    ip = ip_address;
    port = port_number;
}

void bittorrent::MasterPeer::InitiateJob(boost::asio::io_service &service, const std::vector<PeerImage> &peers) {
    for (auto &peer: peers) {
        auto ptr_peer = std::make_shared<network::PeerClient>(Get(), peer.BE_struct, ba::make_strand(service));
        Subscribe(ptr_peer);
        ptr_peer->start_connection();
    }
}

bencode::Node const &bittorrent::MasterPeer::GetChunkHashes() const {
    return torrent.GetMeta()["info"]["files"];
}

std::string bittorrent::MasterPeer::GetInfoHash() const {
    return torrent.GetInfoHash();
}

size_t bittorrent::MasterPeer::GetApplicationPort() const {
    return torrent.GetPort();
}

void bittorrent::MasterPeer::Subscribe(const std::shared_ptr<network::PeerClient> &new_sub) {
    LOG(new_sub->GetStrIP(), " was subscribed!");

    std::lock_guard lock(mut_);
    peers_subscribers_.insert(new_sub);
}

void bittorrent::MasterPeer::Unsubscribe(const std::shared_ptr<network::PeerClient> &unsub) {
    LOG(unsub->GetStrIP(), " was unsubscribed!");

    std::lock_guard lock(mut_);
    peers_subscribers_.erase(unsub);
}