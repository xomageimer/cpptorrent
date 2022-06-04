#ifndef CPPTORRENT_MASTER_H
#define CPPTORRENT_MASTER_H

#include <boost/asio.hpp>

#include "Node.h"
#include "RouteTable.h"

namespace network {
    struct Master : public network::NodeInfo {
    public:
        explicit Master(boost::asio::io_service & serv) : route_table_(*this, serv) {}
    private:
        dht::RouteTable route_table_;
    };
} // namespace network::dht

#endif // CPPTORRENT_MASTER_H
