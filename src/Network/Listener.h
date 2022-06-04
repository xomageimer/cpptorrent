#ifndef CPPTORRENT_LISTENER_H
#define CPPTORRENT_LISTENER_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "constants.h"

#include "Primitives/Socket.h"
#include "NetExceptions.h"

#include "Torrent.h"
#include "DHT/Node.h"
#include "logger.h"

namespace ba = boost::asio;

namespace network {
    struct participant : public std::enable_shared_from_this<participant> {
    public:
        explicit participant(Listener &listener, ba::ip::tcp::socket socket)
            : listener_(listener), socket_(std::move(socket)), timeout_(socket_.get_executor()) {}

        // TODO ���������� �� ���� Get()...
        auto Get() { return shared_from_this(); }

        void Verify();

    private:
        Listener &listener_;

        ba::ip::tcp::socket socket_;

        uint8_t buff[bittorrent_constants::handshake_length];

        ba::deadline_timer timeout_;
    };

    // TODO � DHT ����� ����� ������� ��������� �������� � ������� participant!
    struct Listener {
    public:
        explicit Listener(const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor, std::shared_ptr<dht::MasterNode> dht_entry = nullptr);

        ~Listener();

        [[nodiscard]] size_t GetPort() const { return port_; }

        void AddTorrent(const std::shared_ptr<bittorrent::Torrent> &new_torrent);

    private:
        void get_port();

        void do_accept();

        void dht_catcher();

        friend struct participant;

        std::unordered_map<std::string, std::shared_ptr<bittorrent::Torrent>> torrents_;

        std::shared_ptr<dht::MasterNode> dht_entry_;

        size_t port_ = bittorrent_constants::begin_port;

        ba::ip::tcp::acceptor acceptor_;

        ba::ip::tcp::socket socket_;

        ba::ip::udp::socket dht_socket_;

        boost::asio::streambuf buff_;
    };
} // namespace network

#endif // CPPTORRENT_LISTENER_H
