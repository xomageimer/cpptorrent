#ifndef CPPTORRENT_NODE_H
#define CPPTORRENT_NODE_H

#include <filesystem>

#include "Primitives/Socket.h"

#include "bencode_lib.h"
#include "random_generator.h"
#include "auxiliary.h"
#include "constants.h"
#include "bitfield.h"

namespace network {
    struct Master;
    using GUID = Bitfield;

    enum class status {
        DISABLED,
        ENABLED
    };

    struct NodeInfo {
        NodeInfo();

        explicit NodeInfo(uint32_t ip, uint16_t port);

        GUID id{dht_constants::SHA1_SIZE_BITS};
        uint32_t ip{};
        uint16_t port{};

        std::filesystem::path cash_path {std::filesystem::current_path()};



        size_t raznica = 0;
        // boost::asio::ip::udp::endpoint endpoint{boost::asio::ip::address::from_string("127.0.0.1"), 2889};
        // TODO тут мб надо еще и данные пользователя!
    };

    struct Node : public NodeInfo, public network::UDPSocket {
    public:
        using OnFailedCallback = std::function<void()>;

        Node(uint32_t ip, uint16_t port, Master & master, const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor);

        void Connect();

        void Ping(OnFailedCallback on_failed);

        [[nodiscard]] bool IsAlive() const;

    private:
        void get_dht_query();

        Master & master_node;

        std::mutex status_mut_;
        status status_ = status::ENABLED;
    };
} // namespace network::dht

#endif // CPPTORRENT_NODE_H
