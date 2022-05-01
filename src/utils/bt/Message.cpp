#include "Message.h"

#include <type_traits>

bittorrent::Message::Message(uint8_t *data, size_t size, bittorrent::ByteOrder order) : order_(order) {
    Message::Reset(size);
    std::memcpy(data_, data, size);
}

bittorrent::Message::Message(size_t size, bittorrent::ByteOrder order) : order_(order) {
    Message::Reset(size);
}

bittorrent::Message::Message(Data data, bittorrent::ByteOrder order) {
    Message::Reset(data.size());
    std::memmove(data_, data.data(), data.size());
}

bittorrent::Message::Message(bittorrent::Message &&other) noexcept {
    std::swap(body_length_, other.body_length_);
    std::swap(data_, other.data_);
    order_ = other.order_;
    in_pos_ = other.in_pos_;
    out_pos_ = other.out_pos_;
}

bittorrent::Message &bittorrent::Message::operator=(bittorrent::Message &&other) noexcept {
    if (this != &other) {
        std::swap(body_length_, other.body_length_);
        std::swap(data_, other.data_);
        order_ = other.order_;
        in_pos_ = other.in_pos_;
        out_pos_ = other.out_pos_;
    }
    return *this;
}

void bittorrent::Message::Resize(std::size_t new_length) {
    auto old_length = body_length_;

    if (out_pos_ >= new_length) out_pos_ = 0;
    if (in_pos_ >= new_length) in_pos_ = 0;

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
    in_pos_ = 0;
    out_pos_ = 0;
}

uint8_t *bittorrent::Message::ReleaseData() {
    auto release_data = data_;
    data_ = nullptr;
    return release_data;
}
bittorrent::Message::Message(const bittorrent::Message &other) {
    Message::Reset(other.TotalLength());
    std::memcpy(data_, other.data_, Message::TotalLength());
    order_ = other.order_;
    in_pos_ = other.in_pos_;
    out_pos_ = other.out_pos_;
}

bittorrent::Message &bittorrent::Message::operator=(const bittorrent::Message &other) {
    if (this != &other) {
        Message::Reset(other.TotalLength());
        std::memcpy(data_, other.data_, TotalLength());
        order_ = other.order_;
        in_pos_ = other.in_pos_;
        out_pos_ = other.out_pos_;
    }

    return *this;
}

void bittorrent::PeerMessage::EncodeHeader() {
    char header[header_length + 1]{};
    size_t encode_body_length = 0;
    if (order_ == BigEndian) {
        encode_body_length = NativeToBig(body_length_);
    } else if (order_ == LittleEndian) {
        encode_body_length = NativeToLittle(body_length_);
    }
    std::sprintf(header, "4%d", encode_body_length);
    std::memcpy(data_, header, header_length);
    in_pos_ = header_length + id_length;
}

void bittorrent::PeerMessage::DecodeHeader() {
    char header[header_length + 1]{};
    std::strncat(header, reinterpret_cast<const char *>(data_), header_length);
    int header_int = 0;
    if (order_ == BigEndian) {
        header_int = BigToNative(ArrayToValue<int>(reinterpret_cast<uint8_t *>(header)));
    } else if (order_ == LittleEndian) {
        header_int = LittleToNative(ArrayToValue<int>(reinterpret_cast<uint8_t *>(header)));
    }
    body_length_ = header_int;
    Reset(body_length_);
    out_pos_ = header_length + id_length;
}

bittorrent::PeerMessage bittorrent::MakePeerMessage(Data bin_data) {
    PeerMessage msg;
    msg.Reset(bin_data.size());
    std::memcpy(msg.Body(), bin_data.data(), msg.BodyLength());
    msg.EncodeHeader();
    return msg;
}

bittorrent::PeerMessage bittorrent::UpgradeMsg(const bittorrent::Message &msg) {
    bittorrent::PeerMessage peer_msg;
    peer_msg.Reset(msg.TotalLength());
    std::memcpy(peer_msg.Body(), msg.GetDataPointer(), msg.TotalLength());
    peer_msg.SetOrder(msg.GetOrder());
    return std::move(peer_msg);
}

bittorrent::Message bittorrent::DowngradeMsg(const bittorrent::PeerMessage &peer_msg) {
    bittorrent::Message msg(peer_msg.BodyLength(), peer_msg.GetOrder());
    std::memcpy(msg.GetDataPointer(), peer_msg.Body(), msg.BodyLength());
    return std::move(msg);
}