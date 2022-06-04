#include "Listener.h"

#include <utility>

#include "Network/BitTorrent/PeerClient.h"
#include "Network/DHT/NodeClient.h"

void network::participant::Verify() {
    LOG("Listener"
        " : ",
        __FUNCTION__);
    auto self = Get();

    ba::async_read(socket_, ba::buffer(buff, bittorrent_constants::handshake_length),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred /*length*/) {
            LOG("Listener : ", "get message");
            timeout_.cancel();
            if (!ec && bytes_transferred >= bittorrent_constants::handshake_length) {
                std::string info_hash(reinterpret_cast<const char *>(&buff[28]), 20);
                LOG("Listener : ", "check handshake message == ", listener_.torrents_.count(info_hash), "\n", info_hash, " ~ ",
                    listener_.torrents_.begin()->second->GetInfoHash());
                if (listener_.torrents_.count(info_hash)) {
                    auto local_master_peer = listener_.torrents_[info_hash]->GetRootPeer();

                    auto new_peer = std::make_shared<network::PeerClient>(local_master_peer, std::move(socket_), buff);
                    LOG("Listener : ", "get ", new_peer->GetStrIP());
                    local_master_peer->Subscribe(new_peer);
                }
            }
        });
    timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
    timeout_.async_wait([this, self](boost::system::error_code const &ec) {
        if (!ec) {
            socket_.close();
            timeout_.cancel();
        }
    });
}

network::Listener::Listener(
    const boost::asio::strand<boost::asio::io_service::executor_type> &executor, std::shared_ptr<dht::MasterNode> dht_entry)
    : socket_(executor), dht_socket_(executor), acceptor_(executor), dht_entry_(std::move(dht_entry)) {
    get_port();
    do_accept();
    if (dht_entry) {
        dht_catcher();
    }
}

void network::Listener::AddTorrent(const std::shared_ptr<bittorrent::Torrent> &new_torrent) {
    torrents_.emplace(new_torrent->GetInfoHash(), new_torrent);
}

void network::Listener::get_port() {
    boost::system::error_code ec;
    do {
        if (port_ == bittorrent_constants::last_port) throw BadConnect("can't get free ports, try again later!");

        port_++;
        acceptor_.close();
        acceptor_.open(ba::ip::tcp::v4(), ec) || acceptor_.bind({ba::ip::tcp::v4(), static_cast<unsigned short>(port_)}, ec);
    } while (ec == ba::error::address_in_use);
    acceptor_.listen();
}

void network::Listener::do_accept() {
    LOG("Listener"
        " : ",
        __FUNCTION__);

    acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
        if (!ec) {
            std::make_shared<participant>(*this, std::move(socket_))->Verify();
        }
        do_accept();
    });
}

void network::Listener::dht_catcher() {
    dht_socket_.async_receive(boost::asio::buffer(buff_.prepare(bittorrent_constants::MTU)), [this](size_t bytes_transferred) {
        buff_.commit(bytes_transferred);

        dht_entry_->TryToInsertNode(std::make_shared<NodeClient>(std::move(dht_socket_), *dht_entry_));
        std::shared_ptr<KRPCQuery> rpc_query;
        std::istream is(&buff_);
        bencode::Document doc(bencode::Deserialize::LoadNode(is));
        auto type = doc.GetRoot()['q'].AsString();
        if (type == "ping") {
        }
        dht_entry_->AddRPC(std::move(rpc_query));
        dht_catcher();

        buff_.consume(bytes_transferred);
    });
}

network::Listener::~Listener() {
    socket_.close();
    dht_socket_.close();
    acceptor_.close();
}