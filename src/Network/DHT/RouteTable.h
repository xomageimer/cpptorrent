#ifndef CPPTORRENT_ROUTETABLE_H
#define CPPTORRENT_ROUTETABLE_H

#include <utility>

#include "Node.h"
#include "Kbucket.h"

namespace network::dht {
    struct RouteTable {
    public:
        using NodeType = Kbucket::NodeType;
        using RouteTableType = std::vector<Kbucket>;

        explicit RouteTable(NodeInfo master_info) : master_(std::move(master_info)) { buckets_.reserve(dht_constants::SHA1_SIZE_BITS); }

        void InsertNode(NodeType node);

        size_t FindBucket(NodeInfo const &ni);

        [[nodiscard]] const NodeInfo & GetMasterInfo() const;

    private:
        bool TryToSplit();

        NodeInfo master_;

        RouteTableType buckets_;
    };
} // namespace network::dht

#endif // CPPTORRENT_ROUTETABLE_H
