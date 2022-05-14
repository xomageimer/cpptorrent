#include "Message.h"

#include <type_traits>

void bittorrent::Message::CopyFrom(const Message &msg_buf) {
    std::size_t bytes_copied = boost::asio::buffer_copy(streambuf_ptr->prepare(msg_buf.GetBuf().size()), msg_buf.GetBuf().data());
    streambuf_ptr->commit(bytes_copied);
    arr_ = std::basic_string_view<uint8_t>(boost::asio::buffer_cast<const uint8_t *>(streambuf_ptr->data()), streambuf_ptr->size());
}

void bittorrent::Message::CopyFrom(const void *data, size_t size) {
    std::ostream out(streambuf_ptr);
    out.write(reinterpret_cast<const char *>(data), size);
    arr_ = std::basic_string_view<uint8_t>(boost::asio::buffer_cast<const uint8_t *>(streambuf_ptr->data()), streambuf_ptr->size());
}

void bittorrent::Message::CopyTo(void *data, size_t size) {
    std::memcpy(data, reinterpret_cast<const void *>(arr_.data() + inp_pos_) , size);
    inp_pos_ += size;
}

std::string bittorrent::Message::GetLine() {
    std::string line;
    while (out_pos_ != arr_.size() && arr_[out_pos_] != '\n') {
        line.push_back(arr_[out_pos_++]);
    }
    return std::move(line);
}

std::string bittorrent::Message::GetString() {
    std::string str;
    while (out_pos_ != arr_.size() && !std::isspace(arr_[out_pos_])) {
        str.push_back(arr_[out_pos_++]);
    }
    return std::move(str);
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