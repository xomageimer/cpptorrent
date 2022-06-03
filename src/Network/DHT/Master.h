#ifndef CPPTORRENT_MASTER_H
#define CPPTORRENT_MASTER_H

#include "Primitives/Socket.h"
#include "Node.h"
#include "../../DHT/RouteTable.h"

namespace network {
    struct Master : public NodeInfo, public UDPSocket {

    };
} // namespace network::dht

#endif // CPPTORRENT_MASTER_H
