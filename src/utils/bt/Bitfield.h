#ifndef CPPTORRENT_BITFIELD_H
#define CPPTORRENT_BITFIELD_H

#include "constants.h"
#include "auxiliary.h"

#include <algorithm>
#include <numeric>
#include <vector>

struct Bitfield {
public:
    explicit Bitfield(size_t size) : bits_(size, false) {}
    explicit Bitfield(std::vector<uint8_t> const & from_cast);

    bool Test(size_t i) { return bits_[i]; }
    void Set(size_t i) { bits_[i] = true; }
    void Clear(size_t i) { bits_[i] = false; }
    void Toggle(size_t i) { bits_[i] = !bits_[i]; }

    [[nodiscard]] size_t Size() const { return bits_.size(); }
    [[nodiscard]] size_t Popcount() const { return std::count_if(bits_.begin(), bits_.end(),
            [](auto el) { return el == true; }); }

    [[nodiscard]] std::vector<uint8_t> GetCast() const;
private:
    std::vector<bool> bits_;
};

#endif // CPPTORRENT_BITFIELD_H
