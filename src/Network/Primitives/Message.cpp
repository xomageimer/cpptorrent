#include "Message.h"

#include <type_traits>

void bittorrent::Message::CopyFrom(const bittorrent::Message &msg_buf) {
    std::size_t bytes_copied = boost::asio::buffer_copy(prepare(msg_buf.size()), msg_buf.data());
    commit(bytes_copied);
}

void bittorrent::Message::CopyFrom(const void *data, size_t size) {
    out.write(reinterpret_cast<const char *>(data), size);
}

void bittorrent::Message::CopyTo(void *data, size_t size) {
    inp.read(reinterpret_cast<char *>(data), size);
}

std::string bittorrent::Message::GetLine() {
    std::string line;
    std::getline(inp, line);
    return std::move(line);
}

size_t bittorrent::PeerMessage::GetBodySize() const {
    return body_size_;
}

void bittorrent::PeerMessage::SetHeader(uint32_t size) {
    Clear();
    body_size_ = size;

    char header[header_length + 1]{};
    size_t encode_body_length = 0;
    if (order_ == BigEndian) {
        encode_body_length = NativeToBig(body_size_);
    } else if (order_ == LittleEndian) {
        encode_body_length = NativeToLittle(body_size_);
    }
    std::sprintf(header, "4%d", encode_body_length);
    CopyFrom(header, header_length);
}