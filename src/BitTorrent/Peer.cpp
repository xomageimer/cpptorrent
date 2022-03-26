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

// TODO мб сделать map для пиров subscriber'ов, где ключом будет его IP
void bittorrent::MasterPeer::InitiateJob(boost::asio::io_service &service, const std::vector<PeerImage> &peers) {
    for (auto &peer: peers) {
        Subscribe(std::make_shared<network::PeerClient>(Get(), peer.BE_struct, ba::make_strand(service)));
    }
//      Subscribe(std::make_shared<network::PeerClient>(Get(), peers.begin()->BE_struct, ba::make_strand(service)));
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
    auto it = peers_subscribers_.emplace(new_sub->GetPeerData().GetIP(), new_sub);
    if (it.second)
        it.first->second->StartConnection();
}

void bittorrent::MasterPeer::Unsubscribe(IP unsub_ip) {
    LOG(peers_subscribers_.at(unsub_ip)->GetStrIP(), " was unsubscribed!");

    std::lock_guard lock(mut_);
    peers_subscribers_.erase(unsub_ip);
}

void bittorrent::MasterPeer::MakeHandshake() {
    handshake_message[0] = 0x13;
    std::memcpy(&handshake_message[1], "BitTorrent protocol", 19);
    std::memset(&handshake_message[20], 0x00, 8);// reserved bytes (last |= 0x01 for DHT or last |= 0x04 for FPE)
    std::memcpy(&handshake_message[28], GetInfoHash().data(), 20);
    std::memcpy(&handshake_message[48], GetID(), 20);
}
const uint8_t * bittorrent::MasterPeer::GetHandshake() const {
    return handshake_message;
}
