#ifndef CPPTORRENT_NODECLIENT_H
#define CPPTORRENT_NODECLIENT_H

#include <filesystem>

#include "Node.h"
#include "Primitives/Socket.h"

#include "bencode_lib.h"

#include "random_generator.h"
#include "auxiliary.h"
#include "constants.h"
#include "bitfield.h"

namespace network {

    enum class status {
        DISABLED,
        ENABLED
    };

    struct NodeClient : public dht::Node, public network::UDPSocket {
    public:
        using OnFailedCallback = std::function<void()>;

        NodeClient(uint32_t ip, uint16_t port, dht::Node & master, const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor);

        void Connect();

        void Ping(OnFailedCallback on_failed);

        [[nodiscard]] bool IsAlive() const;

    private:
        void get_dht_query();

        dht::Node & master_node;

        mutable std::mutex status_mut_;
        status status_ = status::ENABLED;
    };
} // namespace network::dht

#endif // CPPTORRENT_NODECLIENT_H
