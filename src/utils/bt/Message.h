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
#include "auxiliary.h"

// TODO сделать специальный message кт и будет отправляться и который будет определять порядок байт при конструировании, например: \
    OutputMessage out(ByteOrder::BigEndian, 17);

using Data = std::basic_string_view<uint8_t>;

// TODO отреафкторить и упросить!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

namespace bittorrent {
    enum ByteOrder {
        BigEndian,
        LittleEndian
    };

    struct Message {
    public:
        static const inline size_t max_body_length = bittorrent_constants::MTU;

        Message(size_t size, ByteOrder order = ByteOrder::BigEndian);

        Message(uint8_t *data, size_t size, ByteOrder order = ByteOrder::BigEndian);

        Message(Data data, ByteOrder order = ByteOrder::BigEndian);

        Message() : body_length_(0), in_pos_(0) {}

        Message(const Message &other);

        Message &operator=(const Message &other);

        Message(Message &&other) noexcept;

        Message &operator=(Message &&other) noexcept;

        virtual ~Message() { delete[] data_; }

        virtual uint8_t *ReleaseData();

        void Add(const uint8_t * add_data, size_t size);

        operator Data() { return {GetDataPointer(), TotalLength()}; }

        [[nodiscard]] const uint8_t *GetDataPointer() const { return data_; }

        uint8_t *GetDataPointer() { return data_; }

        [[nodiscard]] virtual std::size_t TotalLength() const { return body_length_; }

        [[nodiscard]] std::size_t BodyLength() const { return body_length_; }

        void SetOrder(ByteOrder bo) { order_ = bo; }

        [[nodiscard]] ByteOrder GetOrder() const { return order_; }

        virtual void Resize(std::size_t new_length);

        virtual void Reset(std::size_t length);

        bool Empty(){
            return out_pos_ <= TotalLength();
        }

        template <typename T> Message &operator>>(T &value) {
            static_assert(std::is_integral_v<T>);

            if (order_ == BigEndian) {
                value = BigToNative(ArrayToValue<T>(&data_[out_pos_]));
            } else if (order_ == LittleEndian) {
                value = LittleToNative(ArrayToValue<T>(&data_[out_pos_]));
            }
            out_pos_ += sizeof(T);
            return *this;
        }

        template <typename T> const Message &operator>>(T &value) const { return const_cast<Message &>(*this).template operator>>(value); }

        std::string GetString() {
            std::string value;
            while (out_pos_ < body_length_ && (data_[out_pos_] != ' ' || data_[out_pos_] != '\n')) {
                value.push_back(static_cast<char>(data_[out_pos_]));
            }
            return value;
        }

        std::string GetLine(){
            std::string value;
            while (out_pos_ < body_length_ && data_[out_pos_] != '\n') {
                value.push_back(static_cast<char>(data_[out_pos_]));
            }
            return value;
        }

        template <typename T> Message &operator<<(T &value) {
            static_assert(std::is_integral_v<T>);

            if (order_ == BigEndian) {
                ValueToArray(NativeToBig(value), &data_[in_pos_]);
            } else if (order_ == LittleEndian) {
                ValueToArray(NativeToLittle(value), &data_[in_pos_]);
            }
            in_pos_ += sizeof(T);
            return *this;
        }

        template <typename T> const Message &operator<<(T &value) const { return const_cast<Message &>(*this).template operator<<(value); }

    protected:
        uint8_t *data_{nullptr};
        size_t body_length_{0};
        size_t in_pos_{0};
        size_t out_pos_{0};
        ByteOrder order_ = ByteOrder::BigEndian;
    };

    enum PEER_MESSAGE_TYPE : uint8_t {
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

    struct PeerMessage : public Message {
    public:
        static const inline size_t max_body_length = bittorrent_constants::most_request_size;

        static const inline size_t header_length = 4;

        static const inline size_t id_length = 1;

        PeerMessage() : Message() { data_ = new uint8_t[header_length + id_length]; }

        PeerMessage(uint8_t *data, size_t size, ByteOrder order = ByteOrder::BigEndian) : Message() {
            order_ = order;
            PeerMessage::Reset(size);
            std::memcpy(PeerMessage::Body(), data, size);
        }

        [[nodiscard]] inline std::size_t TotalLength() const override { return header_length + body_length_; }

        [[nodiscard]] const uint8_t *Body() const { return data_ + header_length + id_length; }

        uint8_t *Body() { return data_ + header_length + id_length; }

        void Resize(std::size_t new_length) override { Message::Resize(new_length + header_length + id_length); }

        void SetHeader(uint32_t length) { ValueToArray(length, &data_[0]); }

        void Reset(std::size_t new_length) override {
            Message::Reset(new_length + header_length + id_length);
            in_pos_ = header_length + id_length;
        }

        void SetMessageType(PEER_MESSAGE_TYPE type) { GetDataPointer()[header_length] = uint8_t(type); }

        [[nodiscard]] PEER_MESSAGE_TYPE GetMessageType() const { return PEER_MESSAGE_TYPE{GetDataPointer()[header_length]}; }

        void EncodeHeader();

        void DecodeHeader();

        template <typename T> PeerMessage &operator>>(T &value) {
            static_assert(std::is_integral_v<T>);

            if (order_ == BigEndian) {
                value = BigToNative(ArrayToValue<T>(&data_[out_pos_]));
            } else if (order_ == LittleEndian) {
                value = LittleToNative(ArrayToValue<T>(&data_[out_pos_]));
            }
            out_pos_ += sizeof(T);
            return *this;
        }

        template <typename T> const PeerMessage &operator>>(T &value) const {
            return const_cast<PeerMessage &>(*this).template operator>>(value);
        }

        template <typename T> PeerMessage &operator<<(T &value) {
            static_assert(std::is_integral_v<T>);

            if (order_ == BigEndian) {
                ValueToArray(NativeToBig(value), &data_[in_pos_]);
            } else if (order_ == LittleEndian) {
                ValueToArray(NativeToLittle(value), &data_[in_pos_]);
            }
            in_pos_ += sizeof(T);
            return *this;
        }

        template <typename T> const PeerMessage &operator<<(T &value) const {
            return const_cast<PeerMessage &>(*this).template operator<<(value);
        }
    };

    PeerMessage UpgradeMsg(const Message &msg);
    Message DowngradeMsg(const PeerMessage &peer_msg);

    PeerMessage MakePeerMessage(Data bin_data);
} // namespace bittorrent

#endif // CPPTORRENT_MESSAGE_H
