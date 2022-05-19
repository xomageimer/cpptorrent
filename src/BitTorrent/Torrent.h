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

#include "Primitives/Piece.h"
#include "Primitives/ManagerRequests.h"

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
        explicit Torrent(boost::asio::io_service &service, std::filesystem::path const &torrent_file_path,
            std::filesystem::path const &path_to_download, size_t listener_port);

        bool TryConnect(bittorrent::Launch policy = bittorrent::Launch::Best, bittorrent::Event event = bittorrent::Event::Empty);

        void StartCommunicatingPeers();

        void DownloadPieceBlock(WriteRequest req);

        void UploadPieceBlock(ReadRequest req);

        void SayHave(size_t piece_num) { master_peer_->SendHaveToAll(piece_num); }

        [[nodiscard]] const bittorrent::Bitfield &GetOwnerBitfield() const { return file_manager_->GetBitfield(); }

        [[nodiscard]] bool PieceDone(uint32_t idx) const { return file_manager_->PieceDone(idx); };

        [[nodiscard]] bool PieceRequested(uint32_t idx) const { return file_manager_->PieceRequested(idx); };

        [[nodiscard]] std::string const &GetInfoHash() const { return meta_info_.info_hash; }

        [[nodiscard]] bencode::Node const &GetMeta() const { return meta_info_.dict; }

        [[nodiscard]] size_t GetPeersSize() const { return data_from_tracker_ ? data_from_tracker_.value().peers.size() : 0; }

        [[nodiscard]] const uint8_t *GetMasterPeerKey() const { return master_peer_->GetID(); }

        [[nodiscard]] std::shared_ptr<bittorrent::MasterPeer> GetRootPeer() { return master_peer_; }

        [[nodiscard]] size_t GetPort() const { return port_; }

        [[nodiscard]] bittorrent::Query GetDefaultTrackerQuery() const;

        [[nodiscard]] const bittorrent::Response &GetResponse() const;

        [[nodiscard]] size_t GetPieceCount() const { return GetMeta()["info"]["pieces"].AsString().size() / 20; };

        [[nodiscard]] size_t GetPieceSize() const { return GetMeta()["info"]["piece length"].AsNumber(); };

        [[nodiscard]] size_t GetLastPieceSize() const { return last_piece_size_; }

        [[nodiscard]] bool HasTrackers() const { return !active_trackers_.empty(); }

        [[nodiscard]] boost::asio::io_service &GetService() const;

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

        std::shared_ptr<bittorrent::MasterPeer> master_peer_;

        size_t last_piece_size_{};

        std::shared_ptr<TorrentFilesManager> file_manager_;
    };
} // namespace bittorrent

#endif // QTORRENT_TORRENT_H
