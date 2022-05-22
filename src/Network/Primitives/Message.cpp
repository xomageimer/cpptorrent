#include "Message.h"

void bittorrent::ReceivingMessage::CopyTo(void *data, size_t size) {
    std::memcpy(data, reinterpret_cast<const void *>(arr_.data() + inp_pos_) , size);
    inp_pos_ += size;
}

std::string bittorrent::ReceivingMessage::GetLine() const {
    std::string line;
    while (inp_pos_ != arr_.size() && arr_[inp_pos_] != '\n') {
        line.push_back(arr_[inp_pos_++]);
    }
    while (inp_pos_ != arr_.size() && std::isspace(arr_[inp_pos_]))
        inp_pos_++;
    return std::move(line);
}

std::string bittorrent::ReceivingMessage::GetString() const {
    std::string str;
    while (inp_pos_ != arr_.size() && !std::isspace(arr_[inp_pos_])) {
        str.push_back(arr_[inp_pos_++]);
    }
    while (inp_pos_ != arr_.size() && std::isspace(arr_[inp_pos_]))
        inp_pos_++;
    return std::move(str);
}

void bittorrent::ReceivingPeerMessage::DecodeHeader(uint32_t size) const {
    body_size_ = size;
}

void bittorrent::SendingMessage::CopyFrom(const Message &msg_buf) {
    CopyFrom(msg_buf.GetBufferData().data(), msg_buf.Size());
}

void bittorrent::SendingMessage::CopyFrom(const uint8_t *data, size_t size) {
    data_.reserve(size);
    data_.insert(data_.end(), data, data + size);
    out_pos_ += size;
}

void bittorrent::SendingMessage::Clear() {
    data_.clear();
    out_pos_ = 0;
}

void bittorrent::SendingPeerMessage::EncodeHeader() {
    uint32_t body_size = Size() - header_length;
    uint32_t encode_body_length = 0;
    encode_body_length = NativeToBig(body_size);
    ValueToArray(encode_body_length, &data_[0]);
}