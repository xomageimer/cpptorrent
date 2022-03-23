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

#include "Constants.h"
#include "NetExceptions.h"
#include "logger.h"

#include "Message.h"
#include "Peer.h"

namespace ba = boost::asio;

namespace network {
    enum STATE : uint8_t {
        am_choking = 0b0001,    // this client is choking the peer
        am_interested = 0b0010, // this client is interested in the peer
        peer_choking = 0b0100,  // peer is choking this client
        peer_interested = 0b1000// peer is interested in this client
    };
    struct PeerClient : public std::enable_shared_from_this<PeerClient> {
    public:
        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const &master_peer, bittorrent::Peer slave_peer, const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor);
        explicit PeerClient(std::shared_ptr<bittorrent::MasterPeer> const &master_peer, ba::ip::tcp::socket socket, uint8_t * handshake_ptr);
        ~PeerClient();

        void StartConnection();
        std::string GetStrIP() const;

        auto Get() { return shared_from_this(); }
        void Disconnect();

    private:
        void do_resolve();
        void do_connect(ba::ip::tcp::resolver::iterator endpoint);
        void do_verify();

        void try_again_connect();

        void deadline();

        bittorrent::MasterPeer &master_peer_;
        bittorrent::Peer slave_peer_;
        mutable std::string cash_ip_;
        static const inline int MTU = bittorrent_constants::MTU;
        uint8_t buff[MTU]{};
        std::deque<bittorrent::Message> message_queue_;

        size_t connect_attempts = bittorrent_constants::MAX_CONNECT_ATTEMPTS;
        uint8_t status_ = STATE::am_choking | STATE::peer_choking;

        ba::ip::tcp::resolver::iterator endpoint_it_;
        ba::ip::tcp::socket socket_;
        ba::ip::tcp::resolver resolver_;

        ba::deadline_timer timeout_;
        bool is_disconnected = false;

    public:
        template<typename Function>
        void send_message(std::string const &msg, Function &&callback) {
            auto self = Get();

            post(socket_.get_executor(), [this, self, msg, callback]() {
                bool write_in_progress = !message_queue_.empty();
                auto new_msgs = bittorrent::GetMessagesQueue(msg);
                message_queue_.insert(message_queue_.end(), new_msgs.begin(), new_msgs.end());
                if (!write_in_progress) {
                    do_write(std::string(reinterpret_cast<const char*>(message_queue_.front().data()),
                                         message_queue_.front().length()),
                             [this, self, callback](){
                                 message_queue_.pop_front();
                                 if (!message_queue_.empty()) {
                                     do_write(callback);
                                 } else {
                                     callback();
                                 }
                             });
                }
            });
        }

        // TODO проверять что callback обязательно принимает строку
        template<typename Function>
        void read_message(Function &&callback) {
            auto self = Get();

            post(socket_.get_executor(), [this, self, callback]() {

            });
        }

    private:
        template<typename Function>
        void do_write(std::string binstring, Function &&callback) {
            auto self = Get();

            timeout_.async_wait([this, self](boost::system::error_code const &ec) {
                if (!ec) {
                    LOG(GetStrIP(), " : ", "deadline timer from send bin string");
                    deadline(); // TODO что-то другое мб
                }
            });
            ba::async_write(socket_,
                            ba::buffer(binstring),
                            [this, self, callback](boost::system::error_code ec, std::size_t /*length*/) {
                                if (!ec) {
                                    callback();
                                } else {
                                    socket_.close();
                                }
                            });
            timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
        }

        template<typename Function>
        void do_read(Function &&callback) {

        }
    };
}// namespace network

#endif//CPPTORRENT_PEERCLIENT_H
