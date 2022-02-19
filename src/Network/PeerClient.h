#ifndef CPPTORRENT_PEERCLIENT_H
#define CPPTORRENT_PEERCLIENT_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/regex.hpp>

#include <memory>

#include "Peer.h"

namespace ba = boost::asio;

namespace network {
    struct PeerClient {
    public:

    private:
//        bittorrent::Torrent & torrent;
    };
}

#endif //CPPTORRENT_PEERCLIENT_H
