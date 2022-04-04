#ifndef CPPTORRENT_PIECE_H
#define CPPTORRENT_PIECE_H

#include <vector>
#include <cctype>
#include <cstdint>

namespace bittorrent {
    struct Block {
        size_t size = 0;
        uint8_t * data = nullptr;

        explicit Block() = default;
        ~Block() { delete [] data; }
    };
    struct Piece {
        std::vector<Block> blocks;
        size_t index;
    };
} // namespace bittorrent

#endif // CPPTORRENT_PIECE_H
