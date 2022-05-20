#ifndef CPPTORRENT_PIECE_H
#define CPPTORRENT_PIECE_H

#include <cctype>
#include <cstdint>
#include <vector>

namespace bittorrent {
    struct Block {
        explicit Block(uint8_t *data, size_t size, uint32_t beg) : data_(data, data + size), begin_(beg){};

        Block(const Block &other) = delete;

        Block &operator=(const Block &other) = delete;

        Block(Block &&other) noexcept = default;

        Block &operator=(Block &&other) noexcept = default;

        std::vector<uint8_t> data_;

        uint32_t begin_;
    };

    struct Piece {
        explicit Piece(size_t idx, size_t bc) : index(idx), block_count(bc) { blocks.reserve(block_count); }

        Piece(Piece &&) = default;

        Piece& operator=(Piece &&) = default;

        std::vector<Block> blocks;

        size_t current_blocks_num{0};

        size_t block_count;

        size_t index;
    };
} // namespace bittorrent

#endif // CPPTORRENT_PIECE_H
