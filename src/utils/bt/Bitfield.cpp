#include "Bitfield.h"

Bitfield::Bitfield(const std::vector<uint8_t> &from_bytes) {
    bits_.resize(from_bytes.size() * bittorrent_constants::byte_size);
    for (uint8_t number : from_bytes) {
        bits_.append(number);
    }
}

std::vector<uint8_t> Bitfield::GetCast() const
{
    std::vector<uint8_t> cast {};
    boost::to_block_range(bits_, std::back_inserter(cast));
    return std::move(cast);
}
