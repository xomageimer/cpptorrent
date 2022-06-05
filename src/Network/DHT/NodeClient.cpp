#include "NodeClient.h"

#include <utility>

#include "Network/Primitives/Message.h"

using namespace network;

NodeClient::NodeClient(UDPSocket::socket_type socket, dht::MasterNode &master)
    : dht::Node(IpToInt(socket.remote_endpoint().address().to_string()), socket.remote_endpoint().port()), master_node_(master),
      UDPSocket(std::move(socket)) {}

NodeClient::NodeClient(
    uint32_t ip, uint16_t port, dht::MasterNode &master, const asio::strand<boost::asio::io_service::executor_type> &executor)
    : dht::Node(ip, port), master_node_(master), UDPSocket(executor) {
}

void NodeClient::connect() {
    UDPSocket::Connect(
        IpToStr(ip), std::to_string(port), [] {}, [](boost::system::error_code ec) {});
    Await(bittorrent_constants::connection_waiting_time);
}

bittorrent::SendingMessage NodeClient::MakeMessage(bencode::Document doc) {
    std::stringstream stream;
    bencode::Serialize::MakeSerialize(doc.GetRoot(), stream);
    bittorrent::SendingMessage msg;
    msg.CopyFrom(stream);
    return std::move(msg);
}

void network::NodeClient::Ping(OnFailedCallback on_failed) {
    return;
    bencode::Node msg_bencode;

    msg_bencode["t"] = "aa";
    msg_bencode["y"] = "q";
    msg_bencode["q"] = "ping";

    bencode::Node data_part;
    data_part["id"] = master_node_.id_sha1;

    msg_bencode["a"] = std::move(data_part);

    Send(std::move(MakeMessage(bencode::Document{std::move(msg_bencode)})));
    Await(dht_constants::ping_wait_time, [this, on_failed] {
        on_failed();
        std::lock_guard lock(status_mut_);
        status_ = status::DISABLED;
    });
}

void NodeClient::SendQuery(bencode::Document doc, OnSuccessCalback on_success, OnFailedCallback on_failed) {
    Send(std::move(MakeMessage(std::move(doc))));
    Await(dht_constants::query_wait_time, [this, on_failed] {
        on_failed();
        std::lock_guard lock(status_mut_);
        status_ = status::DISABLED;
    });
}

[[nodiscard]] bool network::NodeClient::IsAlive() const {
    std::lock_guard lock(status_mut_);
    return status_ == status::ENABLED;
}

void NodeClient::get_dht_query() {
    Read(
        bittorrent_constants::MTU,
        [this, self = shared_from(this)](const bittorrent::ReceivingMessage &data) {
            if (!IsAlive()) return;

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
