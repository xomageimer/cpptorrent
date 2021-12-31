#include "Tracker.h"

#include <map>
#include <utility>

#include "TrackerRequester.h"
#include <boost/regex.hpp>

#include "auxiliary.h"
#include "Torrent.h"

namespace ba = boost::asio;

// TODO потом перенести логику в приватные методы, а тут решать по протоколу какой (http или https) алгоритм обработки выбрать

tracker::Tracker::Tracker(std::string tracker_url_arg,
                          bittorrent::Torrent & torrent_arg)
    : torrent(torrent_arg)
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

    if (tracker_url.Protocol == "udp")
        request = std::make_shared<network::udpRequester>();
    else
        request = std::make_shared<network::httpRequester>(Get(), torrent.GetService());
    // TODO }
}

std::string const &tracker::Tracker::GetInfoHash() const {
    return torrent.GetInfoHash();
}

size_t tracker::Tracker::GetMasterPeerId() const {
    return torrent.GetMasterPeer()->GetKey();
}

size_t tracker::Tracker::GetPort() const {
    return torrent.GetPort();
}

boost::future<tracker::Response> tracker::Tracker::Request(boost::asio::io_service &service, const tracker::Query &query) {
    request->Connect(query);
    return request->GetResponse();
}
