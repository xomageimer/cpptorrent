#include "Tracker.h"

#include "TrackerRequester.h"
#include "auxiliary.h"
#include "Torrent.h"

namespace ba = boost::asio;

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
    if (!boost::regex_split(std::back_inserter(urls_parts), tracker_url_arg, expression))
    {
        throw std::logic_error("Bad url, can't take it apart");
    }

    auto deserialize = [&](auto & object) {
        size_t i = 0;
        boost::hana::for_each(bh::keys(object), [&](auto key) {
            if (i == urls_parts.size())
                return;
            else {
                auto& member = bh::at_key(object, key);
                member = urls_parts[i++];
            }
        });
    };
    deserialize(tracker_url);
}

std::string const &tracker::Tracker::GetInfoHash() const {
    return torrent.GetInfoHash();
}

size_t tracker::Tracker::GetMasterPeerId() const {
    return torrent.GetMasterPeerKey();
}

size_t tracker::Tracker::GetPort() const {
    return torrent.GetPort();
}

boost::future<tracker::Response> tracker::Tracker::Request(boost::asio::io_service &service, const tracker::Query &query) {
    request->Connect(query);
    return request->GetResponse();
}

void tracker::Tracker::MakeRequester() {
    auto str_tolower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); }
        );
        return s;
    };
    if (str_tolower(tracker_url.Protocol) == "udp")
        request = std::make_shared<network::udpRequester>(Get(), make_strand(torrent.GetService()));
    else
        request = std::make_shared<network::httpRequester>(Get(), make_strand(torrent.GetService()));
}
