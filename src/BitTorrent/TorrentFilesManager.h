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

#include "AsyncWorker.h"
#include "Primitives/ManagerRequests.h"
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

        explicit TorrentFilesManager(Torrent &torrent, std::filesystem::path, size_t thread_count = 1);

        [[nodiscard]] long double GetFilesSize() const { return total_size_GB_; }; // GigaBytes

        [[nodiscard]] size_t GetPieceSize() const { return piece_size_; }

        [[nodiscard]] size_t GetLastPieceSize() const { return last_piece_size_; }

        bool AreCompleted() const;

        void SetPiece(size_t piece_index);

        size_t UploadBlock(ReadRequest);

        void CancelBlock(ReadRequest);

        void DownloadPiece(WriteRequest);

        void WritePieceToFile(Piece piece);

    private:
        void fill_files();

        Torrent &torrent_;

        AsyncWorker a_worker_;

        mutable std::mutex manager_mut_;

        std::map<ReadRequest, size_t> active_requests_; // активные request'ы из a_worker_

        size_t piece_size_{};

        size_t last_piece_size_{};

        const std::filesystem::path path_to_download_;

        long double total_size_GB_;

        std::map<piece_index, std::vector<FileInfo>> pieces_by_files_;

        std::atomic<size_t> ready_pieces_num_ = 0;
    };
} // namespace bittorrent

#endif // CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
