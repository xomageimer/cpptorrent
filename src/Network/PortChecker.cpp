#include "PortChecker.h"

// TODO проверить что это работает!
std::optional<size_t> network::PortChecker::operator()(size_t start_port, size_t max_port) {
    auto cur_port = start_port;
    do {
        ba::ip::tcp::endpoint ep(ba::ip::address::from_string("127.0.0.1"), cur_port);
        ba::ip::tcp::socket sock(ios);

        boost::system::error_code ec;
        sock.connect(ep, ec);
        if (!ec) break;

        cur_port++;
    } while (cur_port != max_port);

    if (cur_port == max_port)
        return std::nullopt;
    return cur_port;
}
