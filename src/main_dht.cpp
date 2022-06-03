#include "DHT/RouteTable.h"
#include "DHT/Kbucket.h"

#include "auxiliary.h"
#include "logger.h"

using namespace std;

int main() {
    boost::asio::io_service service;

    using namespace network::dht;

    network::dht::NodeInfo master {123213, 2990};
    network::dht::RouteTable rt{master};

    for (size_t i = 0; i < 100; i++){
        std::shared_ptr<Node> node = std::make_shared<Node>(random_generator::Random().GetNumber<uint32_t>(), random_generator::Random().GetNumber<uint16_t>(), boost::asio::make_strand(service));
        rt.InsertNode(node);
    }

    size_t i = 0;
    for (auto stat : rt.Statistic()){
        std::cerr << i++ << " : " << stat << std::endl;
    }

    return EXIT_SUCCESS;
}