#ifndef CPPTORRENT_NODE_H
#define CPPTORRENT_NODE_H

#include <memory>

#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

#include "constants.h"
#include "bitfield.h"

using json = nlohmann::json;

namespace dht {
    using GUID = Bitfield;
    struct Node {
        Node();

        explicit Node(uint32_t ip, uint16_t port);

        GUID id{dht_constants::SHA1_SIZE_BITS};
        uint32_t ip{};
        uint16_t port{};

        std::filesystem::path cash_path{std::filesystem::current_path()};

        size_t raznica = 0;
        // boost::asio::ip::udp::endpoint endpoint{boost::asio::ip::address::from_string("127.0.0.1"), 2889};
        // TODO тут мб надо еще и данные пользователя!
    };

    struct MasterNode : public Node { // bootstrap structure for DHT
    public:
        explicit MasterNode(boost::asio::io_service &serv);

    private:
        std::shared_ptr<struct RouteTable> route_table_;
    };
} // namespace dht

#endif // CPPTORRENT_NODE_H
