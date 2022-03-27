#include "Bitfield.h"

Bitfield::Bitfield(const std::vector<uint8_t> &from_bytes) {
    bits_.resize(from_bytes.size() * bittorrent_constants::byte_size);
    for (uint8_t number : from_bytes) {
        auto bit_it = bits_.begin();
        while (number) {
            *bit_it = number % 2;
            bit_it++;
            number /= 2;
        }
    }
}

std::vector<uint8_t> Bitfield::GetCast() const
{
    std::vector<uint8_t> cast {};
    uint8_t i;
    auto ptr = bits_.begin();
    while (ptr != bits_.end()) {
        cast.push_back(std::accumulate(ptr, ptr + std::min(bittorrent_constants::byte_size, static_cast<size_t>(std::distance(ptr, bits_.end()))) , 0, [](int x, int y) { return (x << 1) + y; }));
        ptr += std::min(bittorrent_constants::byte_size, static_cast<size_t>(std::distance(ptr, bits_.end())));
    }
    return std::move(cast);
}
