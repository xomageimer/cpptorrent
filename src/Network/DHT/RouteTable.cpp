#include "RouteTable.h"

using namespace network;

void dht::RouteTable::InsertNode(NodeType node) {
    auto bucket_id = FindBucket(*node);
    auto &bucket = buckets_[bucket_id];
    node->raznica = bucket_id;
    if (!bucket.AddNode(node) && bucket_id == buckets_.size() - 1) {
        if (try_to_split()) {
            bucket.AddNode(node);
        }
    }
}

std::vector<dht::RouteTable::NodeType> dht::RouteTable::FindNearestList(const dht::GUID &hash) const {
    std::vector<NodeType> res;
    res.reserve(buckets_.size() * dht_constants::KBuckets_deep);

    for (auto &bucket : {buckets_.rbegin(), buckets_.rend()}) {
        auto origin = bucket->GetOrigin();
        for (auto &node : origin.bucket_origin) {
            res.push_back(node);
        }
    }
    std::sort(
        res.begin(), res.end(), [hash](const auto &n1, const auto &n2) { return XORDistance(hash, n1->id) < XORDistance(hash, n2->id); });
    if (res.size() > dht_constants::max_nearest_nodes) res.resize(dht_constants::max_nearest_nodes);

    assert(res.size() <= dht_constants::max_nearest_nodes);
    return std::move(res);
}

size_t dht::RouteTable::FindBucket(GUID const &id) {
    size_t num_buckets = buckets_.size();
    if (num_buckets == 0) {
        buckets_.emplace_back(Kbucket{*this});
        ++num_buckets;
    }

    size_t bucket_index = std::min(dht_constants::SHA1_SIZE_BITS - 1 - BucketDistance(master_.id, id), num_buckets - 1);
    return bucket_index;
}

size_t dht::RouteTable::FindBucket(NodeInfo const &ni) {
    return FindBucket(ni.id);
}

bool dht::RouteTable::try_to_split() {
    if (buckets_.size() < dht_constants::SHA1_SIZE_BITS - 1 && !buckets_.empty() && buckets_.back().Size() > 0) {
        buckets_.push_back(buckets_.back().SplitTheBucket(buckets_.size() - 1));
        return true;
    }
    return false;
}

const network::NodeInfo &dht::RouteTable::GetMasterInfo() const {
    return master_;
}