#ifndef CPPTORRENT_NETEXCEPTIONS_H
#define CPPTORRENT_NETEXCEPTIONS_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/exception/all.hpp>

namespace network {
    struct BadConnect : public boost::exception, public std::exception {
    private:
        std::string exception;
    public:
        explicit BadConnect(std::string arg) : exception(std::move(arg)) {};
        [[nodiscard]] const char *what() const noexcept override { return exception.data(); }
    };
}

#endif //CPPTORRENT_NETEXCEPTIONS_H
