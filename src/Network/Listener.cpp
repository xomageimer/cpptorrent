#include "Listener.h"

network::Listener::Listener(const boost::asio::strand<boost::asio::io_service::executor_type> &executor) : socket_(executor), acceptor_(executor) {
    get_port();
    do_accept();
}

void network::Listener::get_port() {
    boost::system::error_code ec;
    do {
        if (port == bittorrent_constants::last_port)
            throw BadConnect("can't get free ports, try again later!");

        port++;
        acceptor_.open(ba::ip::tcp::v4(), ec) ||
                acceptor_.bind({ba::ip::tcp::v4(), static_cast<unsigned short>(port)}, ec);
    } while (ec == ba::error::address_in_use);
    acceptor_.listen();
}


network::Listener::~Listener() {
    socket_.close();
    acceptor_.close();
}

void network::Listener::do_accept() {
    LOG ("Listener" " : ", __FUNCTION__);

//    acceptor_.async_accept(socket_,
//                           [this](boost::system::error_code ec) {
//                               if (!ec) {
//                               }
//                               do_accept();
//                           });
}