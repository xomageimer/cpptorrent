#ifndef CPPTORRENT_BITFIELD_H
#define CPPTORRENT_BITFIELD_H

#include <boost/dynamic_bitset.hpp>

#include "constants.h"
#include "auxiliary.h"

#include <algorithm>
#include <numeric>
#include <vector>

// TODO добавить способ получать самые раритетные имеющиеся pieces
struct Bitfield {
public:
    explicit Bitfield() : Bitfield(0){};

    explicit Bitfield(size_t size) : bits_(size, 0) {}

    explicit Bitfield(std::vector<uint8_t> const &from_cast);

    explicit Bitfield(const uint8_t *data, size_t size);

    Bitfield(const Bitfield &) = default;

    Bitfield(Bitfield &&) = default;

    Bitfield &operator=(const Bitfield &) = default;

    Bitfield &operator=(Bitfield &&) = default;

    void Resize(size_t new_size);

    [[nodiscard]] bool Test(size_t i) const { return bits_[i]; }

    void Set(size_t i) { bits_.set(i); }

    void Clear(size_t i) { bits_.reset(i); }

    void Toggle(size_t i) { bits_.set(i, !bits_[i]); }

    [[nodiscard]] size_t Size() const { return bits_.size(); }

    [[nodiscard]] size_t Popcount() const { return bits_.count(); }

    [[nodiscard]] std::vector<uint8_t> GetCast() const;

    friend Bitfield GetMismatchedBitfield(const Bitfield &main_bitfield, const Bitfield &secondary_bitfield);

    friend Bitfield XORDistance(const Bitfield &lhs, const Bitfield &rhs);

    friend bool operator>(const Bitfield &lhs, const Bitfield &rhs);

    friend bool operator<(const Bitfield &lhs, const Bitfield &rhs);

    friend bool operator==(const Bitfield &lhs, const Bitfield &rhs);

    friend bool operator!=(const Bitfield &lhs, const Bitfield &rhs);

//private:
    boost::dynamic_bitset<uint8_t> bits_;
};

Bitfield GetMismatchedBitfield(const Bitfield &main_bitfield, const Bitfield &secondary_bitfield);

Bitfield XORDistance(const Bitfield &lhs, const Bitfield &rhs);

#endif // CPPTORRENT_BITFIELD_H
