#ifndef CPPTORRENT_BITFIELD_H
#define CPPTORRENT_BITFIELD_H

#include <boost/dynamic_bitset.hpp>

#include "constants.h"
#include "auxiliary.h"

#include <algorithm>
#include <numeric>
#include <vector>

namespace bittorrent {
    struct Bitfield {
    public:
        explicit Bitfield() = default;

        explicit Bitfield(size_t size) : bits_(size, 0) {}

        explicit Bitfield(std::vector<uint8_t> const &from_cast);

        Bitfield(const Bitfield &) = default;

        void Resize(size_t new_size);

        bool Test(size_t i) { return bits_[i]; }

        void Set(size_t i) { bits_.set(i); }

        void Clear(size_t i) { bits_.reset(i); }

        void Toggle(size_t i) { bits_.set(i, !bits_[i]); }

        [[nodiscard]] size_t Size() const { return bits_.size(); }

        [[nodiscard]] size_t Popcount() const { return bits_.count(); }

        [[nodiscard]] std::vector<uint8_t> GetCast() const;

    private:
        boost::dynamic_bitset<uint8_t> bits_;
    };
} // namespace bittorrent

#endif // CPPTORRENT_BITFIELD_H
