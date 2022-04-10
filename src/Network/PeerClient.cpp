//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "PeerClient.h"

#include "auxiliary.h"

#include <utility>

network::PeerClient::PeerClient(const std::shared_ptr<bittorrent::MasterPeer> &master_peer, bittorrent::Peer slave_peer,
    const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor)
    : master_peer_(*master_peer), slave_peer_(std::move(slave_peer)), socket_(executor), resolver_(executor), timeout_(executor) {}

network::PeerClient::PeerClient(
    const std::shared_ptr<bittorrent::MasterPeer> &master_peer, ba::ip::tcp::socket socket, uint8_t *handshake_ptr)
    : master_peer_(*master_peer), socket_(std::move(socket)), resolver_(socket_.get_executor()), timeout_(socket_.get_executor()) {
    for (size_t i = 0; i < bittorrent_constants::handshake_length; i++) {
        buff.data()[i] = handshake_ptr[i];
    }
}

std::string network::PeerClient::GetStrIP() const {
    if (!cash_ip_.empty()) return cash_ip_;

    size_t ip = slave_peer_.GetIP();
    std::vector<uint8_t> ip_address;
    while (ip) {
        ip_address.push_back(ip & 255);
        ip >>= 8;
    }
    std::reverse(ip_address.begin(), ip_address.end());

    std::string ip_address_str;
    bool is_first = true;
    for (auto &el : ip_address) {
        if (!is_first) {
            ip_address_str += '.';
        }
        is_first = false;
        ip_address_str += std::to_string(el);
    }

    cash_ip_ = ip_address_str;
    return cash_ip_;
}

network::PeerClient::~PeerClient() {
    LOG(GetStrIP(), " : ", "destruction");

    timeout_.cancel();
    socket_.close();
}

// TODO реализовать полный дисконнект
void network::PeerClient::Disconnect() {
    if (is_disconnected) return;

    LOG(GetStrIP(), " : ", __FUNCTION__);

    timeout_.cancel();
    if (socket_.is_open()) socket_.cancel();
    master_peer_.Unsubscribe(GetPeerData().GetIP());
    is_disconnected = true;
}

void network::PeerClient::try_again_connect() {
    if (is_disconnected) return;

    auto self = Get();
    post(resolver_.get_executor(), [this, self] {
        if (--connect_attempts) {
            LOG(GetStrIP(), " : attempts ", connect_attempts);
            do_resolve();
            timeout_.async_wait([this, self](boost::system::error_code const &ec) {
                if (!ec) {
                    LOG(GetStrIP(), " : ", "deadline timer to make resolve");
                    deadline();
                }
            });
        } else
            Disconnect();
    });
}

void network::PeerClient::StartConnection() {
    auto self = Get();
    post (socket_.get_executor(), [this, self]{
        if (socket_.is_open()) {
            do_verify();
        } else {
            do_resolve();
        }
        timeout_.async_wait([this, self](boost::system::error_code const &ec) {
            if (!ec) {
                LOG(GetStrIP(), " : ", "deadline timer to make resolve");
                Disconnect();
            }
        });
    });
}

void network::PeerClient::do_resolve() {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    resolver_.async_resolve(GetStrIP(), std::to_string(slave_peer_.GetPort()),
        [this, self](boost::system::error_code ec, ba::ip::tcp::resolver::iterator endpoint) {
            if (!ec) {
                do_connect(std::move(endpoint));
                LOG(GetStrIP(), " : ", "start timer!");
                timeout_.async_wait([this, self](boost::system::error_code const &ec) {
                    if (!ec) {
                        LOG(GetStrIP(), " : ", "deadline timer from do_resolve");
                        deadline();
                    }
                });
            } else {
                Disconnect();
            }
        });
    timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
}

void network::PeerClient::do_connect(ba::ip::tcp::resolver::iterator endpoint) {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    auto self(Get());
    ba::async_connect(
        socket_, std::move(endpoint), [this, self](boost::system::error_code ec, [[maybe_unused]] const ba::ip::tcp::resolver::iterator &) {
            timeout_.cancel();
            if (!ec) {
                // do handshake
                send(std::string(reinterpret_cast<const char *>(master_peer_.GetHandshake()), bittorrent_constants::handshake_length),
                    [this, self]() {
                        ba::async_read(socket_, ba::buffer(buff.data(), bittorrent_constants::handshake_length),
                            [this, self](boost::system::error_code ec, std::size_t bytes_transferred /*length*/) {
                                timeout_.cancel();
                                if ((!ec || ec == ba::error::eof) && bytes_transferred >= 68 && buff.data()[0] == 0x13 &&
                                    memcmp(&buff.data()[1], "BitTorrent protocol", 19) == 0 &&
                                    memcmp(&buff.data()[28], &master_peer_.GetHandshake()[28], 20) == 0) {

                                    std::cerr << GetStrIP() << " was connected!" << std::endl;

                                    GetPeerBitfield().Resize(master_peer_.GetTotalPiecesCount());
                                    drop_timeout();
                                    do_read_header();
                                } else {
                                    try_again_connect();
                                }
                            });
                        timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
                    });
            } else {
                LOG(GetStrIP(), " : ", ec.message());
                try_again_connect();
            }
        });
    timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
}

void network::PeerClient::do_verify() {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    if (buff.data()[0] == 0x13 && memcmp(&buff.data()[1], "BitTorrent protocol", 19) == 0) {
        auto self(Get());
        send(std::string(reinterpret_cast<const char *>(buff.data()), bittorrent_constants::handshake_length), [this, self]() {
            drop_timeout();
            do_read_header();
        });
    } else {
        Disconnect();
    }
}

