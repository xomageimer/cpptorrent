#ifndef CPPTORRENT_PIECE_H
#define CPPTORRENT_PIECE_H

#include <cctype>
#include <cstdint>
#include <vector>

namespace bittorrent {
    struct Block {
        explicit Block(uint8_t *data, size_t size) : data_(data, data + size){};

        Block(const Block &other) = delete;

        Block &operator=(const Block &other) = delete;

        Block(Block &&other) noexcept = default;

        Block &operator=(Block &&other) noexcept = default;

        std::vector<uint8_t> data_;
    };

    struct Piece {
        explicit Piece(size_t idx, size_t bc) : index(idx), block_count(bc) { blocks.reserve(block_count); }

        std::vector<Block> blocks;

        size_t current_block{0};

        size_t block_count;

        size_t index;
    };
  //    void WritePiece(FileInfo fi);
} // namespace bittorrent

#endif // CPPTORRENT_PIECE_H
