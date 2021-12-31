#ifndef CPPTORRENT_TRACKERREQUESTER_H
#define CPPTORRENT_TRACKERREQUESTER_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>

#include <utility>

#include "Tracker.h"

namespace ba = boost::asio;

namespace tracker {
    struct Tracker;
    struct Query;
    struct Response;
}

namespace network {
    struct BadConnect : std::runtime_error {
    public:
        explicit BadConnect(const std::string &arg) : runtime_error(arg) {};
        [[nodiscard]] const char *what() const noexcept override { return std::runtime_error::what(); }
    };

    struct TrackerRequester {
    public:
        virtual void Connect(const tracker::Query &query) = 0;
        [[nodiscard]] virtual boost::future<tracker::Response> GetResponse() { return promise_of_resp.get_future(); };
    protected:
        boost::promise<tracker::Response> promise_of_resp;
        boost::asio::streambuf response;

        virtual void SetResponse();
        virtual void SetException(const network::BadConnect &exc);
    };

    struct httpRequester : public TrackerRequester {
    public:
        explicit httpRequester(const std::shared_ptr<tracker::Tracker>& tracker, boost::asio::io_service & io_service)
                : tracker_(tracker), socket_(io_service), resolver(io_service) {}
        void Connect(const tracker::Query &query) override;
    private:
        void do_connect(ba::ip::tcp::resolver::iterator endpoint_iterator, const tracker::Query &query);
        void do_request(const tracker::Query &query);
        void do_read_response_status();
        void do_read_response_header();
        void do_read_response_body();
    private:
        ba::ip::tcp::resolver resolver;
        ba::ip::tcp::socket socket_;

        std::weak_ptr<tracker::Tracker> tracker_;
    };

    struct udpRequester : public TrackerRequester {
        void Connect(const tracker::Query &query) override {
             promise_of_resp.set_exception(network::BadConnect("No impl for UDP."));
        };
    };
}


#endif //CPPTORRENT_TRACKERREQUESTER_H
