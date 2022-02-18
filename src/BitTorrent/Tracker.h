#ifndef QTORRENT_TRACKER_H
#define QTORRENT_TRACKER_H

#include <cctype>
#include <optional>
#include <chrono>
#include <vector>
#include <string>
#include <utility>
#include <memory>

#include <boost/asio.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>
#include <boost/regex.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/keys.hpp>

#include "bencode_lib.h"
#include "Peer.h"

namespace bh = boost::hana;

namespace bittorrent {
    struct Torrent;
}
namespace network {
    struct TrackerRequester;
}

namespace tracker {
//    enum class ProtoType {
//        UDP,
//        HTTP,
//        HTTPS
//    };
    enum class Event : int32_t {
        Empty = 0,
        Completed = 1,
        Started = 2,
        Stopped = 3
    };
    static std::map<tracker::Event, std::string> events_str {
            {tracker::Event::Completed, "completed"},
            {tracker::Event::Started, "started"},
            {tracker::Event::Stopped, "stopped"}
    };
    struct Query {
        BOOST_HANA_DEFINE_STRUCT(Query,
                                 (Event, event),
                                 (size_t, port),
                                 (size_t, uploaded),
                                 (size_t, downloaded),
                                 (size_t, left),
                                 (bool, compact),
                                 (bool, no_peer_id),

                                 (std::optional<std::string>, ip),
                                 (std::optional<size_t>, numwant),
                                 (std::optional<std::string>, key),
                                 (std::optional<std::string>, trackerid)
        );

    };
    struct Response {
        struct peer_image{
            bittorrent::Peer dict; // PEERS with keys: peer_id, ip, port
            std::string bin; // string consisting of multiples of 6 bytes. First 4 bytes are the IP address and last 2 bytes are the port number. All in network (big endian) notation.
        };

        std::chrono::seconds interval;
        std::string tracker_id {};
        size_t complete {};
        size_t incomplete {};
        std::vector<peer_image> peers;

        std::optional<std::string> warning_message;
        std::optional<std::chrono::seconds> min_interval;

        Response() : interval(std::chrono::seconds(900)) {}
    };
    struct Url {
        BOOST_HANA_DEFINE_STRUCT(Url,
                                 (std::string, Protocol),
                                 (std::string, Host),
                                 (std::string, Port),

                                 (std::optional<std::string>, Path),
                                 (std::optional<std::string>, File),
                                 (std::optional<std::string>, Parameters)
        );
    };

    struct Tracker : std::enable_shared_from_this<Tracker> {
    private:
        Url tracker_url;
        bittorrent::Torrent & torrent;

        std::shared_ptr<network::TrackerRequester> request;
    public:
        Tracker(std::string tracker_url_arg, bittorrent::Torrent & torrent_arg);
        boost::future<Response> Request(boost::asio::io_service & service, const tracker::Query &query);

        auto Get() { return shared_from_this(); }

        Url const & GetUrl() const { return tracker_url; }
        size_t GetPort() const;
        std::string const & GetInfoHash() const;
        size_t GetMasterPeerId() const;
    private:
        friend class bittorrent::Torrent;
        void MakeRequester();
    };
}

#endif //QTORRENT_TRACKER_H
