#ifndef CPPTORRENT_NODECLIENT_H
#define CPPTORRENT_NODECLIENT_H

#include <filesystem>

#include "Node.h"
#include "Network/Primitives/Socket.h"

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
        using OnSuccessCalback = std::function<void(bencode::Document doc)>;
        using OnFailedCallback = std::function<void()>;

        NodeClient(UDPSocket::socket_type socket, dht::MasterNode & master);

        NodeClient(uint32_t ip, uint16_t port, dht::MasterNode & master, const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor);

        void BlockingPing(OnFailedCallback on_failed);

        void SendQuery(bencode::Document doc, OnSuccessCalback on_success, OnFailedCallback on_failed);

        [[nodiscard]] bool IsAlive() const;

    private:
        void connect();

        void get_dht_query();

        dht::MasterNode & master_node_;

        mutable std::mutex status_mut_;
        status status_ = status::ENABLED;
    };
} // namespace network::dht

#endif // CPPTORRENT_NODECLIENT_H
