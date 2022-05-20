#ifndef CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
#define CPPTORRENT_TORRENTFILESTRUCTMANAGER_H

#include <condition_variable>
#include <vector>
#include <set>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <thread>
#include <shared_mutex>
#include <mutex>

#include "Primitives/AsyncWorker.h"
#include "Primitives/ManagerRequests.h"
#include "Primitives/Bitfield.h"
#include "Primitives/Piece.h"

// TODO структура отвечает за правильную обработку файлов торрента
namespace bittorrent {
    struct Torrent;

    struct FileInfo {
        std::filesystem::path path;
        size_t piece_index;
        long long begin;
        long long size;
    };

    struct TorrentFilesManager {
    public:
        using piece_index = size_t;
        using block_worker_id = std::tuple<std::shared_ptr<network::PeerClient>, size_t, size_t>;

        explicit TorrentFilesManager(Torrent &torrent, std::filesystem::path, size_t thread_count = 1);

        [[nodiscard]] long double GetFilesSize() const { return total_size_GB_; }; // GigaBytes

        size_t GetPieceSize(size_t idx);

        void UploadBlock(ReadRequest);

        void CancelBlock(ReadRequest);

        void DownloadBlock(WriteRequest);

        void WritePieceToFile(size_t piece_num);

        [[nodiscard]] bool PieceDone(uint32_t index) const;

        [[nodiscard]] bool PieceRequested(uint32_t index) const;

        [[nodiscard]] const bittorrent::Bitfield &GetBitfield() const { return bitset_; }

    private:
        void fill_files();

        Torrent &torrent_;

        AsyncWorker a_worker_;

        mutable std::shared_mutex manager_mut_;

        struct PieceHandler {
            Piece piece;
            std::set<block_worker_id> workers_id_;
        };
        std::map<piece_index, PieceHandler> active_pieces_;

        size_t max_active_pieces_ = 13;

        size_t piece_size_{};

        size_t last_piece_size_{};

        const std::filesystem::path path_to_download_;

        long double total_size_GB_;

        std::map<piece_index, std::vector<FileInfo>> pieces_by_files_;

        bittorrent::Bitfield bitset_;
    };
} // namespace bittorrent

#endif // CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
