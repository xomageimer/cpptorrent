#ifndef CPPTORRENT_ROUTETABLE_H
#define CPPTORRENT_ROUTETABLE_H

#include <boost/asio.hpp>

#include <utility>

#include "DHT/NodeClient.h"
#include "Kbucket.h"

namespace dht {
    struct RouteTable {
    public:
        using NodeType = Kbucket::NodeType;
        using RouteTableType = std::vector<Kbucket>;

        explicit RouteTable(dht::Node & master_ni, boost::asio::io_service & serv) : master_(master_ni), service_(serv) { buckets_.reserve(dht_constants::SHA1_SIZE_BITS); }

        void InsertNode(NodeType node);

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
