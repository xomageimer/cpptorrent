#include "Kbucket.h"

#include "RouteTable.h"
#include "Node.h"

using namespace network;

dht::Kbucket::Kbucket(const dht::RouteTable &rt) : table_(rt), last_changed_(boost::posix_time::microsec_clock::local_time()) {}

dht::Kbucket::Kbucket(const dht::Kbucket &cpy) : Kbucket(cpy.table_) {
    nodes_ = cpy.nodes_;
}

dht::Kbucket &dht::Kbucket::operator=(const dht::Kbucket &cpy) {
    std::unique_lock lock(cpy.mut_);
    nodes_ = cpy.nodes_;
    update_time();
    return *this;
}

dht::Kbucket::Kbucket(dht::Kbucket &&mv) noexcept : Kbucket(mv.table_) {
    nodes_ = std::move(mv.nodes_);
}

dht::Kbucket &dht::Kbucket::operator=(dht::Kbucket &&mv) noexcept {
    if (this != &mv) {
        std::unique_lock lock(mv.mut_);
        nodes_ = std::move(mv.nodes_);
        update_time();
    }
    return *this;
}

bool dht::Kbucket::AddNode(NodeType n) {
    std::unique_lock lock(mut_);
    if (nodes_.size() < dht_constants::KBuckets_deep ||
        update_nodes() <
            dht_constants::KBuckets_deep) { // If there is room, insert; Otherwise we try to kick out those who died and put in their place
        nodes_.push_back(n);
        update_time();
        return true;
    }
    return false;
}

void dht::Kbucket::ResetNode(NodeType n) {
    std::unique_lock lock(mut_);
    auto it = find_node(*n);
    if (it != nodes_.end()) *it = std::move(n);
    update_time();
}

size_t dht::Kbucket::Size() const {
    std::shared_lock lock(mut_);
    return nodes_.size();
}

bool dht::Kbucket::Exist(const dht::NodeInfo &ni) const {
    std::shared_lock lock(mut_);
    auto it = find_node(ni);
    return it != nodes_.end() && (*it)->IsAlive();
}

boost::posix_time::ptime dht::Kbucket::GetLastChangedTime() const {
    std::shared_lock lock(mut_);
    return last_changed_;
}

std::optional<dht::Kbucket::NodeType> dht::Kbucket::GetNode(const dht::NodeInfo &ni) {
    std::shared_lock lock(mut_);
    auto it = find_node(ni);
    if (it == nodes_.end()) return std::nullopt;
    return *it;
}

dht::Kbucket::NodeType dht::Kbucket::Get(size_t idx) const {
    std::shared_lock lock(mut_);
    if (idx >= nodes_.size())
        throw std::logic_error("requested id " + std::to_string(idx) + " bigger than size(" + std::to_string(nodes_.size()) + ")");

    auto it = std::next(nodes_.begin(), idx);
    return *it;
}

void dht::Kbucket::Kick(const dht::NodeInfo &ni) {
    std::unique_lock lock(mut_);
    auto it = find_node(ni);
    if (it != nodes_.end()) nodes_.erase(it);
    update_time();
}

dht::Kbucket dht::Kbucket::SplitTheBucket() {
    std::unique_lock lock(mut_);

    if (nodes_.empty()) throw std::logic_error("trying to split empty k-bucket!");

    dht::Kbucket new_bucket{table_};
    auto master_id = table_.GetMasterInfo().id;
    auto max_el = *std::max_element(nodes_.begin(), nodes_.end(),
        [master_id](const auto &n1, const auto &n2) { return BucketDistance(n1->id, master_id) < BucketDistance(n2->id, master_id); });
    auto max_id = max_el->id;
    std::copy_if(nodes_.begin(), nodes_.end(), std::back_inserter(new_bucket.nodes_),
        [master_id, max_id](const auto &n) { return BucketDistance(n->id, master_id) != BucketDistance(max_id, master_id); });
    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
                     [master_id, max_id](const auto &n) { return BucketDistance(n->id, master_id) != BucketDistance(max_id, master_id); }),
        nodes_.end());
    return std::move(new_bucket);
}

size_t dht::Kbucket::UpdateNodes() {
    std::unique_lock lock(mut_);
    return update_nodes();
}

dht::Kbucket::IterType dht::Kbucket::find_node(const dht::NodeInfo &ni) {
    return std::find_if(nodes_.begin(), nodes_.end(), [&ni](const auto &n) { return n->id == ni.id; });
}

dht::Kbucket::ConstIterType dht::Kbucket::find_node(const dht::NodeInfo &ni) const {
    return std::find_if(nodes_.cbegin(), nodes_.cend(), [&ni](const auto &n) { return n->id == ni.id; });
}

void dht::Kbucket::update_time() {
    last_changed_ = boost::posix_time::microsec_clock::local_time();
}

size_t dht::Kbucket::update_nodes() {
    auto it = nodes_.begin();
    for (; it != nodes_.end();) {
        auto &node = *it;
        if (!node->IsAlive()) {
            it = nodes_.erase(it);
        } else {
            (*it++)->Ping([this, iter = it] {
                std::unique_lock lock(mut_);
                nodes_.erase(iter);
            });
        }
    }
    update_time();
    return nodes_.size();
}

size_t dht::BucketDistance(const dht::GUID &n1, const dht::GUID &n2) {
    assert(n1.Size() == n2.Size() && n1.Size() == dht_constants::SHA1_SIZE_BITS);
    size_t bit_pos = dht_constants::SHA1_SIZE_BITS - 1;
    for (size_t i = 0; i != dht_constants::SHA1_SIZE_BITS; i++, bit_pos--) {
        assert(i >= 0);
        uint8_t t = n1.Test(i) ^ n2.Test(i);
        if (t != 0) break;
    }
    return bit_pos;
}