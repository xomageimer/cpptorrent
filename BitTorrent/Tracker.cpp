#include "Tracker.h"

#include <map>
#include <utility>

#include <boost/asio.hpp>
#include <boost/regex.hpp>

#include "auxiliary.h"
#include "Torrent.h"

namespace ba = boost::asio;

// TODO потом перенести логику в приватные методы, а тут решать по протоколу какой (http или https) алгоритм обработки выбрать
std::pair<std::string, std::optional<tracker::Response>> tracker::httpRequester::operator()(const tracker::Query &query, const tracker::Tracker & tracker) {
    auto & tracker_url = tracker.GetUrl();
    static std::map<tracker::Event, std::string> events_str {
            {tracker::Event::Completed, "completed"},
            {tracker::Event::Started, "started"},
            {tracker::Event::Stopped, "stopped"}
    };

    SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));

    boost::asio::io_service service;

    ba::ip::tcp::resolver resolver(service);
    auto resolver_query = ba::ip::tcp::resolver::query{tracker_url.Host, tracker_url.Port};

    boost::system::error_code ec;
    ba::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(resolver_query, ec);
    if (ec){
        return {"Can't resolve announce url", std::nullopt};;
    }

    ba::ip::tcp::socket socket(service);

    while (true) {
        try {
            do {
                socket.close();
                socket.connect(*endpoint_iterator++, ec);
            } while (ec && endpoint_iterator != boost::asio::ip::tcp::resolver::iterator());

            ba::streambuf request_query;
            std::ostream request_stream(&request_query);
            request_stream << "GET /" << tracker_url.Path.value_or("") << "?"

                           << "info_hash=" << UrlEncode(tracker.GetInfoHash())
                           << "&peer_id=" << UrlEncode(std::to_string(tracker.GetMasterPeerId()))
                           << "&port=" << tracker.GetPort() << "&uploaded=" << query.uploaded << "&downloaded="
                           << query.downloaded << "&left=" << query.left << "&compact=1"
                           << ((query.event != tracker::Event::Empty) ? "&event=" + events_str.at(query.event) : "")

                           << " HTTP/1.0\r\n"
                           << "Host: " << tracker_url.Host << "\r\n"
                           << "Accept: */*\r\n"
                           << "Connection: close\r\n\r\n";
//            std::cout << request_stream.rdbuf() << std::endl;
            ba::write(socket, request_query);

            ba::streambuf response;
            ba::read_until(socket, response, "\r\n");

            std::istream response_stream(&response);
            std::string http_version;
            response_stream >> http_version;
            std::cerr << http_version << std::endl;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                return {"Invalid response\n", std::nullopt};
            }
            if (status_code != 200) {
                std::string err = "Response returned with status code " + std::to_string(status_code) + "\n";
                return {std::move(err), std::nullopt};
            }

            boost::asio::read_until(socket, response, "\r\n\r\n");

            std::string header;
            while (std::getline(response_stream, header) && header != "\r")
                std::cout << header << "\n";
            std::cout << "\n";

            if (response.size() > 0)
                std::cout << &response;

            boost::system::error_code error;
            while (boost::asio::read(socket, response,
                                     boost::asio::transfer_at_least(1), error))
                std::cout << &response;
            if (error != boost::asio::error::eof)
                throw boost::system::system_error(error);

            return {"", Response{}};
        } catch (std::exception &e) {
            if (endpoint_iterator == boost::asio::ip::tcp::resolver::iterator())
                break;

            std::string err = "Exception: " + std::string(e.what()) + "\n";
            return {std::move(err), std::nullopt};
        }
    }
}

tracker::Tracker::Tracker(std::string tracker_url_arg, size_t port_arg,
                          bittorrent::Torrent & torrent_arg)
    : port(port_arg), torrent(torrent_arg)
{
    std::vector<std::string> urls_parts;
    boost::regex expression(
            // proto
            "^(\?:([^:/\?#]+)://)\?"
            // host
            "(\\w+[^/\?#:]*)"
            // port
            "(\?::(\\d+))\?"
            // path
            "\\/?([^?#]*)"
    );
    // TODO мб рефлексию вместо этого {
    if (!boost::regex_split(std::back_inserter(urls_parts), tracker_url_arg, expression))
    {
        // TODO добавить ошибку / исключение
        return;
    }

    tracker_url.Protocol = urls_parts[0];
    tracker_url.Host = urls_parts[1];
    tracker_url.Port = urls_parts[2];
    if (urls_parts.size() > 3)
        tracker_url.Path = urls_parts[3];
    // TODO }

    if (tracker_url.Protocol == "udp")
        request = std::make_shared<udpRequester>();
    else
        request = std::make_shared<httpRequester>();
}

std::string const &tracker::Tracker::GetInfoHash() const {
    return torrent.GetInfoHash();
}

size_t tracker::Tracker::GetMasterPeerId() const {
    return torrent.GetMasterPeer()->GetKey();
}
