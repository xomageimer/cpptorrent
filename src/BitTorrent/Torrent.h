#ifndef QTORRENT_TORRENT_H
#define QTORRENT_TORRENT_H

#include <string>
#include <memory>
#include <fstream>
#include <filesystem>
#include <utility>
#include <thread>

#include "Tracker.h"
#include "TrackerRequester.h"
#include "bencode_lib.h"
#include "random_generator.h"

// Объект с котором работает клиентский код, следовательно -> поменьше исключений | обрабатывать исключения

namespace bittorrent {
    struct meta_info_file {
        std::string info_hash;
        bencode::Node dict;
    };

    enum class launch {
        any,
        best
    };
    struct Torrent {
    public:
        explicit Torrent(std::filesystem::path const & torrent_file_path);
        bool TryConnect(bittorrent::launch policy = bittorrent::launch::best, tracker::Event event = tracker::Event::Empty);

        [[nodiscard]] std::string const & GetInfoHash() const { return meta_info.info_hash; }
        [[nodiscard]] bencode::Node const & GetMeta() const { return meta_info.dict;}
        [[nodiscard]] size_t GetMasterPeerKey() const { return master_peer->GetKey(); }
        [[nodiscard]] size_t GetPort() const { return port; }
        [[nodiscard]] tracker::Query GetDefaultTrackerQuery() const;
        [[nodiscard]] const tracker::Response & GetResponse() const;

        [[nodiscard]] bool HasTrackers() const { return !active_trackers.empty(); }
    private:
        mutable boost::asio::io_service service; // обязательно в самом верху

        size_t t_uploaded {};
        size_t t_downloaded {};
        size_t t_left {};

        friend class tracker::Tracker;
        std::list<std::shared_ptr<tracker::Tracker>> active_trackers;
        tracker::Response data_from_tracker;

        size_t port = 6881;                    // TODO надо иначе хендлить и создавать порты
        meta_info_file meta_info;
        std::shared_ptr<bittorrent::Peer> master_peer;
    private:
        bool FillTrackers();
        [[nodiscard]] boost::asio::io_service & GetService() const;
    };
}


#endif //QTORRENT_TORRENT_H
