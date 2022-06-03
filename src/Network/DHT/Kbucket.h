#ifndef CPPTORRENT_KBUCKET_H
#define CPPTORRENT_KBUCKET_H

#include <shared_mutex>
#include <memory>
#include <list>
#include <optional>

#include <boost/asio.hpp>

#include "constants.h"
#include "bitfield.h"

namespace network::dht {
    struct RouteTable;
    struct Node;
    struct NodeInfo;
    using GUID = Bitfield;

    struct Kbucket {
    public:
        using NodeType = std::shared_ptr<dht::Node>;
        using BucketType = std::list<NodeType>;
        using ConstIterType = BucketType::const_iterator;
        using IterType = BucketType::iterator;

        explicit Kbucket(RouteTable const & rt);

        Kbucket(const Kbucket &);

        Kbucket &operator=(const Kbucket &);

        Kbucket(Kbucket &&) noexcept;

        Kbucket &operator=(Kbucket &&) noexcept;

        size_t UpdateNodes();

        bool AddNode(NodeType n);

        void ResetNode(NodeType n);

        [[nodiscard]] bool Exist(const NodeInfo &ni) const;

        [[nodiscard]] boost::posix_time::ptime GetLastChangedTime() const;

        [[nodiscard]] std::optional<NodeType> GetNode(const NodeInfo &ni);

        [[nodiscard]] NodeType Get(size_t idx) const;

        [[nodiscard]] size_t Size() const;

        void Kick(const NodeInfo &ni);

        Kbucket SplitTheBucket();

    private:
        size_t update_nodes();

        void update_time();

        [[nodiscard]] IterType find_node(const NodeInfo &id);

        [[nodiscard]] ConstIterType find_node(const NodeInfo &id) const;

        const RouteTable & table_;

        mutable std::shared_mutex mut_;

        BucketType nodes_;

        boost::posix_time::ptime last_changed_;
    };

    size_t BucketDistance(const GUID &n1, const GUID &n2);
} // namespace network::dht

#endif // CPPTORRENT_KBUCKET_H
