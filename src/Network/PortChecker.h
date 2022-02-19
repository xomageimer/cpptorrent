#ifndef CPPTORRENT_PORTCHECKER_H
#define CPPTORRENT_PORTCHECKER_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/exception/all.hpp>

#include <optional>

namespace ba = boost::asio;

namespace network {
    struct PortChecker {
    public:
        explicit PortChecker(ba::io_service & io_s) : ios(io_s) {};
        std::optional<size_t> operator()(size_t start_port, size_t max_port);
    private:
        ba::io_service & ios;
    };
}

#endif //CPPTORRENT_PORTCHECKER_H
