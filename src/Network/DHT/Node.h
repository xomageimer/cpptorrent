#ifndef CPPTORRENT_NODE_H
#define CPPTORRENT_NODE_H

#include "random_generator.h"
#include "auxiliary.h"
#include "constants.h"
#include "bitfield.h"
#include "Primitives/Socket.h"

namespace network {
    using GUID = Bitfield;

    enum class status {
        DISABLED,
        ENABLED
    };

    struct NodeInfo {
        NodeInfo();

        explicit NodeInfo(uint32_t ip, uint32_t port);

        GUID id{dht_constants::SHA1_SIZE_BITS};
        uint32_t ip{};
        uint16_t port{};

        size_t raznica = 0;
        // boost::asio::ip::udp::endpoint endpoint{boost::asio::ip::address::from_string("127.0.0.1"), 2889};
        // TODO тут мб надо еще и данные пользователя!
    };

    struct Node : public NodeInfo, public network::UDPSocket {
    public:
        using OnFailedCallback = std::function<void()>;

        Node(uint32_t ip, uint32_t port, const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor)
            : NodeInfo(ip, port), UDPSocket(executor) {}

        void Ping(OnFailedCallback on_failed);

        [[nodiscard]] bool IsAlive() const;

    private:
        status status_ = status::ENABLED;
    };
} // namespace network::dht

#endif // CPPTORRENT_NODE_H
