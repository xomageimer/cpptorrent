#include "Node.h"

#include "Primitives/Message.h"

using namespace network;

network::NodeInfo::NodeInfo() {
    std::string str;
    for (size_t i = 0; i < dht_constants::key_size; ++i) {
        str.push_back(random_generator::Random().GetNumber<char>());
    }
    auto sha1 = GetSHA1(str);
    id = Bitfield(reinterpret_cast<const uint8_t *>(sha1.data()), sha1.size());
    assert(id.Size() == dht_constants::SHA1_SIZE_BITS);
}

network::NodeInfo::NodeInfo(uint32_t arg_ip, uint16_t arg_port) : NodeInfo() {
    ip = arg_ip;
    port = arg_port;
}

Node::Node(uint32_t ip, uint16_t port, Master &master, const asio::strand<boost::asio::io_service::executor_type> &executor)
    : NodeInfo(ip, port), master_node(master), UDPSocket(executor) {}

void Node::Connect() {
    UDPSocket::Connect(IpToStr(ip), std::to_string(port), []{}, [](boost::system::error_code ec){});
    Await(bittorrent_constants::connection_waiting_time);
}

void network::Node::Ping(network::Node::OnFailedCallback on_failed) {
    // TODO если не вышло, то делаем on_failed, а ѕќ—Ћ≈ него помечаем status как DISABLED
}

[[nodiscard]] bool network::Node::IsAlive() const {
    std::lock_guard lock(status_mut_);
    return status_ == status::ENABLED;
}

void Node::get_dht_query() {
    Read(
        bittorrent_constants::MTU,
        [this, self = shared_from(this)](const bittorrent::ReceivingMessage &data) {


            get_dht_query();
        },
        [this, self = shared_from(this)](boost::system::error_code ec) {
            std::lock_guard lock(status_mut_);
            status_ = status::DISABLED;
        });
}