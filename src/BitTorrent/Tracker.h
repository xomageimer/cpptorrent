#ifndef QTORRENT_TRACKER_H
#define QTORRENT_TRACKER_H

#include <cctype>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY

#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/keys.hpp>
#include <boost/hana/pair.hpp>

#include "constants.h"
#include "Peer.h"
#include "bencode_lib.h"

namespace bh = boost::hana;

namespace bittorrent {
    struct Torrent;
}
namespace network {
    struct TrackerRequester;
}

namespace bittorrent {
    enum class Event : int32_t {
        Empty = 0,
        Completed = 1,
        Started = 2,
        Stopped = 3
    };
    static std::map<bittorrent::Event, std::string> events_str{
            {bittorrent::Event::Completed, "completed"},
            {bittorrent::Event::Started, "started"},
            {bittorrent::Event::Stopped, "stopped"}};
    struct Query {
        Event event;
        size_t port{};
        size_t uploaded{};
        size_t downloaded{};
        size_t left{};
        bool compact{};
        bool no_peer_id{};

        std::optional<std::string> ip;
        std::optional<size_t> numwant;
        std::optional<std::string> key;
        std::optional<std::string> trackerid;
    };
    struct Response {
        std::chrono::seconds interval;
        std::string tracker_id{};
        size_t complete{};
        size_t incomplete{};
        std::vector<bittorrent::PeerImage> peers;

        std::optional<std::string> warning_message;
        std::optional<std::chrono::seconds> min_interval;

        Response() : interval(std::chrono::seconds(bittorrent_constants::tracker_again_request_time_secs)) {}
    };
    struct Url {
        BOOST_HANA_DEFINE_STRUCT(Url,
                                 (std::string, Protocol),
                                 (std::string, Host),
                                 (std::string, Port),

                                 (std::optional<std::string>, Path),
                                 (std::optional<std::string>, File),
                                 (std::optional<std::string>, Parameters));
    };

    struct Tracker : std::enable_shared_from_this<Tracker> {
    public:
        Tracker(std::string tracker_url_arg, bittorrent::Torrent &torrent_arg);
        boost::future<Response> Request(const bittorrent::Query &query);

        auto Get() { return shared_from_this(); }

        Url const &GetUrl() const { return tracker_url; }
        size_t GetPort() const;
        std::string const &GetInfoHash() const;
        const uint8_t *GetMasterPeerId() const;

    private:
        void MakeRequester();

        friend class bittorrent::Torrent;
        std::shared_ptr<network::TrackerRequester> request;

        Url tracker_url;
        bittorrent::Torrent &torrent;
    };
}// namespace bittorrent

#endif//QTORRENT_TRACKER_H