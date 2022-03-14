//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "PeerClient.h"

#include "auxiliary.h"

#include <utility>

// TODO сделать так чтобы захватывался self указатель!
network::PeerClient::PeerClient(const std::shared_ptr<bittorrent::MasterPeer> &master_peer, bittorrent::Peer slave_peer,
                                const boost::asio::strand<typename boost::asio::io_service::executor_type> & executor) : master_peer_(*master_peer), slave_peer_(std::move(slave_peer)), socket_(executor), resolver_(executor), timeout_(executor) {}

network::PeerClient::~PeerClient() {
    LOG (GetStrIP(), " : ", "destruction");

    Disconnect();
    socket_.close();
}

// TODO реализовать полный дисконнект
void network::PeerClient::Disconnect() {
    if (is_disconnected)
        return;

    LOG (GetStrIP(), " : ", __FUNCTION__);

    timeout_.cancel();
    socket_.cancel();
    master_peer_.Unsubscribe(Get());
    is_disconnected = true;
}

void network::PeerClient::start_connection() {
    do_resolve();
}

void network::PeerClient::try_again() {
    if (is_disconnected)
        return;

    auto self = Get();
    post(resolver_.get_executor(), [this, self] {
        timeout_.cancel();
        if (--connect_attempts) {
            LOG (GetStrIP(), " : attempts ", connect_attempts);
            timeout_.expires_from_now(connection_waiting_time + epsilon);
            timeout_.async_wait([this, self](boost::system::error_code const & ec) {
                if (!ec) {
                    LOG (GetStrIP(), " : ", "deadline timer from do_resolve");
                    do_resolve();
                }
            });
        } else Disconnect();
    });
}

void network::PeerClient::do_resolve() {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    resolver_.async_resolve(GetStrIP(), std::to_string(slave_peer_.GetPort()),
                                         [this, self](boost::system::error_code ec,
                                                ba::ip::tcp::resolver::iterator endpoint) {
        if (!ec) {
            do_connect(std::move(endpoint));
            LOG (GetStrIP(), " : ", "start timer!");
            timeout_.async_wait([this, self](boost::system::error_code const & ec) {
                if (!ec) {
                    LOG (GetStrIP(), " : ", "deadline timer from do_resolve");
                    deadline();
                }
            });
        } else {
            try_again();
        }
    });
}

void network::PeerClient::do_connect(ba::ip::tcp::resolver::iterator endpoint) {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    ba::async_connect(socket_, std::move(endpoint), [this, self](boost::system::error_code ec, [[maybe_unused]] const ba::ip::tcp::resolver::iterator&) {
        if (!ec) {
            timeout_.cancel();
            MakeHandshake();
            do_handshake();
        } else try_again();
    });
    timeout_.expires_from_now(connection_waiting_time + epsilon);
}

void network::PeerClient::do_handshake() {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    ba::async_write(socket_, ba::buffer(handshake_message,
        std::size(handshake_message)),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                do_check_handshake();
                LOG (GetStrIP(), " : ", "start timer!");
                timeout_.async_wait([this, self](boost::system::error_code const & ec) {
                    if (!ec) {
                        LOG (GetStrIP(), " : ", "deadline timer from do_handshake");
                        deadline();
                    }
                });
            } else {
                LOG (GetStrIP(), " : ", ec.message());
                try_again();
            }
        });
}

void network::PeerClient::do_check_handshake() {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    ba::async_read(socket_,
        ba::buffer(buff, 68),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred/*length*/) {
            timeout_.cancel();
            if (ec == boost::asio::error::operation_aborted)
                return;

            if ((!ec || ec == ba::error::eof)
                    && bytes_transferred >= 68 && buff[0] == 0x13
                    && memcmp(&buff[1], "BitTorrent protocol", 19) == 0
                    && memcmp(&buff[28], &handshake_message[28], 20) == 0)
            {
                std::cerr << GetStrIP() << " was connected!" << std::endl;
                do_read_message();
            } else {
                try_again();
            }
        }
    );
    timeout_.expires_from_now(connection_waiting_time + epsilon);
}

void network::PeerClient::do_verify() {
    LOG (GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    ba::async_read(socket_,
                   ba::buffer(buff, 68),
                   [this, self](boost::system::error_code ec, std::size_t bytes_transferred/*length*/) {
                       timeout_.cancel();
                       if (ec == boost::asio::error::operation_aborted)
                           return;

                       if ((!ec || ec == ba::error::eof)
                           && bytes_transferred >= 68 && buff[0] == 0x13
                           && memcmp(&buff[1], "BitTorrent protocol", 19) == 0
                           && memcmp(&buff[28], &handshake_message[28], 20) == 0)
                       {

                       } else {
                           try_again();
                       }
                   }
    );
    timeout_.expires_from_now(connection_waiting_time + epsilon);
}

void network::PeerClient::deadline() {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    if (timeout_.expires_at() <= ba::deadline_timer::traits_type::now()) {
        try_again();
    } else {
        LOG (GetStrIP(), " : ", "start timer!");
        auto self(Get());
        timeout_.async_wait([this, self](boost::system::error_code ec) {
            if (!ec) {
                LOG (GetStrIP(), " : ", "deadline timer from deadline!");
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
