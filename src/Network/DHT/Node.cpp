#include "Node.h"

using namespace network;

dht::NodeInfo::NodeInfo(uint32_t ip, uint32_t port) : ip(ip), port(port) {
    std::string str;
    for (size_t i = 0; i < dht_constants::key_size; ++i) {
        str.push_back(random_generator::Random().GetNumber<char>());
    }
    auto sha1 = GetSHA1(str);
    id = Bitfield(reinterpret_cast<const uint8_t *>(sha1.data()), sha1.size());
    assert(id.Size() == dht_constants::SHA1_SIZE_BITS);
}

[[nodiscard]] bool dht::Node::IsAlive() const {
    return status_ == status::ENABLED;
}