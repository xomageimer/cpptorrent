#ifndef CPPTORRENT_NODE_H
#define CPPTORRENT_NODE_H

#include "random_generator.h"
#include "auxiliary.h"
#include "constants.h"
#include "bitfield.h"
#include "Primitives/Socket.h"

namespace network::dht {
    using GUID = Bitfield;
    enum class status {
        DISABLED,
        ENABLED
    };
    struct NodeInfo {
        explicit NodeInfo(uint32_t ip, uint32_t port);

        GUID id{dht_constants::SHA1_SIZE_BITS};
        uint32_t ip{};
        uint16_t port{};

        size_t raznica = 0;
        // boost::asio::ip::udp::endpoint endpoint{boost::asio::ip::address::from_string("127.0.0.1"), 2889};
        // TODO тут мб надо еще и данные пользовател€!
    };

    struct Node : public NodeInfo, public network::UDPSocket {
    public:
        Node(uint32_t ip, uint32_t port, const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor)
            : NodeInfo(ip, port), UDPSocket(executor) {}

        template <typename Handler> void Ping(Handler && on_failed) {

            // TODO если не вышло, то делаем on_failed, а ѕќ—Ћ≈ него помечаем status как DISABLED
        }

        [[nodiscard]] bool IsAlive() const;

    private:
        status status_ = status::ENABLED;
    };
} // namespace netwrok::dht

#endif // CPPTORRENT_NODE_H
