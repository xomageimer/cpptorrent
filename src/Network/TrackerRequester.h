#ifndef CPPTORRENT_TRACKERREQUESTER_H
#define CPPTORRENT_TRACKERREQUESTER_H

#define BOOST_ASIO_ENABLE_CANCELIO

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/exception/all.hpp>
#include <boost/thread.hpp>

#include <utility>

#include "Primitives/Message.h"
#include "constants.h"
#include "NetExceptions.h"
#include "Tracker.h"
#include "Primitives/Socket.h"
#include "logger.h"

namespace ba = boost::asio;

namespace network {
    struct TrackerRequester {
    public:
        explicit TrackerRequester(const std::shared_ptr<bittorrent::Tracker> &tracker)
            : tracker_(*tracker) {}

        TrackerRequester(TrackerRequester const &) = delete;

        TrackerRequester(TrackerRequester &&) = delete;

        virtual void Connect(const bittorrent::Query &query) = 0;

        virtual void Disconnect() { is_set = true; }

        [[nodiscard]] virtual boost::future<bittorrent::Response> GetResponse() { return promise_of_resp.get_future(); };

        virtual ~TrackerRequester() { LOG("Destruction"); }

    protected:
        bool is_set = false;

        boost::promise<bittorrent::Response> promise_of_resp;

        const boost::posix_time::time_duration connect_waiting_ =
            bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon;

        bittorrent::Tracker &tracker_;

        boost::asio::streambuf msg_;

        virtual void SetResponse(Data) = 0;

        void SetException(const std::string &err) {
            if (is_set) return;

            LOG(tracker_.GetUrl().Host, " : ", " get exception");

            promise_of_resp.set_exception(network::BadConnect(err));
            Disconnect();
        }
    };

    struct httpRequester : public TrackerRequester, public TCPSocket {
    public:
        explicit httpRequester(const std::shared_ptr<bittorrent::Tracker> &tracker,
            const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor)
            : TrackerRequester(tracker), TCPSocket(executor) {}

        void Connect(const bittorrent::Query &query) override;

        void Disconnect() override {
            cancel_operation();
            TrackerRequester::Disconnect();
        }

    private:
        void do_request();

        void do_read_response_status();

        void do_read_response_header();

        void do_read_response_body();

        void SetResponse(Data) override;
    };

    struct udpRequester : public TrackerRequester, public UDPSocket {
    public:
        explicit udpRequester(const std::shared_ptr<bittorrent::Tracker> &tracker,
            const boost::asio::strand<typename boost::asio::io_service::executor_type> &executor)
            : TrackerRequester(tracker), UDPSocket(executor) {}

        void Connect(const bittorrent::Query &query) override;

        void Disconnect() override {
            cancel_operation();
            TrackerRequester::Disconnect();
        };

    private:
        void do_try_connect();

        void do_try_connect_delay(boost::posix_time::time_duration dur);

        void do_connect_response();

        void do_try_announce();

        void do_try_announce_delay(boost::posix_time::time_duration dur);

        void do_announce_response();

        void update_endpoint();

        void SetResponse(Data) override;

        void make_announce_request();

        void make_connect_request();

        size_t connect_attempts_ = bittorrent_constants::MAX_CONNECT_ATTEMPTS;

        size_t announce_attempts_ = bittorrent_constants::MAX_ANNOUNCE_ATTEMPTS;

        const boost::posix_time::time_duration announce_waiting_ =
            bittorrent_constants::announce_waiting_time + bittorrent_constants::epsilon;

        bittorrent::Query query_;

        struct connect_request {
            uint64_t protocol_id;
            uint32_t action{0};
            uint32_t transaction_id{};
        } c_req_;

        struct connect_response {
            uint32_t action{0};
            uint32_t transaction_id{};
            uint64_t connection_id{};
        } c_resp_;

        uint8_t announce_req_[bittorrent_constants::middle_buff_size] {};
        // TODO add scrape
    };
} // namespace network

#endif // CPPTORRENT_TRACKERREQUESTER_H