#ifndef CPPTORRENT_PEERLISTENER_H
#define CPPTORRENT_PEERLISTENER_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/regex.hpp>

#include <memory>
#include <utility>
#include <string>

#include "NetExceptions.h"
#include "Peer.h"
#include "logger.h"

namespace ba = boost::asio;

struct PeerListener {
public:
    PeerListener(std::shared_ptr<bittorrent::MasterPeer> const & master_peer, const boost::asio::strand<typename boost::asio::io_service::executor_type>& executor, ba::ip::tcp::endpoint endpoint);
    ~PeerListener();
private:
    void do_accept();

    bittorrent::MasterPeer & master_peer_;

    ba::ip::tcp::acceptor acceptor_;
    ba::ip::tcp::socket socket_;
};


#endif //CPPTORRENT_PEERLISTENER_H
