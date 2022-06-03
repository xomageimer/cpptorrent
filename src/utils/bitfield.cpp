#include "bitfield.h"

#include "auxiliary.h"

Bitfield::Bitfield(const std::vector<uint8_t> &from_bytes) {
    bits_.reserve(from_bytes.size() * bittorrent_constants::byte_size);
    for (uint8_t number : from_bytes) {
        bits_.append(ReverseByte(number));
    }
}

Bitfield::Bitfield(const uint8_t *data, size_t size) {
    bits_.reserve(size * bittorrent_constants::byte_size);
    for (size_t i = 0; i < size; i++) {
        bits_.append(ReverseByte(data[i]));
    }
}

std::vector<uint8_t> Bitfield::GetCast() const {
    std::vector<uint8_t> cast{};
    boost::to_block_range(bits_, std::back_inserter(cast));
    std::for_each(cast.begin(), cast.end(), [](uint8_t &value) { value = ReverseByte(value); });
    return std::move(cast);
}

void Bitfield::Resize(size_t new_size) {
    bits_.resize(new_size);
}

Bitfield GetMismatchedBitfield(const Bitfield &main_bitfield, const Bitfield &secondary_bitfield) {
    Bitfield new_bitfield{main_bitfield.Size()};
    new_bitfield.bits_ = ~main_bitfield.bits_ & secondary_bitfield.bits_;
    return new_bitfield;
}

Bitfield XORDistance(const Bitfield &lhs, const Bitfield &rhs) {
    assert(lhs.Size() == rhs.Size() && lhs.Size() == dht_constants::SHA1_SIZE_BITS);

    Bitfield answ{dht_constants::SHA1_SIZE_BITS};
    answ.bits_ = lhs.bits_ ^ rhs.bits_;
    return std::move(answ);
}

bool operator>(const Bitfield &lhs, const Bitfield &rhs) {
    return lhs.bits_ > rhs.bits_;
}

bool operator==(const Bitfield &lhs, const Bitfield &rhs) {
    return lhs.bits_ == rhs.bits_;
}

bool operator<(const Bitfield &lhs, const Bitfield &rhs) {
    return lhs.bits_ < rhs.bits_;
}

bool operator!=(const Bitfield &lhs, const Bitfield &rhs) {
    return lhs.bits_ != rhs.bits_;
}
