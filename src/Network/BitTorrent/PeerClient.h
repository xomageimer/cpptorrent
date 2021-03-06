#ifndef CPPTORRENT_PEERCLIENT_H
#define CPPTORRENT_PEERCLIENT_H

#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <memory>
#include <string>
#include <utility>

#include "Network/NetExceptions.h"

#include "Network/Primitives/Socket.h"
#include "Peer.h"

#include "Primitives/BittorrentStrategy.h"
#include "Network/Primitives/Message.h"
#include "bitfield.h"
#include "Primitives/Piece.h"

#include "auxiliary.h"
#include "constants.h"
#include "logger.h"
#include "profile.h"

namespace ba = boost::asio;

using SendData = bittorrent::SendingMessage;
using RecvData = bittorrent::ReceivingMessage;
using RecvPeerData = bittorrent::ReceivingPeerMessage;
using SendPeerData = bittorrent::SendingPeerMessage;

extern TotalDuration try_to_request_time;
extern TotalDuration handle_response_time;

namespace network {
    enum STATE : uint8_t {
        am_choking = 0b0001,     // this client is choking the peer
        am_interested = 0b0010,  // this client is interested in the peer
        peer_choking = 0b0100,   // peer is choking this client
        peer_interested = 0b1000 // peer is interested in this client
    };

    struct PeerClient : public TCPSocket {
    public:
        using piece_index = size_t;

        friend class bittorrent::BittorrentStrategy;
        friend class bittorrent::OptimalStrategy;

        friend class bittorrent::MasterPeer;

        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const &master_peer, bittorrent::Peer slave_peer,
            const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor);

        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const &master_peer, ba::ip::tcp::socket socket);

        ~PeerClient();

        void Process(uint8_t *handshake_ptr, size_t size);

        void Disconnect();

        void TryToRequestPiece();

        void ClearAndTryRequest();

        std::string GetStrIP() const;

        auto GetPeerBitfield() { return slave_peer_.GetBitfield(); }

        bittorrent::Torrent &GetTorrent() { return master_peer_.GetTorrent(); }

        const bittorrent::Torrent &GetTorrent() const { return master_peer_.GetTorrent(); }

        bittorrent::Peer &GetPeerData() { return slave_peer_; }

        const bittorrent::Peer &GetPeerData() const { return slave_peer_; }

        size_t TotalPiecesCount() const { return master_peer_.GetTotalPiecesCount(); }

        void BindUpload(size_t id);

        void UnbindUpload(size_t id);

        void BindRequest(size_t id);

        void UnbindRequest(size_t id);

        bool IsClientChoked() const { return status_ & am_choking; }

        bool IsRemoteChoked() const { return status_ & peer_choking; }

        bool IsClientInterested() const { return status_ & am_interested; }

        bool IsRemoteInterested() const { return status_ & peer_interested; }

        bool PieceUploaded(uint32_t idx) const;

        bool PieceRequested(uint32_t idx) const;

        bool PieceDone(uint32_t idx) const;

        auto Strategy();

    private:
        bool check_handshake(const RecvPeerData &) const;

        void verify_handshake(std::vector<uint8_t> v);

        void access();

        void try_again_connect();

        void drop_timeout();

        void wait_piece();

        boost::asio::deadline_timer keep_alive_timeout_;

        boost::asio::deadline_timer piece_wait_timeout_;

        void remind_about_self();

        void do_read_header();

        void do_read_body();

        void handle_response();

        void error_callback(boost::system::error_code ec);

        void send_msg(SendPeerData data);

        void cancel_piece(uint32_t id);

        void send_handshake();

        void send_keep_alive();

        void send_choke();

        void send_unchoke();

        void send_interested();

        void send_not_interested();

        void send_have(uint32_t idx);

        void send_bitfield();

        void send_request(uint32_t piece_request_index, uint32_t begin, uint32_t length);

        void send_block(uint32_t pieceIdx, uint32_t offset, uint8_t * data, size_t size);

        void send_cancel(uint32_t pieceIdx, uint32_t offset, uint32_t length);

        void send_port(size_t port);

        mutable std::string cash_ip_;

        bool is_disconnected = false;

        size_t connect_attempts = bittorrent_constants::MAX_CONNECT_ATTEMPTS;

        std::optional<std::pair<bittorrent::Piece, size_t>> active_piece_;

        bittorrent::MasterPeer &master_peer_;

        bittorrent::Peer slave_peer_;

        RecvPeerData msg_to_read_;

        uint8_t status_ = STATE::am_choking | STATE::peer_choking;
    };
} // namespace network

#endif // CPPTORRENT_PEERCLIENT_H
