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

bool bittorrent::ReceivingPeerMessage::DecodeHeader(uint32_t size) const {
    body_size_ = size;
    if (body_size_ > bittorrent_constants::most_request_size) {
        return false;
    }
    return true;
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
    auto body_size = Size();
    char header[header_length + 1]{};

    size_t encode_body_length = 0;
    if (order_ == BigEndian) {
        encode_body_length = NativeToBig(body_size);
    } else if (order_ == LittleEndian) {
        encode_body_length = NativeToLittle(body_size);
    }

    std::sprintf(header, "4%d", encode_body_length);
    std::memcpy(&data_[0], reinterpret_cast<uint8_t *>(header), header_length);
}