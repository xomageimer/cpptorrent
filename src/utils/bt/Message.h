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

namespace bittorrent {
    enum ByteOrder {
        BigEndian,
        LittleEndian
    };

    struct Message {
    public:
        static const inline size_t max_body_length = bittorrent_constants::MTU;

        Message() : body_length_(0), pos_(0) {}

        Message(const Message &other) = default;

        virtual uint8_t * ReleaseData() { auto release_data = data_; data_ = nullptr; return data_;}

        [[nodiscard]] const uint8_t *GetDataPointer() const { return data_; }

        uint8_t *GetDataPointer() { return data_; }

        [[nodiscard]] virtual const uint8_t *Body() const { return data_; }

        virtual uint8_t *Body() { return data_; }

        [[nodiscard]] virtual std::size_t TotalLength() const { return body_length_; }

        [[nodiscard]] std::size_t BodyLength() const { return body_length_; }

        virtual void Resize(std::size_t new_length);

        virtual void Reset(std::size_t length);

    protected:
        uint8_t *data_{};
        size_t body_length_{};
        size_t pos_{};
        ByteOrder order = ByteOrder::BigEndian;
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

        PeerMessage() : Message() { data_ = new uint8_t[header_length]; }

        uint8_t * ReleaseData() override { auto release_data = data_; data_ = nullptr; return data_;}

        [[nodiscard]] inline std::size_t TotalLength() const override { return header_length + body_length_; }

        [[nodiscard]] const uint8_t *Body() const override { return data_ + header_length + id_length; }

        uint8_t *Body() override { return data_ + header_length + id_length; }

        void Resize(std::size_t new_length) override { Message::Resize(new_length + header_length + id_length); }

        void Reset(std::size_t length) override { Message::Reset(header_length + id_length + body_length_); }

        void SetMessageType(PEER_MESSAGE_TYPE type) { GetDataPointer()[header_length] = uint8_t(type); }

        [[nodiscard]] PEER_MESSAGE_TYPE GetMessageType() const { return PEER_MESSAGE_TYPE{GetDataPointer()[header_length]}; }

        void EncodeHeader();

        void DecodeHeader();
    };

    PeerMessage MakePeerMessage(const std::string &msg);
} // namespace bittorrent

#endif // CPPTORRENT_MESSAGE_H
