#include "Bitfield.h"

#include "auxiliary.h"

bittorrent::Bitfield::Bitfield(const std::vector<uint8_t> &from_bytes) {
    bits_.reserve(from_bytes.size() * bittorrent_constants::byte_size);
    for (uint8_t number : from_bytes) {
        bits_.append(ReverseByte(number));
    }
}

bittorrent::Bitfield::Bitfield(const uint8_t *data, size_t size) {
    bits_.reserve(size * bittorrent_constants::byte_size);
    for (size_t i = 0; i < size; i++) {
        bits_.append(ReverseByte(data[i]));
    }
}

std::vector<uint8_t> bittorrent::Bitfield::GetCast() const {
    std::vector<uint8_t> cast{};
    boost::to_block_range(bits_, std::back_inserter(cast));
    std::for_each(cast.begin(), cast.end(), [](uint8_t &value) { value = ReverseByte(value); });
    return std::move(cast);
}

void bittorrent::Bitfield::Resize(size_t new_size) {
    bits_.resize(new_size);
}

bittorrent::Bitfield bittorrent::GetMismatchedBitfield(
    const bittorrent::Bitfield &main_bitfield, const bittorrent::Bitfield &secondary_bitfield) {
    bittorrent::Bitfield new_bitfield {main_bitfield.Size()};
    new_bitfield.bits_ = ~main_bitfield.bits_ & secondary_bitfield.bits_;
    return new_bitfield;
}
