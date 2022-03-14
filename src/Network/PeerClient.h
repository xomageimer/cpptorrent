#ifndef CPPTORRENT_PEERCLIENT_H
#define CPPTORRENT_PEERCLIENT_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/regex.hpp>

#include <memory>
#include <utility>
#include <string>

#include "Constants.h"
#include "NetExceptions.h"
#include "logger.h"

#include "Peer.h"
#include "Message.h"

namespace ba = boost::asio;

namespace network {
    enum STATE : uint8_t {
        am_choking = 0b0001,         // this client is choking the peer
        am_interested = 0b0010,      // this client is interested in the peer
        peer_choking = 0b0100,       // peer is choking this client
        peer_interested = 0b1000     // peer is interested in this client
    };
    struct PeerClient : public std::enable_shared_from_this<PeerClient> {
    public:
        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const & master_peer, bittorrent::Peer slave_peer, const boost::asio::strand<typename boost::asio::io_service::executor_type>& executor);
        ~PeerClient();

        void start_connection();
        std::string GetStrIP() const;

        auto Get() { return shared_from_this(); }
    private:
        void do_resolve();
        void do_connect(ba::ip::tcp::resolver::iterator endpoint);

        void do_handshake();
        void do_check_handshake();

        void do_verify();

        void deadline();

        void MakeHandshake();
        void Disconnect();

        void try_again();

        bittorrent::MasterPeer & master_peer_;
        bittorrent::Peer slave_peer_;
        mutable std::string cash_ip_;
        uint8_t handshake_message[bittorrent_constants::handshake_length] {};
        static const inline int MTU = bittorrent_constants::MTU;
        uint8_t buff[MTU] {};

        size_t connect_attempts = 3;

        uint8_t status_ = STATE::am_choking | STATE::peer_choking;
        ba::ip::tcp::socket socket_;
        ba::ip::tcp::resolver resolver_;

        ba::deadline_timer timeout_;
        static const inline boost::posix_time::milliseconds epsilon {boost::posix_time::milliseconds(15)}; // чтобы сразу не закончить таймер!
        static const inline boost::posix_time::milliseconds connection_waiting_time {boost::posix_time::milliseconds(2000)};
        bool is_disconnected = false;
    public:
        template <typename Function>
        void do_send_message(std::string const & msg, Function && callback){
            auto self = Get();

        }

        void do_read_message() {

        }
        template <typename Function>
        void do_read_message(Function && callback) {

        }
    };
}

#endif //CPPTORRENT_PEERCLIENT_H
