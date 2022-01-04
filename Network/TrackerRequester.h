#ifndef CPPTORRENT_TRACKERREQUESTER_H
#define CPPTORRENT_TRACKERREQUESTER_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/exception/all.hpp>
#include <boost/endian/buffers.hpp>

#include <utility>

#include "Tracker.h"

namespace ba = boost::asio;
namespace be = boost::endian;

namespace tracker {
    struct Tracker;
    struct Query;
    struct Response;
}

namespace network {
    struct BadConnect : public boost::exception, public std::exception {
    private:
        std::string exception;
    public:
        explicit BadConnect(std::string arg) : exception(std::move(arg)) {};
        [[nodiscard]] const char *what() const noexcept override { return exception.data(); }
    };

    struct TrackerRequester {
    public:
        explicit TrackerRequester(const std::shared_ptr<tracker::Tracker>& tracker)
                : tracker_(tracker) {}

        virtual void Connect(ba::io_service & io_service, const tracker::Query &query) = 0;
        virtual void Disconnect() = 0;
        [[nodiscard]] virtual boost::future<tracker::Response> GetResponse() { return promise_of_resp.get_future(); };
        virtual ~TrackerRequester() = default;
    protected:
        boost::promise<tracker::Response> promise_of_resp;
        std::weak_ptr<tracker::Tracker> tracker_;

        virtual void SetResponse() = 0;
        virtual void SetException(const std::string &exc);
    };

    struct httpRequester : public TrackerRequester {
    public:
        explicit httpRequester(const std::shared_ptr<tracker::Tracker>& tracker, ba::ip::tcp::resolver & resolver)
                : TrackerRequester(tracker), resolver_(resolver) {}

        void Connect(ba::io_service & io_service, const tracker::Query &query) override;
        void Disconnect() override {
            socket_->close();
            socket_.reset();
        };
    private:
        void do_resolve();
        void do_connect(ba::ip::tcp::resolver::iterator endpoints);
        void do_request();
        void do_read_response_status();
        void do_read_response_header();
        void do_read_response_body();

        void SetResponse() override;

        ba::streambuf request_;
        ba::streambuf response_;

        ba::ip::tcp::resolver & resolver_;
        std::optional<ba::ip::tcp::socket> socket_;
    };

    // TODO добавить глоабальный таймаут и в случае чего делать SetException
    struct udpRequester : public TrackerRequester {
    public:
        explicit udpRequester(const std::shared_ptr<tracker::Tracker>& tracker, ba::ip::udp::resolver & resolver)
                : resolver_(resolver), TrackerRequester(tracker) {}

        void Connect(ba::io_service & io_service, const tracker::Query &query) override;
        void Disconnect() override {
            socket_->close();
            socket_.reset();
        };
    private:
        void do_resolve();
        void do_connect(ba::ip::udp::resolver::iterator endpoints, uint8_t * buff);
        void do_read_connect_response();

        ba::ip::udp::resolver & resolver_;
        std::optional<ba::ip::udp::socket> socket_;

        tracker::Query query_;

        struct connect_request {
            be::big_int64_buf_t protocol_id {0x41727101980};
            be::big_int32_buf_t action {0};
            be::big_int32_buf_t transaction_id{};
        };
        struct connect_response {
            be::big_int32_buf_t action {0};
            be::big_int32_buf_t transaction_id{};
            be::big_int64_buf_t connection_id{};
        };
//        struct announce_request {
//            be::big_int64_buf_t connection_id{};
//            be::big_int32_buf_t action {1};
//            be::big_int32_buf_t transaction_id{};
//            int8_t info_hash[20]{};
//            int8_t peer_id[20]{};
//            be::big_int64_buf_t downloaded{};
//            be::big_int64_buf_t left{};
//            be::big_int64_buf_t uploaded{};
//            be::big_int32_buf_t event {0};
//            be::big_int32_buf_t ip {0};
//            be::big_int32_buf_t key{};
//            be::big_int32_buf_t numwant {-1};
//            be::big_int16_buf_t port{};
//        };
        struct announce_response {
            be::big_int32_buf_t action {1};
            be::big_int32_buf_t transaction_id{};
            be::big_int32_buf_t interval{};
            be::big_int32_buf_t leechers{};
            be::big_int32_buf_t seeders{};
            be::big_int32_buf_t ip{};
            be::big_int32_buf_t port{};
        };
        struct error_response {
            be::big_int32_buf_t action {3};
            be::big_int32_buf_t transaction_id;
            std::string message;
        };
        // TODO add scrape
    };
}

#endif //CPPTORRENT_TRACKERREQUESTER_H