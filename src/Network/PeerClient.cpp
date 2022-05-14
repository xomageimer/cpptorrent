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

    PeerData msg_data{&msg_};
    msg_data.GetBuf().prepare(bittorrent_constants::handshake_length);
    msg_data.CopyFrom(handshake_ptr, bittorrent_constants::handshake_length);
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
            std::bind(&PeerClient::try_again_connect, this));
        Await(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
    }
}

void network::PeerClient::error_callback(boost::system::error_code ec) {
    LOG(GetStrIP(), " get error : ", ec.message());
    Disconnect();
}

void network::PeerClient::drop_timeout() {
    Await(bittorrent_constants::waiting_time + bittorrent_constants::epsilon, [this] { Disconnect(); });
}

void network::PeerClient::do_read_header() {
    LOG(GetStrIP(), " : ", "trying to read message");

    Read(
        sizeof(bittorrent::PeerMessage::header_length),
        [this](const Data &data) {
            drop_timeout();

            uint32_t header;
            data >> header;

            PeerData msg_data{&msg_};
            msg_data.SetHeader(header);

            if (!msg_data.GetBodySize()) {
                LOG(GetStrIP(), " : ", "Keep-alive message");
            } else {
                do_read_body();
            }
        },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::do_read_body() {
    LOG(GetStrIP(), " : trying to read payload of message");

    PeerData data_msg{&msg_};
    Read(
        bittorrent::PeerMessage::id_length + data_msg.GetBodySize(),
        [data_msg, this](const Data &data) mutable {
            LOG(GetStrIP(), " : correct message");

            data_msg.CopyFrom(data);
            handle_response();
        },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::try_to_request_piece() {
    LOG(GetStrIP(), " : ", __FUNCTION__);

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

    PeerData msg_data{&msg_};
    if (check_handshake(msg_data)) {
        Send(
            msg_data, [this](size_t) { access(); }, std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
    } else {
        Disconnect();
    }
}

bool network::PeerClient::check_handshake(const Data &data) const {
    if (data[0] == 0x13 && memcmp(&data[1], "BitTorrent protocol", 19) == 0 &&
        memcmp(&data[28], &master_peer_.GetHandshake()[28], 20) == 0) {
        return true;
    }
    return false;
}

void network::PeerClient::send_handshake() {
    LOG(GetStrIP(), " : send handshake");

    auto failed_attempt = [this](boost::system::error_code ec) {
        LOG(GetStrIP(), " : try connect again");
        try_again_connect();
    };

    Data msg_data{&msg_};
    msg_data.GetBuf().consume(msg_data.GetBuf().size());
    msg_data.CopyFrom(master_peer_.GetHandshake(), bittorrent_constants::handshake_length);
    Send(
        msg_data,
        [this, failed_attempt](size_t size) {
            Read(
                bittorrent_constants::handshake_length,
                [this](const Data &data) {
                    LOG(GetStrIP(), " : correct answer");

                    LOG(GetStrIP(), " : \"handshake.TotalLength() >= bittorrent_constants::handshake_length\" == ",
                        data.GetBuf().size() >= bittorrent_constants::handshake_length);
                    bool is_handshake_correct = check_handshake(data);
                    LOG(GetStrIP(), " : is handshake correct == ", is_handshake_correct);

                    if (data.GetBuf().size() >= bittorrent_constants::handshake_length && check_handshake(data) &&
                        memcmp(&data[28], &master_peer_.GetHandshake()[28], 20) == 0) {
                        LOG(GetStrIP(), " : correct verify");
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

void network::PeerClient::send_interested() {
    using namespace bittorrent;

    PeerData msg_data(&msg_);
    msg_data.Clear();

    msg_data.SetHeader(1);
    msg_data << interested;

    Send(
        msg_data, [this](size_t xfr) { LOG(GetStrIP(), " : owner interested now"); },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::send_not_interested() {}

void network::PeerClient::send_have() {}

void network::PeerClient::send_bitfield() {
    using namespace bittorrent;

    PeerData msg_data(&msg_);
    msg_data.Clear();

    const auto bytesToSend = (size_t)ceil((float)GetOwnerBitfield().Size() / 8.0f);
    msg_data.SetHeader(1 + bytesToSend);
    msg_data << bitfield;

    auto bits_like_bytes = GetOwnerBitfield().GetCast();
    msg_data.CopyFrom(reinterpret_cast<const void *>(bits_like_bytes.data()), bytesToSend);

    Send(
        msg_data, [this](size_t xfr) { LOG(GetStrIP(), " : owner bitfield of ", xfr, " bytes send"); },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::send_request(size_t piece_request_index) {}

void network::PeerClient::send_piece(uint32_t pieceIdx, uint32_t offset, uint32_t length) {}

void network::PeerClient::send_cancel(size_t piece_index) {}

void network::PeerClient::send_port(size_t port) {}

void network::PeerClient::handle_response() {
    PeerData msg_data(&msg_);

    auto message_type = msg_data.GetType();
    size_t payload_size = msg_data.GetBodySize();
    auto &payload = msg_data;

    using namespace bittorrent;
    switch (message_type) {
        case choke:
            LOG(GetStrIP(), " : choke-message");
            if (payload_size != 0) {
                LOG(GetStrIP(), " : invalid choke-message size");
                Disconnect();
            }
            status_ |= peer_choking;

            break;
        case unchoke:
            LOG(GetStrIP(), " : unchoke-message");
            if (payload_size != 0) {
                LOG(GetStrIP(), " : invalid unchoke-message size");
                Disconnect();
            }
            status_ &= ~peer_choking;

            try_to_request_piece();

            break;
        case interested:
            LOG(GetStrIP(), " : interested-message");
            if (payload_size != 0) {
                LOG(GetStrIP(), " : invalid interested-message size");
                Disconnect();
            }
            status_ |= peer_interested;

            if (IsClientChoked()) send_unchoke();

            break;
        case not_interested:
            LOG(GetStrIP(), " : not_interested-message");
            if (payload_size != 0) {
                LOG(GetStrIP(), " : invalid not_interested-message size");
                Disconnect();
            }
            status_ &= ~peer_interested;

            break;
        case have: {
            LOG(GetStrIP(), " : have-message");
            if (payload_size != 4) {
                LOG(GetStrIP(), " : invalid have-message size");
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
            LOG(GetStrIP(), " : bitfield-message");
            if (payload_size < 1) {
                LOG(GetStrIP(), " : bitfield-message empty, all bits are zero");
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
            LOG(GetStrIP(), " : request-message");
            if (payload_size != 12) {
                LOG(GetStrIP(), " : invalid request-message size");
                Disconnect();
            }
            if (!IsRemoteInterested()) {
                LOG(GetStrIP(), " : peer requested piece block without showing interest");
                Disconnect();
            }
            if (IsClientChoked()) {
                LOG(GetStrIP(), " : peer requested piece while choked");
                Disconnect();
            }

            uint32_t index, begin, length;
            payload >> index;
            payload >> begin;
            payload >> length;

            LOG(GetStrIP(), " : size of message:", BytesToHumanReadable(length));
            if (length > bittorrent_constants::most_request_size) {
                LOG(GetStrIP(), " : invalid size");
                Disconnect();
            }

            // TODO проверка на наличие такого куска
            send_piece(index, begin, length);

            break;
        }
        case piece_block: {
            LOG(GetStrIP(), " : piece message");
            if (payload_size < 9) {
                LOG(GetStrIP(), " : invalid piece message");
                Disconnect();
            }

            uint32_t index, begin;
            payload >> index;
            payload >> begin;

            payload_size -= 8;

            // TODO реализовать метод IsClientRequested
            if (!IsClientRequested(index)) {
                LOG(GetStrIP(), " : received piece ", std::to_string(index), " which didn't asked");
                return;
            }

            if (payload_size == 0 || payload_size > bittorrent_constants::most_request_size) {
                LOG(GetStrIP(), " : received too big piece block of size ", BytesToHumanReadable(payload_size));
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
                LOG(GetStrIP(), " : invalid port-message size");
                Disconnect();
            }
            uint16_t port;
            payload >> port;

            break;
    }
}