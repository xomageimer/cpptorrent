#include "Node.h"

using namespace network;

network::NodeInfo::NodeInfo(uint32_t arg_ip, uint32_t arg_port) : NodeInfo() {
    ip = arg_ip;
    port = arg_port;
}

network::NodeInfo::NodeInfo() {
    std::string str;
    for (size_t i = 0; i < dht_constants::key_size; ++i) {
        str.push_back(random_generator::Random().GetNumber<char>());
    }
    auto sha1 = GetSHA1(str);
    id = Bitfield(reinterpret_cast<const uint8_t *>(sha1.data()), sha1.size());
    assert(id.Size() == dht_constants::SHA1_SIZE_BITS);
}

void network::Node::Ping(network::Node::OnFailedCallback on_failed) {
    // TODO если не вышло, то делаем on_failed, а ѕќ—Ћ≈ него помечаем status как DISABLED
}

[[nodiscard]] bool network::Node::IsAlive() const {
    return status_ == status::ENABLED;
}