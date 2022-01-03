#ifndef CPPTORRENT_TRACKERREQUESTER_H
#define CPPTORRENT_TRACKERREQUESTER_H

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/exception/all.hpp>

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
        explicit TrackerRequester(const std::shared_ptr<tracker::Tracker>& tracker, boost::asio::io_service & io_service, ba::ip::tcp::resolver & resolver)
                : tracker_(tracker), socket_(io_service), resolver_(resolver) {}
        virtual void Connect(const tracker::Query &query) = 0;
        [[nodiscard]] virtual boost::future<tracker::Response> GetResponse() { return promise_of_resp.get_future(); };
        virtual ~TrackerRequester() = default;
    protected:
        ba::ip::tcp::resolver & resolver_;
        ba::ip::tcp::socket socket_;

        boost::asio::streambuf request_;
        boost::asio::streambuf response_;

        boost::promise<tracker::Response> promise_of_resp;
        std::weak_ptr<tracker::Tracker> tracker_;

        void SetResponse();
        void SetException(const std::string &exc);
    };

    struct httpRequester : public TrackerRequester {
    public:
        explicit httpRequester(std::shared_ptr<tracker::Tracker> tracker, boost::asio::io_service & io_service, ba::ip::tcp::resolver & resolver)
                : TrackerRequester(std::move(tracker), io_service, resolver) {}
        void Connect(const tracker::Query &query) override;
    private:
        void do_resolve();
        void do_connect(ba::ip::tcp::resolver::iterator endpoints);
        void do_request();
        void do_read_response_status();
        void do_read_response_header();
        void do_read_response_body();
    };

    struct udpRequester : public TrackerRequester {
        explicit udpRequester(std::shared_ptr<tracker::Tracker> tracker, boost::asio::io_service & io_service, ba::ip::tcp::resolver & resolver)
                : TrackerRequester(std::move(tracker), io_service, resolver) {}
        void Connect(const tracker::Query &query) override {
             promise_of_resp.set_exception(network::BadConnect("No impl for UDP."));
        };
    };
}


#endif //CPPTORRENT_TRACKERREQUESTER_H
