#ifndef CPPTORRENT_PIECE_H
#define CPPTORRENT_PIECE_H

#include <cctype>
#include <cstdint>
#include <vector>

namespace bittorrent {
    // TODO переделать pieces, убрать блоки!
    struct Block {
        Block(uint8_t *data, size_t size) : m_data(data), m_size(size){};
        Block(const Block &other) = delete;
        Block &operator=(const Block &other) = delete;
        Block(Block &&other) noexcept {
            m_data = other.m_data;
            other.m_data = nullptr;
            m_size = other.m_size;
        }
        Block &operator=(Block &&other) noexcept {
            if (this != &other) {
                m_data = other.m_data;
                other.m_data = nullptr;
                m_size = other.m_size;
            }
            return *this;
        }

        uint8_t *m_data = nullptr;
        size_t m_size = 0;

        ~Block() { delete[] m_data; }
    };

    struct Piece {
        std::vector<Block> blocks;

        size_t current_block;

        size_t block_count;

        size_t index;
    };
} // namespace bittorrent

#endif // CPPTORRENT_PIECE_H
