#include "Message.h"

#include "auxiliary.h"

bittorrent::Message bittorrent::MakeMessage(const std::string &bin_str) {
    Message msg;
    msg.body_length(bin_str.length());
    std::memcpy(msg.body(), bin_str.data(), msg.body_length());
    msg.encode_header();
    return msg;
}

void bittorrent::Message::encode_header() {
    char header[header_length + 1]{};
    auto BE_body_length = as_big_endian(static_cast<int>(body_length_)).AsValue();
    std::sprintf(header, "4%d", BE_body_length);
    std::memcpy(data_, header, header_length);
}

void bittorrent::Message::decode_header() {
    char header[header_length + 1]{};
    std::strncat(header, reinterpret_cast<const char *>(data_), header_length);
    auto header_int = SwapEndian(ArrayToValue<int>(reinterpret_cast<uint8_t *>(header)));
    body_length_ = header_int;
}