#ifndef QTORRENT_TORRENT_H
#define QTORRENT_TORRENT_H

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "Peer.h"
#include "Tracker.h"
#include "bencode_lib.h"
#include "random_generator.h"

// Объект с которым работает клиентский код, следовательно -> поменьше исключений | обрабатывать исключения

namespace bittorrent {
    struct meta_info_file {
        std::string info_hash;
        bencode::Node dict;
    };
    enum class Launch {
        Any,
        Best
    };
    struct Torrent {
    public:
        explicit Torrent(boost::asio::io_service &service, std::filesystem::path const &torrent_file_path, std::filesystem::path const &path_to_download, size_t listener_port);
        bool TryConnect(bittorrent::Launch policy = bittorrent::Launch::Best, bittorrent::Event event = bittorrent::Event::Empty);
        void StartCommunicatingPeers();

        [[nodiscard]] size_t GetPeersSize() const { return data_from_tracker ? data_from_tracker.value().peers.size() : 0; }
        [[nodiscard]] std::string const &GetInfoHash() const { return meta_info.info_hash; }
        [[nodiscard]] bencode::Node const &GetMeta() const { return meta_info.dict; }
        [[nodiscard]] const uint8_t *GetMasterPeerKey() const { return master_peer->GetID(); }
        [[nodiscard]] std::shared_ptr<bittorrent::MasterPeer> GetRootPeer() { return master_peer; }
        [[nodiscard]] size_t GetPort() const { return port; }
        [[nodiscard]] bittorrent::Query GetDefaultTrackerQuery() const;
        [[nodiscard]] const bittorrent::Response &GetResponse() const;

        [[nodiscard]] bool HasTrackers() const { return !active_trackers.empty(); }

    private:
        boost::asio::io_service &service;// обязательно в самом верху

        size_t t_uploaded{};
        size_t t_downloaded{};
        size_t t_left{};

        friend class bittorrent::Tracker;
        std::list<std::shared_ptr<bittorrent::Tracker>> active_trackers;
        std::optional<bittorrent::Response> data_from_tracker;

        size_t port;
        meta_info_file meta_info;
        std::shared_ptr<bittorrent::MasterPeer> master_peer;

        const std::filesystem::path path_to_download;

    private:
        bool FillTrackers();
        [[nodiscard]] boost::asio::io_service &GetService() const;
    };
}// namespace bittorrent


#endif//QTORRENT_TORRENT_H
