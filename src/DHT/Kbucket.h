#ifndef CPPTORRENT_KBUCKET_H
#define CPPTORRENT_KBUCKET_H

#include <shared_mutex>
#include <memory>
#include <list>
#include <optional>

#include "DHT/Node.h"

#include "constants.h"
#include "bitfield.h"

namespace dht {
    struct RouteTable;
    using GUID = Bitfield;

    struct Kbucket {
    public:
        using NodeType = std::shared_ptr<network::Node>;
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

        [[nodiscard]] bool Exist(const network::NodeInfo &ni) const;

        [[nodiscard]] boost::posix_time::ptime GetLastChangedTime() const;

        [[nodiscard]] std::optional<NodeType> GetNode(const network::NodeInfo &ni);

        [[nodiscard]] size_t Size() const;

        void Kick(const network::NodeInfo &ni);

        Kbucket SplitTheBucket(size_t by_id);

        struct Access{
            std::shared_lock<std::shared_mutex> lock;
            const BucketType & bucket_origin;
        };
        [[nodiscard]] Access GetOrigin() const;

    private:
        size_t update_nodes();

        void update_time();

        [[nodiscard]] IterType find_node(const network::NodeInfo &id);

        [[nodiscard]] ConstIterType find_node(const network::NodeInfo &id) const;

        const RouteTable & table_;

        mutable std::shared_mutex mut_;

        BucketType nodes_;

        boost::posix_time::ptime last_changed_;
    };

    size_t BucketDistance(const GUID &n1, const GUID &n2);
} // namespace network::dht

#endif // CPPTORRENT_KBUCKET_H
