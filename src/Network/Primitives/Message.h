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
        boost::asio::streambuf &GetBuf() { return *streambuf_ptr; }

        [[nodiscard]] const boost::asio::streambuf &GetBuf() const { return *streambuf_ptr; }

        explicit Message(boost::asio::streambuf *ptr, ByteOrder bo = BigEndian)
            : streambuf_ptr(ptr), arr_(boost::asio::buffer_cast<const uint8_t *>(streambuf_ptr->data()), streambuf_ptr->size()),
              order_(bo){};

        Message(const Message &msg) : streambuf_ptr(msg.streambuf_ptr), arr_(msg.arr_), order_(msg.order_) {}

        void SetOrder(ByteOrder bo) { order_ = bo; }

        const auto &operator[](size_t i) const { return arr_[i]; }

        void Clear() {
            GetBuf().consume(GetBuf().size());
            arr_ = std::basic_string_view<uint8_t>(boost::asio::buffer_cast<const uint8_t *>(streambuf_ptr->data()), streambuf_ptr->size());
        }

        void CopyFrom(const Message &msg_buf);

        void CopyFrom(const void *data, size_t size);

        void CopyTo(void *data, size_t size);

        std::string GetLine();

        std::string GetString();

        ByteOrder GetOrder() { return order_; }

        template <typename T> Message &operator<<(const T &value) {
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

        template <typename T> const Message &operator<<(const T &value) const {
            return const_cast<Message &>(*this).template operator<<(value);
        }

        template <typename T> Message &operator>>(T &value) {
            uint8_t bytes[sizeof(T)]{};
            CopyTo(bytes, sizeof(T));
            if (order_ == BigEndian) {
                value = BigToNative(ArrayToValue<T>(bytes));
            } else if (order_ == LittleEndian) {
                value = LittleToNative(ArrayToValue<T>(bytes));
            }
            return *this;
        }

        template <typename T> const Message &operator>>(T &value) const { return const_cast<Message &>(*this).template operator>>(value); }

    protected:
        ByteOrder order_ = ByteOrder::BigEndian;
        boost::asio::streambuf *streambuf_ptr;
        std::basic_string_view<uint8_t> arr_;
        size_t inp_pos_ = 0;
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

    struct PeerMessage : public Message {
    public:
        using bittorrent::Message::Message;

        static const inline size_t max_body_length = bittorrent_constants::most_request_size;

        static const inline size_t header_length = 4;

        static const inline size_t id_length = 1;

        [[nodiscard]] const auto *Body() const {
            return boost::asio::buffer_cast<const uint8_t *>(GetBuf().data()) + header_length + id_length;
        }

        [[nodiscard]] size_t GetBodySize() const;

        [[nodiscard]] PEER_MESSAGE_TYPE GetType() const {
            return PEER_MESSAGE_TYPE{boost::asio::buffer_cast<const uint8_t *>(GetBuf().data())[header_length]};
        };

        void SetHeader(uint32_t size);

    private:
        size_t body_size_ = 0;
    };
} // namespace bittorrent

#endif // CPPTORRENT_MESSAGE_H
