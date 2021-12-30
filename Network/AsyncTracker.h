#ifndef CPPTORRENT_ASYNCTRACKER_H
#define CPPTORRENT_ASYNCTRACKER_H

#include <boost/asio.hpp>
#include <utility>

namespace ba = boost::asio;

namespace tracker {
    struct Tracker;
    struct Query;
}

namespace network {
    struct BadConnect : std::runtime_error {
    public:
        explicit BadConnect(const std::string &arg) : runtime_error(arg) {};

        [[nodiscard]] const char *what() const noexcept override { return std::runtime_error::what(); }
    };

    struct AsyncTracker {
    public:
        explicit AsyncTracker(std::shared_ptr<tracker::Tracker> tracker, boost::asio::io_service & io_service)
                : tracker_(std::move(tracker)), socket_(io_service), resolver(io_service) {}
        void Connect(const tracker::Query &query);
    private:
        void do_connect(const ba::ip::tcp::resolver::iterator& endpoint_iterator, const tracker::Query &query);
        void do_request(const tracker::Query &query);
        void do_read_response_status();
        void do_read_response_header();
        void do_read_response_body();
    private:
        ba::ip::tcp::resolver resolver;
        ba::ip::tcp::socket socket_;

        std::shared_ptr<tracker::Tracker> tracker_;

        boost::asio::streambuf response;
    };
}


#endif //CPPTORRENT_ASYNCTRACKER_H
