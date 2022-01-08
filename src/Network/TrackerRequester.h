#ifndef CPPTORRENT_TRACKERREQUESTER_H
#define CPPTORRENT_TRACKERREQUESTER_H

#define BOOST_ASIO_ENABLE_CANCELIO

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/exception/all.hpp>
//#include <boost/endian/buffers.hpp>
//#include <boost/endian/conversion.hpp>

#include <utility>

#include "Tracker.h"

namespace ba = boost::asio;

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
        virtual void SetException(const std::string &exc) {
            promise_of_resp.set_exception(network::BadConnect(exc));
            Disconnect();
        }
    };

    struct httpRequester : public TrackerRequester {
    public:
        explicit httpRequester(const std::shared_ptr<tracker::Tracker>& tracker, ba::io_service & service)
                : resolver_(service), TrackerRequester(tracker) {}

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

        ba::ip::tcp::resolver resolver_;
        std::optional<ba::ip::tcp::socket> socket_;
    };

    struct udpRequester : public TrackerRequester {
    public:
        explicit udpRequester(const std::shared_ptr<tracker::Tracker>& tracker, ba::io_service & service)
                : resolver_(service), connect_timeout_(service), announce_timeout_(service), TrackerRequester(tracker) {}

        void Connect(ba::io_service & io_service, const tracker::Query &query) override;
        void Disconnect() override {
            announce_timeout_.cancel();
            connect_timeout_.cancel();
            socket_->close();
        };
    private:
        void do_resolve();

        void do_try_connect();
        void do_connect();
        void do_connect_response();

        void do_try_announce();
        void do_announce();
        void do_announce_response();

        void connect_deadline();
        void announce_deadline();
        void UpdateEndpoint();

        void SetResponse() override;

        void make_announce_request();
        void make_connect_request();

        ba::ip::udp::resolver resolver_;
        std::optional<ba::ip::udp::socket> socket_;
        ba::ip::udp::resolver::iterator endpoints_it_;

        uint8_t buff[16];

        static const inline int MTU = 1500;
        uint8_t request[98];
        uint8_t response[MTU] {};

        static const inline int MAX_CONNECT_ATTEMPTS = 4;
        static const inline int MAX_ANNOUNCE_ATTEMPTS = 3;
        size_t attempts_ = 0;
        size_t announce_attempts_ = 0;

        static const inline boost::posix_time::milliseconds epsilon {boost::posix_time::milliseconds(15)}; // чтобы сразу не закончить таймер!
        static const inline boost::posix_time::milliseconds connection_waiting_time {boost::posix_time::milliseconds(15)};
        static const inline boost::posix_time::milliseconds announce_waiting_time {boost::posix_time::milliseconds(10)};
        ba::deadline_timer connect_timeout_;
        ba::deadline_timer announce_timeout_;

        tracker::Query query_;

        struct connect_request {
            uint64_t protocol_id;
            uint32_t action {0};
            uint32_t transaction_id{};
        } c_req;
        struct connect_response {
            uint32_t action {0};
            uint32_t transaction_id{};
            uint64_t connection_id{};
        } c_resp;
        // TODO add scrape
    };
}

#endif //CPPTORRENT_TRACKERREQUESTER_H