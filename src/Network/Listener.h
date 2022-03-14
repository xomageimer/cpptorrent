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

#include "Constants.h"

#include "NetExceptions.h"
#include "Peer.h"
#include "Torrent.h"
#include "logger.h"

namespace ba = boost::asio;

namespace network {
    struct Listener {
    public:
        explicit Listener(const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor);
        ~Listener();

        [[nodiscard]] size_t GetPort() const { return port; }

    private:
        void get_port();
        void do_accept();

        std::unordered_map<std::string, std::shared_ptr<bittorrent::Torrent>> torrents;

        size_t port = bittorrent_constants::begin_port;

        ba::ip::tcp::acceptor acceptor_;
        ba::ip::tcp::socket socket_;
    };
}// namespace network


#endif//CPPTORRENT_LISTENER_H
