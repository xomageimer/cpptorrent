#ifndef CPPTORRENT_PEERCLIENT_H
#define CPPTORRENT_PEERCLIENT_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <memory>
#include <string>
#include <utility>

#include "NetExceptions.h"

#include "Primitives/Socket.h"
#include "Peer.h"

#include "Primitives/BittorrentStrategy.h"
#include "Primitives/Message.h"
#include "Primitives/Bitfield.h"
#include "Primitives/Piece.h"

#include "auxiliary.h"
#include "constants.h"
#include "logger.h"

namespace ba = boost::asio;

using SendData = bittorrent::SendingMessage;
using RecvData = bittorrent::ReceivingMessage;
using RecvPeerData = bittorrent::ReceivingPeerMessage;
using SendPeerData = bittorrent::SendingPeerMessage;

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

        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const &master_peer, bittorrent::Peer slave_peer,
            const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor);

        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const &master_peer, ba::ip::tcp::socket socket, uint8_t *handshake_ptr);

        ~PeerClient();

        void Process();

        void Disconnect();

        void TryToRequest();

        std::string GetStrIP() const;

        bittorrent::Bitfield &GetPeerBitfield() { return slave_peer_.GetBitfield(); }

        bittorrent::Torrent &GetTorrent() { return master_peer_.GetTorrent(); }

        const bittorrent::Torrent &GetTorrent() const { return master_peer_.GetTorrent(); }

        bittorrent::Peer &GetPeerData() { return slave_peer_; }

        const bittorrent::Peer &GetPeerData() const { return slave_peer_; }

        size_t TotalPiecesCount() const { return master_peer_.GetTotalPiecesCount(); }

        const bittorrent::Bitfield &GetOwnerBitfield() const { return master_peer_.GetBitfield(); }

        void BindUpload(size_t id);

        void UnbindUpload(size_t id);

        bool IsClientChoked() const { return status_ & am_choking; }

        bool IsRemoteChoked() const { return status_ & peer_choking; }

        bool IsClientInterested() const { return status_ & am_interested; }

        bool IsRemoteInterested() const { return status_ & peer_interested; }

        bool PieceUploaded(uint32_t idx) const;

        bool PieceRequested(uint32_t idx) const;

        bool PieceDone(uint32_t idx) const;

    private:
        bool check_handshake(const RecvPeerData &) const;

        void verify_handshake();

        void access();

        void try_again_connect();

        void drop_timeout();

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

        std::map<piece_index, bittorrent::Piece> active_pieces_;

        const size_t max_active_pieces_ = bittorrent_constants::MAX_REQUESTED_PIECES_BY_PEER_ONE_TIME;

        bittorrent::MasterPeer &master_peer_;

        bittorrent::Peer slave_peer_;

        RecvPeerData msg_to_read_;

        uint8_t status_ = STATE::am_choking | STATE::peer_choking;
    };
} // namespace network

#endif // CPPTORRENT_PEERCLIENT_H
