#include "DHT/RouteTable.h"
#include "DHT/Kbucket.h"

#include "auxiliary.h"
#include "logger.h"

using namespace std;

int main() {
    boost::asio::io_service service;

    using namespace dht;

    network::NodeInfo master {123213, 2990};
    dht::RouteTable rt{service};

    for (size_t i = 0; i < 100; i++){
        std::shared_ptr<network::Node> node = std::make_shared<network::Node>(random_generator::Random().GetNumber<uint32_t>(), random_generator::Random().GetNumber<uint16_t>(), boost::asio::make_strand(service));
        rt.InsertNode(node);
    }

    return EXIT_SUCCESS;
}