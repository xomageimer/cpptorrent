#ifndef CPPTORRENT_MESSAGE_H
#define CPPTORRENT_MESSAGE_H

#include <boost/asio.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <cctype>

#include "constants.h"
#include "auxiliary.h"

namespace bittorrent {
    enum ByteOrder {
        BigEndian,
        LittleEndian
    };

    struct Message {
    public:
        explicit Message(ByteOrder bo = ByteOrder::BigEndian) : order_(bo) {}

        Message(const Message &) = delete;

        Message &operator=(const Message &) = delete;

        Message(Message &&) = default;

        Message &operator=(Message &&) = default;

        virtual ~Message() = default;

        virtual const uint8_t &operator[](size_t i) const = 0;

        [[nodiscard]] virtual std::basic_string_view<uint8_t> GetBufferData() const = 0;

        [[nodiscard]] virtual size_t Size() const = 0;

        void SetOrder(ByteOrder bo) { order_ = bo; }

        [[nodiscard]] ByteOrder GetOrder() const { return order_; }

    protected:
        ByteOrder order_ = ByteOrder::BigEndian;
    };

    struct ReceivingMessage : public Message {
    public:
        using Message::Message;

        ReceivingMessage(const uint8_t *data, size_t size, ByteOrder bo = ByteOrder::BigEndian) : arr_(data, size), Message(bo) {}

        const uint8_t &operator[](size_t i) const override { return arr_[i]; }

        [[nodiscard]] std::basic_string_view<uint8_t> GetBufferData() const override { return arr_; }

        void CopyTo(void *data, size_t size);

        std::string GetLine() const;

        std::string GetString() const;

        [[nodiscard]] size_t Size() const override { return arr_.size(); }

        template <typename T> ReceivingMessage &operator>>(T &value) {
            uint8_t bytes[sizeof(T)]{};
            CopyTo(bytes, sizeof(T));
            if (order_ == BigEndian) {
                value = BigToNative(ArrayToValue<T>(bytes));
            } else if (order_ == LittleEndian) {
                value = LittleToNative(ArrayToValue<T>(bytes));
            }
            return *this;
        }

        template <typename T> const ReceivingMessage &operator>>(T &value) const {
            return const_cast<ReceivingMessage &>(*this).template operator>>(value);
        }

    protected:
        std::basic_string_view<uint8_t> arr_;
        mutable size_t inp_pos_ = 0;
    };

    struct SendingMessage : public Message {
    public:
        using Message::Message;

        const uint8_t &operator[](size_t i) const override { return data_[i]; }

        [[nodiscard]] std::basic_string_view<uint8_t> GetBufferData() const override {
            return std::basic_string_view<uint8_t>{data_.data(), data_.size()};
        }

        void CopyFrom(const Message &msg_buf); // all reference to operator[] are is invalidated

        void CopyFrom(const uint8_t *data, size_t size); // all reference to operator[] are is invalidated

        void Clear();

        [[nodiscard]] size_t Size() const override { return data_.size(); }

        template <typename T> SendingMessage &operator<<(const T &value) {
            uint8_t bytes[sizeof(T)]{};
            if (order_ == BigEndian) {
                ValueToArray(NativeToBig(value), bytes);
                CopyFrom(bytes, sizeof(T));
            } else if (order_ == LittleEndian) {
                ValueToArray(NativeToLittle(value), bytes);
                CopyFrom(bytes, sizeof(T));
            }
            return *this;
        }

        template <typename T> const SendingMessage &operator<<(const T &value) const {
            return const_cast<SendingMessage &>(*this).template operator<<(value);
        }

        ReceivingMessage MakeReading() { return std::move(ReceivingMessage{data_.data(), data_.size(), order_}); }

    protected:
        std::vector<uint8_t> data_;
        size_t out_pos_ = 0;
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
    static const std::map<PEER_MESSAGE_TYPE, std::string> type_by_id_ {
        {choke, "choke"},
        {unchoke, "unchoke"},
        {interested, "interested"},
        {not_interested, "not_interested"},
        {have, "have"},
        {bitfield, "bitfield"},
        {request, "request"},
        {piece_block, "piece_block"},
        {cancel, "cancel"},
        {port, "port"},
    };

    static const inline size_t header_length = 4;

    static const inline size_t id_length = 1;

    struct ReceivingPeerMessage : public ReceivingMessage {
    public:
        using bittorrent::ReceivingMessage::ReceivingMessage;

        explicit ReceivingPeerMessage(ByteOrder bo = ByteOrder::BigEndian) : ReceivingMessage(nullptr, 0, bo) { inp_pos_ = 1; }

        [[nodiscard]] const auto *Body() const { return arr_.data(); }

        [[nodiscard]] size_t BodySize() const { return body_size_; };

        [[nodiscard]] PEER_MESSAGE_TYPE Type() const { return PEER_MESSAGE_TYPE{GetBufferData()[0]}; }

        bool DecodeHeader(uint32_t size) const;

        void SetBuffer(const uint8_t *data, size_t size) { arr_ = std::basic_string_view<uint8_t>{data, size}; }

    private:
        mutable size_t body_size_ = 0;
    };

    struct SendingPeerMessage : public SendingMessage {
    public:
        using bittorrent::SendingMessage::SendingMessage;

        explicit SendingPeerMessage(ByteOrder bo = ByteOrder::BigEndian) : SendingMessage(bo) { data_.resize(header_length, 0); }

        [[nodiscard]] auto *Body() { return data_.data() + header_length; }

        [[nodiscard]] size_t BodySize() const { return ArrayToValue<uint32_t>(&data_[0]); }

        [[nodiscard]] PEER_MESSAGE_TYPE Type() { return PEER_MESSAGE_TYPE{GetBufferData()[header_length]}; }

        void EncodeHeader();
    };
} // namespace bittorrent

#endif // CPPTORRENT_MESSAGE_H