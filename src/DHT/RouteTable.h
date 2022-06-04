#ifndef CPPTORRENT_ROUTETABLE_H
#define CPPTORRENT_ROUTETABLE_H

#include <boost/asio.hpp>

#include <utility>

#include "Node.h"
#include "Network/DHT/NodeClient.h"
#include "Kbucket.h"

namespace dht {
    struct RouteTable {
    public:
        using NodeType = dht::Kbucket::NodeType;
        using RouteTableType = std::vector<Kbucket>;

        RouteTable(dht::Node & master_ni, boost::asio::io_service & serv);

        void InsertNode(NodeType node);

        void KickNode(dht::Node const & node);

        [[nodiscard]] std::vector<NodeType> FindNearestList(GUID const & hash) const;

        size_t FindBucket(GUID const & id);

        size_t FindBucket(dht::Node const &ni);

        [[nodiscard]] const dht::Node & GetMasterInfo() const;

        [[nodiscard]] boost::asio::io_service & GetService() const { return service_; }

    private:
        bool try_to_split();

        dht::Node & master_;

        boost::asio::io_service & service_;

        RouteTableType buckets_;
    };
} // namespace network::dht

#endif // CPPTORRENT_ROUTETABLE_H
