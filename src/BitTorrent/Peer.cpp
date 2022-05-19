#include "Peer.h"
#include "Torrent.h"

#include "Listener.h"
#include "PeerClient.h"

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
    bitfield_.Resize(torrent_.GetPieceCount());
    for (auto &peer : peers) {
        Subscribe(std::make_shared<network::PeerClient>(Get(), peer.BE_struct, ba::make_strand(service)));
    }
}

bencode::Node const &bittorrent::MasterPeer::GetChunkHashes() const {
    return torrent_.GetMeta()["info"]["files"];
}

std::string bittorrent::MasterPeer::GetInfoHash() const {
    return torrent_.GetInfoHash();
}

size_t bittorrent::MasterPeer::GetApplicationPort() const {
    return torrent_.GetPort();
}

void bittorrent::MasterPeer::Subscribe(const std::shared_ptr<network::PeerClient> &new_sub) {
    std::lock_guard lock(mut_);

    LOG(new_sub->GetStrIP(), " was subscribed!");

    auto it = peers_subscribers_.emplace(new_sub->GetPeerData().GetIP(), new_sub);
    if (it.second) it.first->second->Process();
}

void bittorrent::MasterPeer::Unsubscribe(IP unsub_ip) {
    std::lock_guard lock(mut_);

    LOG(peers_subscribers_.at(unsub_ip)->GetStrIP(), " was unsubscribed!");

    peers_subscribers_.erase(unsub_ip);

    // TODO если пиров не осталось или осталось очень мало, то можно попробовать сразу начать общение с трекерами

    LOG("peers remain: ", (int)peers_subscribers_.size());
}

void bittorrent::MasterPeer::MakeHandshake() {
    handshake_message_[0] = 0x13;
    std::memcpy(&handshake_message_[1], "BitTorrent protocol", 19);
    std::memset(&handshake_message_[20], 0x00, 8); // reserved bytes (last |= 0x01 for DHT or last |= 0x04 for FPE)
    std::memcpy(&handshake_message_[28], GetInfoHash().data(), 20);
    std::memcpy(&handshake_message_[48], GetID(), 20);
}

const uint8_t *bittorrent::MasterPeer::GetHandshake() const {
    return handshake_message_;
}

size_t bittorrent::MasterPeer::GetTotalPiecesCount() const {
    return torrent_.GetPieceCount();
}

bittorrent::Torrent &bittorrent::MasterPeer::GetTorrent() {
    return torrent_;
}

const bittorrent::Bitfield &bittorrent::MasterPeer::GetBitfield() const {
    return torrent_.GetOwnerBitfield();
}

bool bittorrent::MasterPeer::CanUnchokePeer(size_t peer_ip) const {
    if (!peers_subscribers_.count(peer_ip)) {
        return false;
    }
    return available_unchoke_count_.load() == 0;
}

void bittorrent::MasterPeer::SendHaveToAll(size_t piece_num) {
    for (auto & [id, peer] : peers_subscribers_){
        peer->send_have(piece_num);
    }
}
