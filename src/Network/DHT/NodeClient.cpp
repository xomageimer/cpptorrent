#include "NodeClient.h"

#include "Primitives/Message.h"

using namespace network;

NodeClient::NodeClient(uint32_t ip, uint16_t port, dht::Node &master, const asio::strand<boost::asio::io_service::executor_type> &executor)
    : dht::Node(ip, port), master_node(master), UDPSocket(executor) {}

void NodeClient::Connect() {
    UDPSocket::Connect(IpToStr(ip), std::to_string(port), []{}, [](boost::system::error_code ec){});
    Await(bittorrent_constants::connection_waiting_time);
}

void network::NodeClient::Ping(network::NodeClient::OnFailedCallback on_failed) {
    // TODO если не вышло, то делаем on_failed, а ѕќ—Ћ≈ него помечаем status как DISABLED
}

[[nodiscard]] bool network::NodeClient::IsAlive() const {
    std::lock_guard lock(status_mut_);
    return status_ == status::ENABLED;
}

void NodeClient::get_dht_query() {
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