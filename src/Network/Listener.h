#ifndef CPPTORRENT_LISTENER_H
#define CPPTORRENT_LISTENER_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/regex.hpp>

#include <unordered_map>
#include <memory>
#include <utility>
#include <string>

#include "Torrent.h"
#include "NetExceptions.h"
#include "Peer.h"
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

        static const inline size_t max_port = 6889;
        size_t port = 6880; // TODO config from console!

        ba::ip::tcp::acceptor acceptor_;
        ba::ip::tcp::socket socket_;


    };
}


#endif //CPPTORRENT_LISTENER_H
