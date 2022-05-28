//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "Torrent.h"
#include "PeerClient.h"

#include "auxiliary.h"

#include <utility>

network::PeerClient::PeerClient(const std::shared_ptr<bittorrent::MasterPeer> &master_peer, bittorrent::Peer slave_peer,
    const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor)
    : TCPSocket(executor), keep_alive_timeout_(executor), piece_wait_timeout_(executor), master_peer_(*master_peer), slave_peer_(std::move(slave_peer)),
      active_piece_(std::nullopt) {}

network::PeerClient::PeerClient(
    const std::shared_ptr<bittorrent::MasterPeer> &master_peer, ba::ip::tcp::socket socket, uint8_t *handshake_ptr)
    : TCPSocket(std::move(socket)), keep_alive_timeout_(TCPSocket::socket_.get_executor()), piece_wait_timeout_(TCPSocket::socket_.get_executor()), master_peer_(*master_peer),
      active_piece_(std::nullopt) {

    asio::ip::tcp::endpoint remote_ep = socket_.remote_endpoint();
    uint32_t ip_int = IpToInt(remote_ep.address().to_string());
    uint16_t port = remote_ep.port();
    slave_peer_ = bittorrent::Peer{ip_int, port};

    SendData msg;
    msg.CopyFrom(handshake_ptr, bittorrent_constants::handshake_length);
    msg_to_read_.SetBuffer(msg.GetBufferData().data(), msg.Size());
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

void network::PeerClient::remind_about_self() {
    keep_alive_timeout_.cancel();
    keep_alive_timeout_.expires_from_now(bittorrent_constants::keep_alive_time);
    keep_alive_timeout_.async_wait([this, self = shared_from(this)](boost::system::error_code ec) {
        if (!ec && (IsClientInterested() || !IsRemoteChoked())) {
            send_keep_alive();
            if (IsClientInterested()) {
                LOG(GetStrIP(), " : still interested");
                TryToRequestPiece();
            }
        }
    });
}

void network::PeerClient::wait_piece() {
    piece_wait_timeout_.cancel();
    piece_wait_timeout_.expires_from_now(bittorrent_constants::piece_waiting_time);
    piece_wait_timeout_.async_wait([this, self = shared_from(this)](boost::system::error_code ec) {
        if (!ec && active_piece_) {
            if (!IsRemoteChoked()) {
                cancel_piece(active_piece_.value().index);
            }
            UnbindRequest(active_piece_.value().index);
            active_piece_.reset();
        }
    });
}

void network::PeerClient::do_read_header() {
    if (is_disconnected) return;

    LOG(GetStrIP(), " : ", "trying to read message");

    Read(
        bittorrent::header_length,
        [this](const RecvPeerData &data) {
            drop_timeout();

            uint32_t header;
            data >> header;

            msg_to_read_.DecodeHeader(header);

            if (!msg_to_read_.BodySize()) {
                LOG(GetStrIP(), " : ", "Keep-alive message");
            } else {
                do_read_body();
            }
        },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::do_read_body() {
    if (is_disconnected) return;

    LOG(GetStrIP(), " : trying to read payload of message");

    Read(
        msg_to_read_.BodySize(),
        [this](const RecvPeerData &data) mutable {
            LOG(GetStrIP(), " : correct message");

            msg_to_read_.SetBuffer(&data[0], msg_to_read_.BodySize());

            handle_response();
            do_read_header();
        },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::TryToRequestPiece() {
    if ((active_piece_.has_value())) return;
    if (IsRemoteChoked()) return;
    if (!IsClientInterested()) send_interested();

    LOG(GetStrIP(), " : ", __FUNCTION__);

    if (GetOwnerBitfield().Popcount() == TotalPiecesCount()) {
        send_not_interested();
        return;
    }

    auto chosen_piece_index = GetTorrent().DetermineNextPiece(GetPeerData());

    if (!chosen_piece_index) {
        send_not_interested();
        return;
    }

    LOG(GetStrIP(), " choose piece at number ", chosen_piece_index.value());

    size_t piece_size = (chosen_piece_index.value() == master_peer_.GetTotalPiecesCount() - 1) ? GetTorrent().GetLastPieceSize()
                                                                                               : GetTorrent().GetPieceSize();
    active_piece_.emplace(bittorrent::Piece{chosen_piece_index.value(),
        static_cast<size_t>(std::ceil(static_cast<double>(piece_size) / static_cast<double>(bittorrent_constants::most_request_size)))});
    uint32_t begin = 0;
    for (; piece_size > bittorrent_constants::most_request_size;
         piece_size -= bittorrent_constants::most_request_size, begin += bittorrent_constants::most_request_size) {
        send_request(active_piece_.value().index, begin, bittorrent_constants::most_request_size);
    }
    send_request(active_piece_.value().index, begin, piece_size);
    wait_piece();
}

bool network::PeerClient::PieceDone(uint32_t idx) const {
    return master_peer_.IsPieceDone(idx);
}

bool network::PeerClient::PieceRequested(uint32_t idx) const {
    return master_peer_.IsPieceRequested(idx);
}

bool network::PeerClient::PieceUploaded(uint32_t idx) const {
    return master_peer_.IsPieceUploaded(idx);
}

void network::PeerClient::BindUpload(size_t id) {
    master_peer_.MarkUploadedPiece(id);
}

void network::PeerClient::UnbindUpload(size_t id) {
    master_peer_.UnmarkUploadedPiece(id);
}

void network::PeerClient::BindRequest(size_t id) {
    master_peer_.MarkRequestedPiece(id);
}

void network::PeerClient::UnbindRequest(size_t id) {
    master_peer_.UnmarkRequestedPiece(id);
}

void network::PeerClient::cancel_piece(uint32_t id) {
    size_t begin = 0;
    size_t length = (id == master_peer_.GetTotalPiecesCount() - 1) ? GetTorrent().GetLastPieceSize() : GetTorrent().GetPieceSize();
    for (; length > bittorrent_constants::most_request_size;
         length -= bittorrent_constants::most_request_size, begin += bittorrent_constants::most_request_size)
        send_cancel(id, begin, bittorrent_constants::most_request_size);
    send_cancel(id, begin, length);
}

void network::PeerClient::access() {
    GetPeerBitfield() = std::move(bittorrent::Bitfield(GetOwnerBitfield().Size()));

    LOG(GetStrIP(), " was connected!");

    send_bitfield();
    // send_interested(); // TODO должно быть интересно и unchoked, только если у него есть нужные pieces

    drop_timeout();
    remind_about_self();
    do_read_header();
}

void network::PeerClient::verify_handshake() {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    if (check_handshake(msg_to_read_)) {
        SendData msg_;
        msg_.CopyFrom(msg_to_read_);
        Send(
            std::move(msg_), [this](size_t) { access(); }, std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
    } else {
        Disconnect();
    }
}

bool network::PeerClient::check_handshake(const RecvPeerData &data) const {
    if (data[0] == 0x13 && memcmp(&data[1], "BitTorrent protocol", 19) == 0 &&
        memcmp(&data[28], &master_peer_.GetHandshake()[28], 20) == 0) {
        return true;
    }
    return false;
}

void network::PeerClient::send_handshake() {
    LOG(GetStrIP(), " thread num #", std::this_thread::get_id());

    LOG(GetStrIP(), " : send handshake");

    auto failed_attempt = [this](boost::system::error_code ec) {
        LOG(GetStrIP(), " : try connect again");
        try_again_connect();
    };

    SendData msg;
    msg.CopyFrom(master_peer_.GetHandshake(), bittorrent_constants::handshake_length);
    Send(
        std::move(msg),
        [this, failed_attempt](size_t size) {
            Read(
                bittorrent_constants::handshake_length,
                [this](const RecvPeerData &data) {
                    LOG(GetStrIP(), " : correct answer");

                    LOG(GetStrIP(), " : \"handshake.TotalLength() >= bittorrent_constants::handshake_length\" == ",
                        data.Size() >= bittorrent_constants::handshake_length);
                    bool is_handshake_correct = check_handshake(data);
                    LOG(GetStrIP(), " : is handshake correct == ", is_handshake_correct);

                    if (data.Size() >= bittorrent_constants::handshake_length && check_handshake(data) &&
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

void network::PeerClient::send_msg(SendPeerData data) {
    if (is_disconnected) return;

    data.EncodeHeader();
    auto type = data.Type();
    Send(
        std::move(data),
        [this, type](size_t xfr) {
            LOG(GetStrIP(), " : data of type ", bittorrent::type_by_id_.at(type), " successfully sent");
            remind_about_self();
        },
        std::bind(&PeerClient::error_callback, this, std::placeholders::_1));
}

void network::PeerClient::send_keep_alive() {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    SendPeerData msg;

    send_msg(std::move(msg));
}

void network::PeerClient::send_choke() {
    LOG(GetStrIP(), " : ", __FUNCTION__);
    using namespace bittorrent;

    SendPeerData msg;
    msg << choke;
    status_ |= am_choking;

    send_msg(std::move(msg));
}

void network::PeerClient::send_unchoke() {
    LOG(GetStrIP(), " : ", __FUNCTION__);
    using namespace bittorrent;

    SendPeerData msg;
    msg << unchoke;
    status_ &= ~am_choking;

    send_msg(std::move(msg));
}

void network::PeerClient::send_interested() {
    LOG(GetStrIP(), " : ", __FUNCTION__);
    using namespace bittorrent;

    SendPeerData msg;
    msg << interested;
    status_ |= am_interested;

    send_msg(std::move(msg));
}

void network::PeerClient::send_not_interested() {
    LOG(GetStrIP(), " : ", __FUNCTION__);
    using namespace bittorrent;

    SendPeerData msg;
    msg << not_interested;
    status_ &= ~am_interested;

    send_msg(std::move(msg));
}

void network::PeerClient::send_have(uint32_t idx) {
    LOG(GetStrIP(), " : ", __FUNCTION__, " ", idx, " piece");
    using namespace bittorrent;

    SendPeerData msg;
    msg << have << idx;

    send_msg(std::move(msg));
}

void network::PeerClient::send_bitfield() {
    LOG(GetStrIP(), " : ", __FUNCTION__);
    using namespace bittorrent;

    SendPeerData msg;
    const auto &bitfs = GetOwnerBitfield();
    const auto bytesToSend = (size_t)ceil((float)GetOwnerBitfield().Size() / 8.0f);
    msg << bitfield;
    auto bits_like_bytes = GetOwnerBitfield().GetCast();
    msg.CopyFrom(bits_like_bytes.data(), bytesToSend);

    send_msg(std::move(msg));
}

void network::PeerClient::send_request(uint32_t piece_request_index, uint32_t begin, uint32_t length) {
    LOG(GetStrIP(), " : ", __FUNCTION__, " index: ", piece_request_index, ", begin: ", begin, ", length: ", length);

    SendPeerData msg;
    msg << (uint8_t)bittorrent::request;
    msg << piece_request_index << begin << length;

    send_msg(std::move(msg));
}

void network::PeerClient::send_block(uint32_t pieceIdx, uint32_t offset, uint8_t *data, size_t size) {
    LOG(GetStrIP(), " : ", __FUNCTION__);
    using namespace bittorrent;

    SendPeerData msg;
    msg << piece_block << pieceIdx << offset;
    msg.CopyFrom(data, size);

    send_msg(std::move(msg));
}

void network::PeerClient::send_cancel(uint32_t pieceIdx, uint32_t offset, uint32_t length) {
    LOG(GetStrIP(), " : ", __FUNCTION__);
    using namespace bittorrent;

    SendPeerData msg;
    msg << cancel << pieceIdx << offset << length;

    send_msg(std::move(msg));
    if (active_piece_.has_value()) active_piece_.value().current_blocks_num--;
}

void network::PeerClient::send_port(size_t port) {
    LOG(GetStrIP(), " : ", __FUNCTION__);

    SendPeerData msg;
    msg << bittorrent::port << port;

    send_msg(std::move(msg));
}

auto network::PeerClient::Strategy() {
    return GetTorrent().GetStrategy();
}

void network::PeerClient::handle_response() {
    auto message_type = msg_to_read_.Type();
    size_t payload_size = msg_to_read_.BodySize();
    auto &payload = msg_to_read_;

    using namespace bittorrent;
    switch (message_type) {
        case choke:
            LOG(GetStrIP(), " : choke-message");
            if (payload_size != 1) {
                LOG(GetStrIP(), " : invalid choke-message size");
                Disconnect();
                return;
            }
            status_ |= peer_choking;

            Strategy()->OnChoked(shared_from(this));

            break;
        case unchoke:
            LOG(GetStrIP(), " : unchoke-message");
            if (payload_size != 1) {
                LOG(GetStrIP(), " : invalid unchoke-message size");
                Disconnect();
                return;
            }
            status_ &= ~peer_choking;

            Strategy()->OnUnchoked(shared_from(this));

            break;
        case interested:
            LOG(GetStrIP(), " : interested-message");
            if (payload_size != 1) {
                LOG(GetStrIP(), " : invalid interested-message size");
                Disconnect();
                return;
            }
            status_ |= peer_interested;

            Strategy()->OnInterested(shared_from(this));

            break;
        case not_interested:
            LOG(GetStrIP(), " : not_interested-message");
            if (payload_size != 1) {
                LOG(GetStrIP(), " : invalid not_interested-message size");
                Disconnect();
                return;
            }
            status_ &= ~peer_interested;

            Strategy()->OnNotInterested(shared_from(this));

            break;
        case have: {
            LOG(GetStrIP(), " : have-message");
            if (payload_size != 5) {
                LOG(GetStrIP(), " : invalid have-message size");
                Disconnect();
                return;
            }
            uint32_t i;
            payload >> i;
            if (i >= TotalPiecesCount()) {
                LOG(GetStrIP(), " : bad index from have message, get ", i, " index bigger than ", TotalPiecesCount(),
                    " (count of total pieces). Ignoring.");
                return;
            }
            LOG(GetStrIP(), " correct index from have message: ", i);
            GetPeerBitfield().Set(i);

            Strategy()->OnHave(shared_from(this), i);

            break;
        }
        case bitfield: {
            LOG(GetStrIP(), " : bitfield-message");
            if (payload_size < 1) {
                LOG(GetStrIP(), " : bitfield-message empty, all bits are zero");
                return;
            }
            auto size = std::ceil((double)TotalPiecesCount() / (double)bittorrent_constants::byte_size);
            if (payload_size - 1 > size) {
                Disconnect();
                return;
            }
            for (size_t i = 1; i < payload_size; ++i) {
                for (size_t x = 0; x < 8; ++x) {
                    if (payload.Body()[i] & (1 << (7 - x))) {
                        size_t idx = (i - 1) * 8 + x;
                        if (idx < TotalPiecesCount()) GetPeerBitfield().Set(idx);
                    }
                }
            }

            Strategy()->OnBitfield(shared_from(this));

            break;
        }
        case request: {
            LOG(GetStrIP(), " : request-message");
            if (payload_size != 13) {
                LOG(GetStrIP(), " : invalid request-message size");
                Disconnect();
                return;
            }
            if (!IsRemoteInterested()) {
                LOG(GetStrIP(), " : peer requested piece block without showing interest");
                Disconnect();
                return;
            }
            if (IsClientChoked()) {
                LOG(GetStrIP(), " : peer requested piece while choked us");
                Disconnect();
                return;
            }

            uint32_t index, begin, length;
            payload >> index;
            payload >> begin;
            payload >> length;

            Strategy()->OnRequest(shared_from(this), index, begin, length);

            break;
        }
        case piece_block: {
            LOG(GetStrIP(), " : piece message");
            if (payload_size < 9) {
                LOG(GetStrIP(), " : invalid piece message");
                Disconnect();
                return;
            }

            uint32_t index, begin;
            payload >> index;
            payload >> begin;

            payload_size -= 9;

            Strategy()->OnPieceBlock(shared_from(this), index, begin, &payload.Body()[9], payload_size);

            break;
        }
        case cancel: {
            LOG(GetStrIP(), " : cancel");
            if (payload_size != 13) {
                LOG(GetStrIP(), " : invalid cancel message");
                Disconnect();
                return;
            }

            uint32_t index, begin, length;
            payload >> index;
            payload >> begin;
            payload >> length;

            Strategy()->OnCancel(shared_from(this), index, begin, length);

            break;
        }
        case port: {
            if (payload_size != 3) {
                LOG(GetStrIP(), " : invalid port-message size");
                Disconnect();
                return;
            }
            uint16_t port;
            payload >> port;

            Strategy()->OnPort(shared_from(this), port);

            break;
        }
        default:
            LOG(GetStrIP(), " : Unknown message of id ", (uint32_t)message_type, " received by peer. Ignoring.");
            break;
    }
}