void network::PeerClient::deadline() {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    if (timeout_.expires_at() <= ba::deadline_timer::traits_type::now()) {
        timeout_.cancel();
        socket_.cancel();
    } else {
        LOG(GetStrIP(), " : ", "start timer!");
        auto self(Get());
        timeout_.async_wait([this, self](boost::system::error_code ec) {
            if (!ec) {
                LOG(GetStrIP(), " : ", "deadline timer from deadline!");
                deadline();
            }
        });
    }
}

void network::PeerClient::drop_timeout() {
    timeout_.cancel();
    timeout_.expires_from_now(bittorrent_constants::waiting_time + bittorrent_constants::epsilon);

    auto self(Get());
    timeout_.async_wait([this, self](boost::system::error_code ec) {
        if (!ec) {
            Disconnect();
        }
    });
}

void network::PeerClient::do_read_header() {
    LOG("trying to read message");
    auto self = Get();

    ba::async_read(socket_, ba::buffer(buff.data(), bittorrent::Message::header_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec || length < bittorrent::Message::header_length) {
                buff.decode_header();
                if (!buff.body_length()) {
                    LOG("Keep-alive message");
                    return drop_timeout();
                } else {
                    return do_read_body();
                }
            } else {
                Disconnect();
            }
        });
}
void network::PeerClient::do_read_body() {
    LOG("trying to read payload of message");

    auto self = Get();
    ba::async_read(socket_, ba::buffer(buff.body(), bittorrent::Message::id_length + buff.body_length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                auto message_type = buff.GetMessageType();
                size_t payload_size = buff.body_length() - 1;
                auto payload = buff.body();

                switch (message_type) {
                    case bittorrent::MESSAGE_TYPE::choke:
                        LOG("choke message");
                        if (payload_size != 0) {
                            LOG("invalid choke-message size");
                            Disconnect();
                        }
                        status_ |= peer_choking;

                        break;
                    case bittorrent::MESSAGE_TYPE::unchoke:
                        LOG("unchoke message");
                        if (payload_size != 0) {
                            LOG("invalid unchoke-message size");
                            Disconnect();
                        }
                        status_ &= ~peer_choking;
                        // TODO запросить необходимые куски!

                        break;
                    case bittorrent::MESSAGE_TYPE::interested:
                        LOG("interested message");
                        if (payload_size != 0) {
                            LOG("invalid interested-message size");
                            Disconnect();
                        }
                        status_ |= peer_interested;

                        if (IsClientChoked()) send_unchoke();

                        break;
                    case bittorrent::MESSAGE_TYPE::not_interested:
                        LOG("not interested message");
                        if (payload_size != 0) {
                            LOG("invalid not_interested-message size");
                            Disconnect();
                        }
                        status_ &= ~peer_interested;

                        break;
                    case bittorrent::MESSAGE_TYPE::have: {
                        LOG("have message");
                        if (payload_size != 4) {
                            LOG("invalid have-message size");
                            Disconnect();
                        }
                        uint32_t i = SwapEndian(ArrayToValue<uint32_t>(payload));
                        if (i < GetPeerBitfield().Size()) {
                            Disconnect();
                            break;
                        }

                        GetPeerBitfield().Set(i);
                        break;
                    }
                    case bittorrent::MESSAGE_TYPE::bitfield:
                        LOG("bitfield message");
                        if (payload_size == 0) {
                            LOG("invalid bitfield-message size");
                            Disconnect();
                        }
                        for (size_t i = 0; i < payload_size; i++) {
                            for (size_t x = 0; x < 8; x++) {
                                if (payload[i] & (1 << (7 - x))) {
                                    size_t idx = i * 8 + x;
                                    if (idx >= TotalPiecesCount()) {
                                        Disconnect();
                                        return;
                                    }
                                    GetPeerBitfield().Set(idx);
                                }
                            }
                        }

                        if (GetPeerBitfield().Popcount() != TotalPiecesCount()) {
                            // TODO тогда запросить еще куски
                        }

                        break;
                    case bittorrent::MESSAGE_TYPE::request:
                        LOG("request message");
                        if (payload_size != 12) {
                            LOG("invalid request-message size");
                            Disconnect();
                        }
                        if (!IsRemoteInterested()) {
                            LOG("peer requested piece block without showing interest");
                            Disconnect();
                        }
                        if (IsClientChoked()) {
                            LOG("peer requested piece while choked");
                            Disconnect();
                        }

                        uint32_t index, begin, length;
                        index = *(reinterpret_cast<uint32_t *>(&payload[0]));
                        begin = *(reinterpret_cast<uint32_t *>(&payload[4]));
                        length = *(reinterpret_cast<uint32_t *>(&payload[8]));

                        LOG("size of message:", BytesToHumanReadable(length));
                        if (length > bittorrent_constants::max_request_size) {
                            LOG("invalid size");
                            Disconnect();
                        }

                        break;
                    case bittorrent::MESSAGE_TYPE::piece_block:

                        break;
                    case bittorrent::MESSAGE_TYPE::cancel:

                        break;
                    case bittorrent::MESSAGE_TYPE::port:
                        // TODO хз че тут надо
                        if (payload_size != 2) {
                            LOG("invalid port-message size");
                            Disconnect();
                        }
                        uint16_t port;
                        port = *(reinterpret_cast<uint16_t *>(&payload[0]));

                        break;
                }
            } else {
                Disconnect();
            }
        });
}

void network::PeerClient::send_unchoke() {}
void network::PeerClient::request_piece(size_t piece_index) {}
void network::PeerClient::cancel_piece(size_t piece_index) {}
