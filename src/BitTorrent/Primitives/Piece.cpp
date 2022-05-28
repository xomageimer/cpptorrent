#include "Piece.h"

#include <boost/compute/detail/sha1.hpp>
#include <boost/function_output_iterator.hpp>

#include <cstring>

std::string bittorrent::GetSHA1FromPiece(const Piece &p_arg) {
    boost::uuids::detail::sha1 sha1;
    for (const auto & block : p_arg.blocks) {
        sha1.process_block(block.data_.data(), block.data_.data() + block.data_.size());
    }
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

    union value_type {
        unsigned full;
        unsigned char u8[sizeof(unsigned)];
    } dest{};
    for (auto &el : hash) {
        value_type source{};
        source.full = el;

        for (size_t k = 0; k < sizeof(unsigned); k++) {
            dest.u8[k] = source.u8[sizeof(unsigned) - k - 1];
        }
        el = dest.full;
    }

    char str_hash[sizeof(unsigned) * 5];

    memcpy(str_hash, (char *)&hash, sizeof(unsigned) * 5);
    return {str_hash, std::size(str_hash)};
}