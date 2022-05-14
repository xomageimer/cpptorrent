#include "Bitfield.h"

#include "auxiliary.h"

bittorrent::Bitfield::Bitfield(const std::vector<uint8_t> &from_bytes) {
    bits_.clear();
    bits_.reserve(from_bytes.size() * bittorrent_constants::byte_size);
    for (uint8_t number : from_bytes) {
        bits_.append(ReverseByte(number));
    }
}

std::vector<uint8_t> bittorrent::Bitfield::GetCast() const {
    std::vector<uint8_t> cast{};
    boost::to_block_range(bits_, std::back_inserter(cast));
    std::for_each(cast.begin(), cast.end(), [](uint8_t & value) { value = ReverseByte(value);});
    return std::move(cast);
}

void bittorrent::Bitfield::Resize(size_t new_size) {
    bits_.resize(new_size);
}
