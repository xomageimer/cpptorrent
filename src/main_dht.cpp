#include "DHT/RouteTable.h"
#include "DHT/Kbucket.h"

#include "auxiliary.h"
#include "logger.h"

using namespace std;

int main() {
    boost::asio::io_service service;

    using namespace dht;

    dht::MasterNode master {service};

    for (size_t i = 0; i < 100; i++){
        std::shared_ptr<network::NodeClient> node = std::make_shared<network::NodeClient>(random_generator::Random().GetNumber<uint32_t>(), random_generator::Random().GetNumber<uint16_t>(), master, boost::asio::make_strand(service));
        master.TryToInsertNode(node);
    }

    return EXIT_SUCCESS;
}