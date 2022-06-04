#include "NodeClient.h"

#include "Network/Primitives/Message.h"

using namespace network;

NodeClient::NodeClient(UDPSocket::socket_type socket, dht::MasterNode & master)
    : dht::Node(IpToInt(socket.remote_endpoint().address().to_string()), socket.remote_endpoint().port()), master_node_(master), UDPSocket(std::move(socket)) {}

NodeClient::NodeClient(
    uint32_t ip, uint16_t port, dht::MasterNode &master, const asio::strand<boost::asio::io_service::executor_type> &executor)
    : dht::Node(ip, port), master_node_(master), UDPSocket(executor) {
    connect();
}

void NodeClient::connect() {
    UDPSocket::Connect(
        IpToStr(ip), std::to_string(port), [] {}, [](boost::system::error_code ec) {});
    Await(bittorrent_constants::connection_waiting_time);
}

void network::NodeClient::BlockingPing(OnFailedCallback on_failed) {

//    Await(dht_constants::ping_wait_time);
//    // TODO делаем bencode запрос
//    boost::system::error_code ec;
//    std::string bencode_serialize;
//    auto endp = socket_.remote_endpoint();
//    socket_.send_to(boost::asio::buffer(bencode_serialize), endp, boost::asio::socket_base::message_peek, ec);
//    if (ec) {
//        on_failed();
//        status_ = status::DISABLED;
//        return;
//    }
//    socket_.receive_from(boost::asio::buffer(data, max_length), sender_endpoint);
    // TODO если не вышло, то делаем on_failed, а ѕќ—Ћ≈ него помечаем status как DISABLED
}

void NodeClient::SendQuery(bencode::Document doc, OnSuccessCalback on_success, OnFailedCallback on_failed) {}

[[nodiscard]] bool network::NodeClient::IsAlive() const {
    std::lock_guard lock(status_mut_);
    return status_ == status::ENABLED;
}

void NodeClient::get_dht_query() {
    Read(
        bittorrent_constants::MTU,
        [this, self = shared_from(this)](const bittorrent::ReceivingMessage &data) {
            std::shared_ptr<KRPCQuery> rpc_query;
            // TODO парсим bencode -> создаем KRPC
            master_node_.AddRPC(rpc_query);
            get_dht_query();
        },
        [this, self = shared_from(this)](boost::system::error_code ec) {
            std::lock_guard lock(status_mut_);
            status_ = status::DISABLED;
        });
}