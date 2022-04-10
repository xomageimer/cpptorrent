#include "Listener.h"

void network::Listener::participant::Verify() {
    LOG("Listener"
        " : ",
        __FUNCTION__);
    auto self = Get();

    ba::async_read(socket_, ba::buffer(buff, bittorrent_constants::handshake_length),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred /*length*/) {
            timeout_.cancel();
            if (!ec && bytes_transferred >= bittorrent_constants::handshake_length) {
                std::string info_hash(reinterpret_cast<const char *>(&buff[28], 20));
                if (listener_.torrents.count(info_hash)) {
                    auto local_master_peer = listener_.torrents[info_hash]->GetRootPeer();

                    auto new_peer = std::make_shared<network::PeerClient>(local_master_peer, std::move(socket_), buff);
                    local_master_peer->Subscribe(new_peer);
                }
            }
        });
    timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
    timeout_.async_wait([this, self](boost::system::error_code const &ec) {
        if (!ec) {
            socket_.cancel();
            timeout_.cancel();
        }
    });
}

network::Listener::Listener(const boost::asio::strand<boost::asio::io_service::executor_type> &executor)
    : socket_(executor), acceptor_(executor) {
    get_port();
    do_accept();
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

network::Listener::~Listener() {
    socket_.close();
    acceptor_.close();
}

void network::Listener::do_accept() {
    LOG("Listener"
        " : ",
        __FUNCTION__);
    return;

    acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
        if (!ec) {
            std::make_shared<participant>(*this, std::move(socket_))->Verify();
        }
        do_accept();
    });
}