#include "RouteTable.h"

using namespace network;

void dht::RouteTable::InsertNode(NodeType node) {
    auto bucket_id = FindBucket(*node);
    node->raznica = dht_constants::SHA1_SIZE_BITS - 1 - BucketDistance(master_.id, node->id);
    auto & bucket = buckets_[bucket_id];
    if (!bucket.AddNode(node) && bucket_id == buckets_.size() - 1) {
        if (TryToSplit()) {
            bucket.AddNode(node);
        }
    }
}

size_t dht::RouteTable::FindBucket(NodeInfo const &ni) {
    size_t num_buckets = buckets_.size();
    if (num_buckets == 0) {
        buckets_.emplace_back(Kbucket{*this});
        ++num_buckets;
    }

    size_t bucket_index = std::min(dht_constants::SHA1_SIZE_BITS - 1 - BucketDistance(master_.id, ni.id), num_buckets - 1);
    return bucket_index;
}

bool dht::RouteTable::TryToSplit() {
    if (buckets_.size() < dht_constants::SHA1_SIZE_BITS - 1 && !buckets_.empty() && buckets_.back().Size() > 0) {
        buckets_.push_back(buckets_.back().SplitTheBucket());
        std::sort(buckets_.begin(), std::prev(buckets_.end()), [this](const auto & b1, const auto & b2){
            return (dht_constants::SHA1_SIZE_BITS - 1 - BucketDistance(master_.id, b1.Get(0)->id)) <
                   (dht_constants::SHA1_SIZE_BITS - 1 - BucketDistance(master_.id, b2.Get(0)->id));
        });
        return true;
    }
    return false;
}

const dht::NodeInfo &dht::RouteTable::GetMasterInfo() const {
    return master_;
}