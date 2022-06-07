#include "DHT/RouteTable.h"
#include "DHT/Kbucket.h"

#include "auxiliary.h"
#include "logger.h"

#include <fstream>

using namespace std;

int main() {
    size_t n = 1000;
    boost::asio::io_service service;

    using namespace dht;

    std::vector<dht::MasterNode> nodes;
    nodes.reserve(n);
    for (size_t i = 0; i < n; i++) {
        nodes.emplace_back(service);
    }

    for (size_t j = 0; j < n; j++) {
        auto &master = nodes[j];
        for (size_t i = 0; i < n; i++) {
            if (i == j) continue;
            std::shared_ptr<network::NodeClient> node =
                std::make_shared<network::NodeClient>(random_generator::Random().GetNumber<uint32_t>(),
                    random_generator::Random().GetNumber<uint16_t>(), master, boost::asio::make_strand(service));
            node->ip = nodes[i].ip;
            node->port = nodes[i].port;
            node->id = nodes[i].id;
            node->id_sha1 = nodes[i].id_sha1;

            auto weight = XORDistance(master.id, node->id);
            size_t pos = 0;
            for (size_t p = 0; p < weight.Size(); p++) {
                if (weight.Test(p)) {
                    pos = p;
                    break;
                }
            }
            weight.Resize(std::max(pos + 1, (size_t)32));
            std::vector<uint8_t> cast = weight.GetCast();
            node->weight = bytesToInt64(cast.data(), weight.Size() / 8);

            master.TryToInsertNode(node);
        }
    }

    std::fstream file("nodes.log", std::ios::binary | std::ios_base::out | std::ios_base::trunc);
    for (auto & master : nodes) {
        std::reverse(master.route_table_->buckets_.begin(), master.route_table_->buckets_.end());
        size_t i = 5;
        int a = 2;
        for (auto & bucket : master.route_table_->buckets_){
            if (a % 2 == 0) {
                for (auto &node : bucket.nodes_) {
                    file << master.id.bits_ << " " << node->id.bits_ << " " << i << '\n';
                }
                i += 5;
                a = random_generator::Random().GetNumber<uint16_t>();
            }
        }
    }

    return EXIT_SUCCESS;
}