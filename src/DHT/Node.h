#ifndef CPPTORRENT_NODE_H
#define CPPTORRENT_NODE_H

#include <memory>

#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

#include "Network/DHT/KRPC.h"

#include "async_worker.h"
#include "constants.h"
#include "bitfield.h"

using json = nlohmann::json;

namespace network{
    struct NodeClient;
}

namespace dht {
    using GUID = Bitfield;
    struct Node {
        Node();

        explicit Node(uint32_t ip, uint16_t port);

        std::string id_sha1;
        GUID id{dht_constants::SHA1_SIZE_BITS};
        uint32_t ip{};
        uint16_t port{};

        std::filesystem::path cash_path{std::filesystem::current_path() / "cash"};

        size_t weight = 0;

        size_t raznica = 0;
        // boost::asio::ip::udp::endpoint endpoint{boost::asio::ip::address::from_string("127.0.0.1"), 2889};
        // TODO ��� �� ���� ��� � ������ ������������!
    };

    struct MasterNode : public dht::Node { // bootstrap structure for DHT
    public:
        using rpc_type = std::shared_ptr<network::KRPCQuery>;
        explicit MasterNode(boost::asio::io_service &serv, size_t thread_num = 1);

        MasterNode(MasterNode && mv) noexcept {
            id = mv.id;
            id_sha1 = mv.id_sha1;
            ip = mv.ip;
            port = mv.port;
            weight = mv.weight;
            raznica = mv.raznica;
            cash_path = mv.cash_path;
            route_table_ = std::move(mv.route_table_);
        }

        void AddRPC(rpc_type);

        void TryToInsertNode(std::shared_ptr<network::NodeClient> nc);

        void FindGiveaway(std::string const & info_hash) {}; // ����� �������

        void AnnounceMe(std::string const & info_hash) {};  // �������� ��� ����� � �������

        boost::asio::io_service & GetService();

//    private:
        friend struct RouteTable;
        std::shared_ptr<struct RouteTable> route_table_;
        AsyncWorker a_worker_;
    };
} // namespace dht

#endif // CPPTORRENT_NODE_H
