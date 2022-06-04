#ifndef CPPTORRENT_KRPC_H
#define CPPTORRENT_KRPC_H

#include "bencode_lib.h"

namespace dht {
    struct RouteTable;
}

namespace network {
    struct KRPCQuery {
    public:
        explicit KRPCQuery(bencode::Document doc, dht::RouteTable &route_table);

        virtual void Call() = 0;

    protected:
        dht::RouteTable &route_table_;

        bencode::Document doc_;
    };

    struct PingQuery : public KRPCQuery {};

    struct FindNodeQuery : public KRPCQuery {};

    struct GetPeersQuery : public KRPCQuery {};

    struct AnnouncePeerQuery : public KRPCQuery {};
} // namespace network

#endif // CPPTORRENT_KRPC_H
