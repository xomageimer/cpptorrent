#ifndef QTORRENT_TORRENT_H
#define QTORRENT_TORRENT_H

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "TorrentFilesManager.h"
#include "Peer.h"
#include "Tracker.h"

#include "bt/Piece.h"
#include "bencode_lib.h"
#include "random_generator.h"

// Объект с которым работает клиентский код, следовательно -> поменьше исключений | обрабатывать исключения
// TODO изменить стиль названий, поля должны кончаться на _, а название приватных методов быть name_name_name... ()

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
        explicit Torrent(boost::asio::io_service &service, std::filesystem::path const &torrent_file_path,
            std::filesystem::path const &path_to_download, size_t listener_port);

        bool TryConnect(bittorrent::Launch policy = bittorrent::Launch::Best, bittorrent::Event event = bittorrent::Event::Empty);

        void StartCommunicatingPeers();

        void RequestBlock(uint32_t index, uint32_t begin, uint32_t length);

        [[nodiscard]] std::string const &GetInfoHash() const { return meta_info.info_hash; }

        [[nodiscard]] bencode::Node const &GetMeta() const { return meta_info.dict; }

        [[nodiscard]] size_t GetPeersSize() const { return data_from_tracker ? data_from_tracker.value().peers.size() : 0; }

        [[nodiscard]] const uint8_t *GetMasterPeerKey() const { return master_peer->GetID(); }

        [[nodiscard]] std::shared_ptr<bittorrent::MasterPeer> GetRootPeer() { return master_peer; }

        [[nodiscard]] size_t GetPort() const { return port; }

        [[nodiscard]] bittorrent::Query GetDefaultTrackerQuery() const;

        [[nodiscard]] const bittorrent::Response &GetResponse() const;

        [[nodiscard]] size_t GetPieceCount() const { return GetMeta()["info"]["pieces"].AsString().size() / 20; };

        [[nodiscard]] size_t GetPieceSize() const { return GetMeta()["info"]["piece length"].AsNumber(); };

        [[nodiscard]] size_t GetLastPieceSize() const { return last_piece_size; }

        [[nodiscard]] bool HasTrackers() const { return !active_trackers.empty(); }

    private:
        bool FillTrackers();

        [[nodiscard]] boost::asio::io_service &GetService() const;

        boost::asio::io_service &service; // обязательно в самом верху

        size_t t_uploaded{};

        size_t t_downloaded{};

        size_t t_left{};

        friend class bittorrent::Tracker;
        std::list<std::shared_ptr<bittorrent::Tracker>> active_trackers;

        std::optional<bittorrent::Response> data_from_tracker;

        size_t port;

        meta_info_file meta_info;

        std::shared_ptr<bittorrent::MasterPeer> master_peer;

        size_t last_piece_size{};

        std::shared_ptr<TorrentFilesManager> file_manager;
    };
} // namespace bittorrent

#endif // QTORRENT_TORRENT_H
