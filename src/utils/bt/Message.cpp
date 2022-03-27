#include "Message.h"

#include "auxiliary.h"

std::deque<bittorrent::Message> bittorrent::GetMessagesQueue(const std::string &msg) {
    size_t count = msg.size() > bittorrent::Message::max_body_length ? msg.size() / bittorrent::Message::max_body_length : 1;
    auto block_size = msg.size() / count;
    std::deque<bittorrent::Message> queue_new_msgs;
    auto msg_cur_pos = msg.data();
    for (size_t i = 0; i < count; i++) {
        bittorrent::Message m;
        m.body_length(block_size);
        std::memcpy(m.body(), msg_cur_pos, m.body_length());
        queue_new_msgs.push_back(m);
        msg_cur_pos += block_size;
    }
    auto total_blocks_size = msg.size() - block_size * (count);
    if (total_blocks_size) {
        auto last_pos = msg.data() + block_size * (count);
        auto last_size = (total_blocks_size == 1) ? 2 : total_blocks_size;
        bittorrent::Message m;
        m.body_length(last_size);
        std::memcpy(m.body(), last_pos, m.body_length());
        queue_new_msgs.push_back(m);
    }

    return std::move(queue_new_msgs);
}

void bittorrent::Message::encode_header() {
    char header[header_length + 1]{};
    std::sprintf(header, "4%d", static_cast<int>(body_length_));
    std::memcpy(data_, header, header_length);
}

void bittorrent::Message::decode_header() {
    char header[header_length + 1]{};
    std::strncat(header, reinterpret_cast<const char *>(data_), header_length);
    auto header_int = SwapEndian(ArrayToValue<int>(reinterpret_cast<uint8_t *>(header)));
    body_length_ = header_int;
}