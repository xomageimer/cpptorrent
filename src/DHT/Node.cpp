#include "Node.h"

#include "DHT/NodeClient.h"
#include "RouteTable.h"

using namespace dht;

dht::Node::Node() {
    std::string str;
    for (size_t i = 0; i < dht_constants::key_size; ++i) {
        str.push_back(random_generator::Random().GetNumber<char>());
    }
    auto sha1 = GetSHA1(str);
    id = Bitfield(reinterpret_cast<const uint8_t *>(sha1.data()), sha1.size());
    assert(id.Size() == dht_constants::SHA1_SIZE_BITS);
}

dht::Node::Node(uint32_t arg_ip, uint16_t arg_port) : Node() {
    ip = arg_ip;
    port = arg_port;
}

MasterNode::MasterNode(boost::asio::io_service &serv) : route_table_(std::make_shared<RouteTable>(*reinterpret_cast<dht::Node *>(this), serv)) {}
