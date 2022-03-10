#include "PeerListener.h"

PeerListener::PeerListener(const std::shared_ptr<bittorrent::MasterPeer> &master_peer,
                           const boost::asio::strand<boost::asio::io_service::executor_type> &executor,
                           ba::ip::tcp::endpoint endpoint) : master_peer_(*master_peer), socket_(executor), acceptor_(executor, endpoint) {
    do_accept();
}

PeerListener::~PeerListener() {
    socket_.close();
    acceptor_.close();
}

void PeerListener::do_accept() {
    acceptor_.async_accept(socket_,
                           [this](boost::system::error_code ec){
       if (!ec) {

       }
       do_accept();
    });
}
