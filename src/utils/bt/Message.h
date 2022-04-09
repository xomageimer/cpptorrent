#ifndef CPPTORRENT_MESSAGE_H
#define CPPTORRENT_MESSAGE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <utility>

#include "constants.h"

namespace bittorrent {
    enum MESSAGE_TYPE : uint8_t {
        choke = 0,
        unchoke = 1,
        interested = 2,
        not_interested = 3,
        have = 4,
        bitfield = 5,
        request = 6,
        piece_block = 7,
        cancel = 8,
        port = 9
    };
    // TODO сделать другой message, а этот унаследовать от него!
    struct Message {
    public:
        static const inline int max_body_length = bittorrent_constants::MTU;
        static const inline int header_length = 4;
        static const inline int id_length = 1;

        Message() : body_length_() {}
        Message(const Message &other) = default;

        [[nodiscard]] inline const uint8_t *data() const { return data_; }

        inline uint8_t *data() { return data_; }

        [[nodiscard]] inline std::size_t length() const { return header_length + body_length_; }

        [[nodiscard]] const uint8_t *body() const { return data_ + header_length + id_length; }

        uint8_t *body() { return data_ + header_length + id_length; }

        [[nodiscard]] std::size_t body_length() const { return body_length_; }

        void body_length(std::size_t new_length) {
            body_length_ = new_length;
            if (body_length_ > max_body_length) body_length_ = max_body_length;
        }

        [[nodiscard]] MESSAGE_TYPE GetMessageType() const { return MESSAGE_TYPE{data()[header_length + 1]}; }

        void encode_header();
        void decode_header();

    private:
        uint8_t data_[header_length + id_length + max_body_length]{};
        size_t body_length_{};
    };
    std::deque<Message> GetMessagesQueue(const std::string &msg);
} // namespace bittorrent

#endif // CPPTORRENT_MESSAGE_H
