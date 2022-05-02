//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "Torrent.h"
#include "PeerClient.h"

#include "auxiliary.h"

#include <utility>

// TODO пускай всё работает с PeerMessage, чем с Message (check_handshake и тп, чтобы было понятнее)!

network::PeerClient::PeerClient(const std::shared_ptr<bittorrent::MasterPeer> &master_peer, bittorrent::Peer slave_peer,
    const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor)
    : TCPSocket(executor), master_peer_(*master_peer), slave_peer_(std::move(slave_peer)) {}

network::PeerClient::PeerClient(
    const std::shared_ptr<bittorrent::MasterPeer> &master_peer, ba::ip::tcp::socket socket, uint8_t *handshake_ptr)
    : TCPSocket(std::move(socket)), master_peer_(*master_peer) {

    asio::ip::tcp::endpoint remote_ep = socket_.remote_endpoint();
    uint32_t ip_int = IpToInt(remote_ep.address().to_string());
    uint16_t port = remote_ep.port();
    slave_peer_ = bittorrent::Peer{ip_int, port};

    msg_.Reset(bittorrent_constants::handshake_length);
    for (size_t i = 0; i < bittorrent_constants::handshake_length; i++) {
        msg_.GetDataPointer()[i] = handshake_ptr[i];
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

    Cancel();
    master_peer_.Unsubscribe(GetPeerData().GetIP());
    is_disconnected = true;
}

void network::PeerClient::try_again_connect() {
    if (is_disconnected) return;

    if (--connect_attempts) {
        LOG(GetStrIP(), " : attempts ", connect_attempts);
        Connect(GetStrIP(), std::to_string(slave_peer_.GetPort()), std::bind(&PeerClient::send_handshake, this),
            std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
        Await(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
    } else
        Disconnect();
}

void network::PeerClient::Process() {
    if (socket_.is_open()) {
        Post([this] { verify_handshake(); });
    } else {
        Connect(GetStrIP(), std::to_string(slave_peer_.GetPort()), std::bind(&PeerClient::send_handshake, this),
            std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
        Await(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
    }
}

void network::PeerClient::error_callback(const std::string &err) {
    LOG(GetStrIP(), " get error : ", err);
    Disconnect();
}

void network::PeerClient::drop_timeout() {
    Await(bittorrent_constants::waiting_time + bittorrent_constants::epsilon, [this] { Disconnect(); });
}

void network::PeerClient::do_read_header() {
    LOG(GetStrIP(), " : ", "trying to read message");

    Read(
        bittorrent::PeerMessage::header_length,
        [this](Data data) {
            drop_timeout();

            msg_.SetHeader(ArrayToValue<uint32_t>(reinterpret_cast<const uint8_t *>(data.data())));
            msg_.DecodeHeader();

            if (!msg_.BodyLength()) {
                LOG(GetStrIP(), " : ", "Keep-alive message");
            } else {
                do_read_body();
            }
        },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::do_read_body() {
    LOG("trying to read payload of message");

    Read(
        bittorrent::PeerMessage::id_length + msg_.BodyLength(),
        [this](Data data) {
            LOG(GetStrIP(), " : correct message"); // TODO перенести msg в peer_message

            msg_.SetMessageType(bittorrent::PEER_MESSAGE_TYPE{data[0]});
            std::memcpy(msg_.Body(), data.data() + 1, data.size() - 1);

            handle_response();
        },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::try_to_request_piece() {
    if (GetOwnerBitfield().Popcount() == TotalPiecesCount()) {
        return;
    }
}

void network::PeerClient::receive_piece_block(uint32_t index, uint32_t begin, bittorrent::Block block) {
    GetTorrent().ReceivePieceBlock(index, begin, std::move(block));
}

void network::PeerClient::access() {
    GetPeerBitfield().Resize(master_peer_.GetTotalPiecesCount());

    LOG(GetStrIP(), " was connected!");

    send_bitfield();
    send_interested();

    drop_timeout();
    do_read_header();
}

void network::PeerClient::verify_handshake() {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    if (check_handshake(std::move(DowngradeMsg(msg_)))) {
        Send(
            msg_, [this](size_t) { access(); },
            std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
    } else {
        Disconnect();
    }
}

bool network::PeerClient::check_handshake(const bittorrent::Message& msg) const {
    if (msg.GetDataPointer()[0] == 0x13 && memcmp(&msg.GetDataPointer()[1], "BitTorrent protocol", 19) == 0) {
        return true;
    }
    return false;
}

void network::PeerClient::send_handshake() {
    LOG(GetStrIP(), " : send handshake");

    auto failed_attempt = [this](const std::string &) {
        LOG(GetStrIP(), " : try connect again");
        try_again_connect();
    };

    Send(
        {master_peer_.GetHandshake(), bittorrent_constants::handshake_length},
        [this, failed_attempt](size_t size) {
            Read(
                bittorrent_constants::handshake_length,
                [this](Data data) {
                    LOG(GetStrIP(), " : correct answer");

                    bittorrent::Message hndshke(bittorrent_constants::handshake_length);
                    std::memcpy(hndshke.GetDataPointer(), data.data(), data.size());

                    LOG (GetStrIP(), " : \"hndshke.BodyLength() >= bittorrent_constants::handshake_length\" == ", hndshke.BodyLength() >= bittorrent_constants::handshake_length);
                    LOG (GetStrIP(), " : \"memcmp(&hndshke.GetDataPointer()[28], &master_peer_.GetHandshake()[28], 20) == 0\" == ", memcmp(&hndshke.GetDataPointer()[28], &master_peer_.GetHandshake()[28], 20) == 0);

                    if (hndshke.BodyLength() >= bittorrent_constants::handshake_length && check_handshake(hndshke)
                        && memcmp(&hndshke.GetDataPointer()[28], &master_peer_.GetHandshake()[28], 20) == 0) {
                        LOG (GetStrIP(), " : correct verify");
                        access();
                    }
                },
                failed_attempt);
            Await(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
        },
        failed_attempt);
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

void network::PeerClient::handle_response() {
    auto message_type = msg_.GetMessageType();
    size_t payload_size = msg_.BodyLength();
    auto & payload = msg_;

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
            uint32_t i;
            payload >> i;
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
                    if (payload.Body()[i] & (1 << (7 - x))) {
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
            payload >> index;
            payload >> begin;
            payload >> length;

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
            payload >> index;
            payload >> begin;

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
                receive_piece_block(index, begin, Block{reinterpret_cast<uint8_t *>(payload.Body()[8]), payload_size});
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
            payload >> port;

            break;
    }
}