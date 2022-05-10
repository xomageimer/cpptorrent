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

#include "Primitives/Message.h"
#include "bt/Bitfield.h"
#include "bt/Piece.h"

#include "auxiliary.h"
#include "constants.h"
#include "logger.h"

namespace ba = boost::asio;

using DataPtr = std::shared_ptr<bittorrent::Message>;
using PeerDataPtr = std::shared_ptr<bittorrent::PeerMessage>;

// TODO вынести основную часть сети куда-нибудь отдельно и наследоваться от нее

namespace network {
    enum STATE : uint8_t {
        am_choking = 0b0001,     // this client is choking the peer
        am_interested = 0b0010,  // this client is interested in the peer
        peer_choking = 0b0100,   // peer is choking this client
        peer_interested = 0b1000 // peer is interested in this client
    };
    // TODO переписать с использованием сокета
    struct PeerClient : public TCPSocket {
    public:
        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const &master_peer, bittorrent::Peer slave_peer,
            const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor);

        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const &master_peer, ba::ip::tcp::socket socket, uint8_t *handshake_ptr);

        ~PeerClient();

        void Process();

        std::string GetStrIP() const;

        bittorrent::Bitfield &GetPeerBitfield() { return slave_peer_.GetBitfield(); }

        bittorrent::Torrent &GetTorrent() { return master_peer_.GetTorrent(); }

        const bittorrent::Peer &GetPeerData() const { return slave_peer_; }

        size_t TotalPiecesCount() const { return master_peer_.GetTotalPiecesCount(); }

        const bittorrent::Bitfield &GetOwnerBitfield() const { return master_peer_.GetBitfield(); }

        bool IsClientChoked() const { return status_ & am_choking; }

        bool IsRemoteChoked() const { return status_ & peer_choking; }

        bool IsClientInterested() const { return status_ & am_interested; }

        bool IsRemoteInterested() const { return status_ & peer_interested; }

        bool IsClientRequested(uint32_t idx) const {}

        void Disconnect();

    private:
        bool check_handshake(const DataPtr &) const;

        void verify_handshake();

        void access();

        void try_again_connect();

        void drop_timeout();

        void do_read_header();

        void do_read_body();

        void handle_response();

        void error_callback(boost::system::error_code ec);

        mutable std::string cash_ip_;

        bool is_disconnected = false;

        size_t connect_attempts = bittorrent_constants::MAX_CONNECT_ATTEMPTS;

        void try_to_request_piece();

        void receive_piece_block(uint32_t index, uint32_t begin, bittorrent::Block block);

        void send_handshake();

        void send_choke();

        void send_unchoke();

        void send_interested();

        void send_not_interested();

        void send_have();

        void send_bitfield();

        void send_request(size_t piece_request_index);

        void send_piece(uint32_t pieceIdx, uint32_t offset, uint32_t length);

        void send_cancel(size_t piece_index);

        void send_port(size_t port);

        bittorrent::MasterPeer &master_peer_;

        bittorrent::Peer slave_peer_;

        // TODO сделать отдельно сетевой интерфейс в виде сокета
        PeerDataPtr msg_{};

        uint8_t status_ = STATE::am_choking | STATE::peer_choking;

        // TODO задать битовое поле из торрент структуры
    };
} // namespace network

#endif // CPPTORRENT_PEERCLIENT_H
