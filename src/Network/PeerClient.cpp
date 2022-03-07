//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "PeerClient.h"

#include "auxiliary.h"

#include <utility>

// TODO попробовать без subscribe
network::PeerClient::PeerClient(const std::shared_ptr<bittorrent::MasterPeer> &master_peer, bittorrent::Peer slave_peer,
                                const boost::asio::strand<typename boost::asio::io_service::executor_type> & executor) : master_peer_(*master_peer), slave_peer_(std::move(slave_peer)), socket_(executor), resolver_(executor), timeout_(executor) {
    do_resolve();
}

network::PeerClient::~PeerClient() {
    socket_.close();
    timeout_.cancel();
}

// TODO реализовать полный дисконнект
void network::PeerClient::Disconnect() {
    LOG (GetStrIP(), " : ", "disconnect");

    socket_.close();
    timeout_.cancel();
    master_peer_.Unsubscribe(Get());
}

void network::PeerClient::do_resolve() {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    resolver_.async_resolve(GetStrIP(), std::to_string(slave_peer_.GetPort()),
                                         [this](boost::system::error_code ec,
                                                ba::ip::tcp::resolver::iterator endpoint) {
        if (!ec) {
            do_connect(std::move(endpoint));
            timeout_.async_wait([this](boost::system::error_code const & ec) {
                if (!ec) {
                    deadline();
                }
            });
        } else {
            Disconnect();
        }
    });
}

void network::PeerClient::do_connect(ba::ip::tcp::resolver::iterator endpoint) {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    ba::async_connect(socket_, std::move(endpoint), [this](boost::system::error_code ec, [[maybe_unused]] const ba::ip::tcp::resolver::iterator&) {
        timeout_.cancel();
        if (!ec) {
            do_handshake();
        } else {
            Disconnect();
        }
    });
    timeout_.expires_from_now(connection_waiting_time + epsilon);
}

void network::PeerClient::do_handshake() {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    MakeHandshake();
    ba::async_write(socket_, ba::buffer(handshake_message,
        std::size(handshake_message)),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                do_verify();
                timeout_.async_wait([this](boost::system::error_code const & ec) {
                    if (!ec) {
                        deadline();
                    }
                });
            } else {
                Disconnect();
            }
        });
}

void network::PeerClient::do_verify() {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    ba::async_read(socket_,
        ba::buffer(buff, MTU),
        [this](boost::system::error_code ec, std::size_t bytes_transferred/*length*/) {
            timeout_.cancel();
            bool wrong_answer = true;
            if ((!ec || ec == ba::error::eof)
                    && bytes_transferred >= 68 && buff[0] == 0x13
                    && memcmp(&buff[1], "BitTorrent protocol", 19) == 0
                    && memcmp(&buff[28], &handshake_message[28], 20) == 0)
            {
                wrong_answer = false;
                std::cerr << GetStrIP() << " was connected!" << std::endl;
                do_send_message();
                do_read_message();
            } else {
                std::cerr << "_____________________________________________" << std::endl;
                std::cerr << GetStrIP() << std::endl;
                std::cerr << ec.message() << std::endl;
                std::cerr << (bytes_transferred >= 68) << std::endl << (buff[0] == 0x13)
                << std::endl << (memcmp(&buff[1], "BitTorrent protocol", 19) == 0)
                << std::endl << (memcmp(&buff[28], &handshake_message[28], 20) == 0) << std::endl;
                std::cerr << "_____________________________________________" << std::endl;
            } if (wrong_answer) {
                if (--connect_attempts)
                    do_handshake();
                else Disconnect();
            }
        }
    );
    timeout_.expires_from_now(connection_waiting_time + epsilon);
}

void network::PeerClient::do_send_message() {

}

void network::PeerClient::do_read_message() {

}

void network::PeerClient::deadline() {
    LOG(GetStrIP(), " : ", "deadline was called");

    if (timeout_.expires_at() <=  ba::deadline_timer::traits_type::now()) {
        Disconnect();
    } else {
        timeout_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                deadline();
            }
        });
    }
}

void network::PeerClient::MakeHandshake() {
    handshake_message[0] = 0x13;
    std::memcpy(&handshake_message[1], "BitTorrent protocol", 19);
    std::memset(&handshake_message[20], 0x00, 8); // reserved bytes (last |= 0x01 for DHT or last |= 0x04 for FPE)
    std::memcpy(&handshake_message[28], master_peer_.GetInfoHash().data(), 20);
    std::memcpy(&handshake_message[48], master_peer_.GetID(), 20);
}

std::string network::PeerClient::GetStrIP() const {
    if (!cash_ip_.empty())
        return cash_ip_;

    size_t ip = slave_peer_.GetIP();
    std::vector<uint8_t> ip_address;
    while (ip) {
        ip_address.push_back(ip & 255);
        ip >>= 8;
    }
    std::reverse(ip_address.begin(), ip_address.end());

    std::string ip_address_str;
    bool is_first = true;
    for (auto & el : ip_address) {
        if (!is_first) {
            ip_address_str += '.';
        }
        is_first = false;
        ip_address_str += std::to_string(el);
    }

    cash_ip_ = ip_address_str;
    return cash_ip_;
}