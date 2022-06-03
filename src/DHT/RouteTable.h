#ifndef CPPTORRENT_ROUTETABLE_H
#define CPPTORRENT_ROUTETABLE_H

#include <utility>

#include "DHT/Node.h"
#include "Kbucket.h"

namespace dht {
    struct RouteTable {
    public:
        using NodeType = Kbucket::NodeType;
        using RouteTableType = std::vector<Kbucket>;

        explicit RouteTable(network::NodeInfo master_info) : master_(std::move(master_info)) { buckets_.reserve(dht_constants::SHA1_SIZE_BITS); }

        void InsertNode(NodeType node);

        [[nodiscard]] std::vector<NodeType> FindNearestList(GUID const & hash) const;

        size_t FindBucket(GUID const & id);

        size_t FindBucket(network::NodeInfo const &ni);

        [[nodiscard]] const network::NodeInfo & GetMasterInfo() const;

    private:
        bool try_to_split();

        network::NodeInfo master_;

        RouteTableType buckets_;
    };
} // namespace network::dht

#endif // CPPTORRENT_ROUTETABLE_H
