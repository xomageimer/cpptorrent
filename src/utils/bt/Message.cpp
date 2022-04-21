#include "Message.h"

#include "auxiliary.h"

void bittorrent::Message::Resize(std::size_t new_length) {
    auto old_length = body_length_;
    body_length_ = new_length;
    if (body_length_ > max_body_length) body_length_ = max_body_length;

    auto new_data = new uint8_t[body_length_];
    std::memcpy(new_data, data_, old_length);

    delete[] data_;
    data_ = new_data;
}

void bittorrent::Message::Reset(std::size_t length) {
    body_length_ = length;
    if (body_length_ > max_body_length) body_length_ = max_body_length;
    delete[] data_;
    data_ = new uint8_t[body_length_];
    pos_ = 0;
}

void bittorrent::PeerMessage::EncodeHeader() {
    char header[header_length + 1]{};
    auto BE_body_length = as_big_endian(static_cast<int>(body_length_)).AsValue();
    std::sprintf(header, "4%d", BE_body_length);
    std::memcpy(data_, header, header_length);
}

void bittorrent::PeerMessage::DecodeHeader() {
    char header[header_length + 1]{};
    std::strncat(header, reinterpret_cast<const char *>(data_), header_length);
    auto header_int = SwapEndian(ArrayToValue<int>(reinterpret_cast<uint8_t *>(header)));
    body_length_ = header_int;
    Resize(body_length_ + header_length);
}

bittorrent::PeerMessage bittorrent::MakePeerMessage(const std::string &bin_str) {
    PeerMessage msg;
    msg.Resize(bin_str.length());
    std::memcpy(msg.Body(), bin_str.data(), msg.BodyLength());
    msg.EncodeHeader();
    return msg;
}
