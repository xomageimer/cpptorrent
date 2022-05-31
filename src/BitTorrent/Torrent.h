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

#include "Primitives/BittorrentStrategy.h"
#include "Primitives/Piece.h"
#include "Primitives/ManagerRequests.h"

#include "bencode_lib.h"
#include "random_generator.h"

#include "profile.h"

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
        explicit Torrent(boost::asio::io_service &service, std::filesystem::path const &torrent_file_path,
            std::filesystem::path const &path_to_download, size_t listener_port,
            std::shared_ptr<BittorrentStrategy> strategy = std::make_shared<BittorrentStrategy>());

        Torrent(const Torrent &) = delete;

        Torrent &operator=(const Torrent &) = delete;

        bool TryConnect(bittorrent::Launch policy = bittorrent::Launch::Best, bittorrent::Event event = bittorrent::Event::Empty);

        void ProcessMeetingPeers();

        [[nodiscard]] size_t DownloadedPieceCount() const { return ready_pieces_num_.load(); }

        void DownloadPiece(WriteRequest req);

        size_t UploadPieceBlock(ReadRequest req);

        void CancelBlockUpload(ReadRequest req);

        void Have(size_t piece_num);

        std::optional<size_t> DetermineNextPiece(const Peer &peer);

        void OnPieceDownloaded(size_t already_downloaded_pieces_count);

        [[nodiscard]] bool CouldReconnect() const;

        [[nodiscard]] size_t ActivePeersCount() const;

        [[nodiscard]] std::string const &GetInfoHash() const { return meta_info_.info_hash; }

        [[nodiscard]] bencode::Node const &GetMeta() const { return meta_info_.dict; }

        [[nodiscard]] size_t GetPeersSize() const { return data_from_tracker_ ? data_from_tracker_.value().peers.size() : 0; }

        [[nodiscard]] const uint8_t *GetMasterPeerKey() const { return master_peer_->GetID(); }

        [[nodiscard]] std::shared_ptr<bittorrent::MasterPeer> GetRootPeer() { return master_peer_; }

        [[nodiscard]] size_t GetPort() const { return port_; }

        [[nodiscard]] bittorrent::Query GetDefaultTrackerQuery() const;

        [[nodiscard]] const bittorrent::Response &GetResponse() const;

        [[nodiscard]] size_t GetPieceCount() const { return GetMeta()["info"]["pieces"].AsString().size() / 20; };

        [[nodiscard]] size_t GetPieceSize() const { return file_manager_->GetPieceSize(); };

        [[nodiscard]] size_t GetLastPieceSize() const { return file_manager_->GetLastPieceSize(); }

        [[nodiscard]] bool HasTrackers() const { return !active_trackers_.empty(); }

        [[nodiscard]] boost::asio::io_service &GetService() const;

        [[nodiscard]] std::shared_ptr<BittorrentStrategy> GetStrategy() { return strategy_; };

    private:
        bool fill_trackers();

        boost::asio::io_service &service_; // обязательно в самом верху

        size_t uploaded_{};

        size_t downloaded_{};

        size_t left_{};

        friend class bittorrent::Tracker;
        std::list<std::shared_ptr<bittorrent::Tracker>> active_trackers_;

        std::optional<bittorrent::Response> data_from_tracker_;

        size_t port_;

        meta_info_file meta_info_;

        std::shared_ptr<BittorrentStrategy> strategy_;

        std::shared_ptr<bittorrent::MasterPeer> master_peer_;

        std::shared_ptr<TorrentFilesManager> file_manager_;

        std::atomic<size_t> ready_pieces_num_ = 0;

        TotalDuration determine_timer {"determine next piece func"};
    };
} // namespace bittorrent

#endif // QTORRENT_TORRENT_H
