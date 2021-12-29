#ifndef QTORRENT_TORRENT_H
#define QTORRENT_TORRENT_H

#include <string>
#include <memory>
#include <fstream>
#include <filesystem>

#include "bencode_lib.h"
#include "Tracker.h"

// Объект с котором работает клиентский код, следовательно -> поменьше исключений | обрабатывать исключения

namespace bittorrent {
    struct meta_info_file {
        std::string info_hash;
        bencode::Node dict;
    };

    struct Torrent {
    public:
        explicit Torrent(std::filesystem::path const & torrent_file_path);
        bool TryConnect(tracker::Event event = tracker::Event::Empty); // TODO начальный метод просто сделать коннект, потом переделать! Мб обходить все доступные трекеры и проверять где лучше респоунс

        [[nodiscard]] std::string const & GetInfoHash() const { return meta_info.info_hash;}
        [[nodiscard]] bencode::Node const & GetMeta() const { return meta_info.dict;}
        [[nodiscard]] std::shared_ptr<bittorrent::Peer> GetMasterPeer() const;

        [[nodiscard]] bool HasTrackers() const { return !active_trackers.empty(); }
    private:
        size_t t_uploaded {};
        size_t t_downloaded {};
        size_t t_left {};

        std::vector<std::shared_ptr<tracker::Tracker>> active_trackers;
        size_t port = 6881;                    // TODO надо иначе хендлить и создавать порты
        meta_info_file meta_info;
        std::shared_ptr<bittorrent::Peer> master_peer;
    private:
        bool FillTrackers();
    };
}


#endif //QTORRENT_TORRENT_H
