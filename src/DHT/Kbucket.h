#ifndef CPPTORRENT_KBUCKET_H
#define CPPTORRENT_KBUCKET_H

#include <boost/asio.hpp>

#include <shared_mutex>
#include <memory>
#include <list>
#include <optional>

#include "Network/DHT/NodeClient.h"

#include "constants.h"
#include "bitfield.h"

namespace dht {
    struct RouteTable;
    using GUID = Bitfield;

    struct Kbucket {
    public:
        using NodeType = std::shared_ptr<network::NodeClient>;
        using BucketType = std::list<NodeType>;
        using ConstIterType = BucketType::const_iterator;
        using IterType = BucketType::iterator;

        explicit Kbucket(RouteTable const & rt);

        Kbucket(const Kbucket &);

        Kbucket &operator=(const Kbucket &);

        Kbucket(Kbucket &&) noexcept;

        Kbucket &operator=(Kbucket &&) noexcept;

        ~Kbucket();

        size_t UpdateNodes();

        bool AddNode(NodeType n);

        void ResetNode(NodeType n);

        [[nodiscard]] bool Exist(const Node &ni) const;

        [[nodiscard]] std::optional<NodeType> GetNode(const Node &ni);

        [[nodiscard]] size_t Size() const;

        void Kick(const Node &ni);

        Kbucket SplitTheBucket(size_t by_id);

        struct Access{
            std::shared_lock<std::shared_mutex> lock;
            const BucketType & bucket_origin;
        };
        [[nodiscard]] Access GetOrigin() const;

    private:
        size_t update_nodes();

        void update_time();

        [[nodiscard]] IterType find_node(const Node &id);

        [[nodiscard]] ConstIterType find_node(const Node &id) const;

        const RouteTable & table_;

        mutable std::shared_mutex mut_;

        BucketType nodes_;

        boost::asio::deadline_timer last_changed_timeout_;
    };

    size_t BucketDistance(const GUID &n1, const GUID &n2);
} // namespace network::dht

#endif // CPPTORRENT_KBUCKET_H