//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "Torrent.h"
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
    cash_ip_ = IpToStr(slave_peer_.GetIP());
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
    post(socket_.get_executor(), [this, self] {
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
                                    GetPeerBitfield().Resize(master_peer_.GetTotalPiecesCount());

                                    LOG(GetStrIP(), " was connected!");

                                    send_bitfield();
                                    send_interested();

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
    auto self(Get());

    timeout_.expires_from_now(bittorrent_constants::waiting_time + bittorrent_constants::epsilon);
    timeout_.async_wait([this, self](boost::system::error_code ec) {
        if (!ec) {
            Disconnect();
        }
    });
}

void network::PeerClient::do_read_header() {
    LOG("trying to read message");
    auto self = Get();

    ba::async_read(socket_, ba::buffer(buff.data(), bittorrent::PeerMessage::header_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec || length < bittorrent::PeerMessage::header_length) {
                drop_timeout();
                buff.DecodeHeader();

                if (!buff.body_length()) {
                    LOG("Keep-alive message");
                } else {
                    do_read_body();
                }
            } else {
                Disconnect();
            }
        });
}

void network::PeerClient::do_read_body() {
    LOG("trying to read payload of message");

    auto self = Get();
    ba::async_read(socket_, ba::buffer(buff.body(), bittorrent::PeerMessage::id_length + buff.body_length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                auto message_type = buff.GetMessageType();
                size_t payload_size = buff.body_length();
                auto payload = buff.body();

                using namespace bittorrent;
                switch (message_type) {
                    case choke:
                        LOG("choke-message");
                        if (payload_size != 0) {
                            LOG("invalid choke-message size");
                            Disconnect();
                        }
                        status_ |= peer_choking;

                        break;
                    case unchoke:
                        LOG("unchoke-message");
                        if (payload_size != 0) {
                            LOG("invalid unchoke-message size");
                            Disconnect();
                        }
                        status_ &= ~peer_choking;

                        try_to_request_piece();

                        break;
                    case interested:
                        LOG("interested-message");
                        if (payload_size != 0) {
                            LOG("invalid interested-message size");
                            Disconnect();
                        }
                        status_ |= peer_interested;

                        if (IsClientChoked()) send_unchoke();

                        break;
                    case not_interested:
                        LOG("not_interested-message");
                        if (payload_size != 0) {
                            LOG("invalid not_interested-message size");
                            Disconnect();
                        }
                        status_ &= ~peer_interested;

                        break;
                    case have: {
                        LOG("have-message");
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

                        try_to_request_piece();

                        break;
                    }
                    case bitfield:
                        LOG("bitfield-message");
                        if (payload_size < 1) {
                            LOG("bitfield-message empty, all bits are zero");
                            return;
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

                        try_to_request_piece();

                        break;
                    case request: {
                        LOG("request-message");
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
                        index = SwapEndian(ArrayToValue<uint32_t>(&payload[0]));
                        begin = SwapEndian(ArrayToValue<uint32_t>(&payload[4]));
                        length = SwapEndian(ArrayToValue<uint32_t>(&payload[8]));

                        LOG("size of message:", BytesToHumanReadable(length));
                        if (length > bittorrent_constants::most_request_size) {
                            LOG("invalid size");
                            Disconnect();
                        }

                        // TODO проверка на наличие такого куска
                        send_piece(index, begin, length);

                        break;
                    }
                    case piece_block: {
                        LOG("piece message");
                        if (payload_size < 9) {
                            LOG("invalid piece message");
                            Disconnect();
                        }

                        uint32_t index, begin;
                        index = SwapEndian(ArrayToValue<uint32_t>(&payload[0]));
                        begin = SwapEndian(ArrayToValue<uint32_t>(&payload[4]));
                        payload_size -= 8;

                        // TODO реализовать метод IsClientRequested
                        if (!IsClientRequested(index)) {
                            LOG("received piece ", std::to_string(index), " which didn't asked");
                            return;
                        }

                        if (payload_size == 0 || payload_size > bittorrent_constants::most_request_size) {
                            LOG("received too big piece block of size ", BytesToHumanReadable(payload_size));
                        }

                        if (GetTorrent().PieceDone(index))
                            send_cancel(index);
                        else {
                            receive_piece_block(index, begin, Block{&payload[8], payload_size});
                        }

                        break;
                    }
                    case cancel:

                        break;
                    case port:
                        // TODO хз че тут надо
                        if (payload_size != 2) {
                            LOG("invalid port-message size");
                            Disconnect();
                        }
                        uint16_t port;
                        port = SwapEndian(ArrayToValue<uint16_t>(&payload[0]));

                        break;
                }
            } else {
                Disconnect();
            }
        });
}

void network::PeerClient::try_to_request_piece() {
    if (GetOwnerBitfield().Popcount() == TotalPiecesCount()) {
        return;
    }
}

void network::PeerClient::receive_piece_block(uint32_t index, uint32_t begin, bittorrent::Block block) {
    GetTorrent().ReceivePieceBlock(index, begin, std::move(block));
}

void network::PeerClient::send_choke() {}

void network::PeerClient::send_unchoke() {}

void network::PeerClient::send_interested() {}

void network::PeerClient::send_not_interested() {}

void network::PeerClient::send_have() {}

void network::PeerClient::send_bitfield() {}

void network::PeerClient::send_request(size_t piece_request_index) {}

void network::PeerClient::send_piece(uint32_t pieceIdx, uint32_t offset, uint32_t length) {}

void network::PeerClient::send_cancel(size_t piece_index) {}

void network::PeerClient::send_port(size_t port) {}
