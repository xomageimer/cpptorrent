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

        const boost::asio::streambuf &GetBuf() const { return *streambuf_ptr; }

        explicit Message(boost::asio::streambuf *ptr, ByteOrder bo = BigEndian)
            : streambuf_ptr(ptr), order_(bo), inp(std::make_shared<std::istream>(streambuf_ptr)),
              out(std::make_shared<std::ostream>(streambuf_ptr)){};

        Message(const Message &msg) : streambuf_ptr(msg.streambuf_ptr), inp(msg.inp), out(msg.out), order_(msg.order_) {}

        void SetOrder(ByteOrder bo) { order_ = bo; }

        const auto &operator[](size_t i) const { return boost::asio::buffer_cast<const uint8_t *>(GetBuf().data())[i]; }

        void Clear() { GetBuf().consume(GetBuf().size()); }

        void CopyFrom(const Message &msg_buf);

        void CopyFrom(const void *data, size_t size);

        void CopyTo(void *data, size_t size);

        std::string GetLine();

        ByteOrder GetOrder() { return order_; }

        template <typename T> Message &operator<<(const T &value) {
            if constexpr (!std::is_arithmetic_v<T>) {
                (*out) << value;
            } else {
                uint8_t bytes[sizeof(T)]{};
                if (order_ == BigEndian) {
                    ValueToArray(NativeToBig(value), bytes);
                    CopyFrom(bytes, sizeof(T));
                } else if (order_ == LittleEndian) {
                    ValueToArray(NativeToLittle(value), bytes);
                    CopyFrom(bytes, sizeof(T));
                }
            }
            return *this;
        }

        template <typename T> const Message &operator<<(const T &value) const {
            return const_cast<Message &>(*this).template operator<<(value);
        }

        template <typename T> Message &operator>>(T &value) {
            if constexpr (!std::is_arithmetic_v<T>) {
                (*inp) >> value;
            } else {
                uint8_t bytes[sizeof(T)]{};
                CopyTo(bytes, sizeof(T));
                if (order_ == BigEndian) {
                    value = BigToNative(ArrayToValue<T>(bytes));
                } else if (order_ == LittleEndian) {
                    value = LittleToNative(ArrayToValue<T>(bytes));
                }
            }
            return *this;
        }

        template <typename T> const Message &operator>>(T &value) const { return const_cast<Message &>(*this).template operator>>(value); }

    protected:
        ByteOrder order_ = ByteOrder::BigEndian;
        boost::asio::streambuf *streambuf_ptr;
        std::shared_ptr<std::istream> inp;
        std::shared_ptr<std::ostream> out;
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

        const auto *Body() const { return boost::asio::buffer_cast<const uint8_t *>(GetBuf().data()) + header_length + id_length; }

        size_t GetBodySize() const;

        PEER_MESSAGE_TYPE GetType() const {
            return PEER_MESSAGE_TYPE{boost::asio::buffer_cast<const uint8_t *>(GetBuf().data())[header_length]};
        };

        void SetHeader(uint32_t size);

    private:
        size_t body_size_ = 0;
    };
} // namespace bittorrent

#endif // CPPTORRENT_MESSAGE_H
