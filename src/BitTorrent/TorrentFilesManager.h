#ifndef CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
#define CPPTORRENT_TORRENTFILESTRUCTMANAGER_H

#include <condition_variable>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <mutex>

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
        explicit TorrentFilesManager(Torrent &torrent, std::filesystem::path);

        void WritePieceToFile(size_t piece_num);

        [[nodiscard]] long double GetFilesSize() const { return total_size_GB_; }; // GigaBytes

        void UploadBlock(const ReadRequest &);

        void DownloadBlock(const WriteRequest &);

        [[nodiscard]] bool PieceDone(uint32_t index) const;

        [[nodiscard]] bool PieceRequested(uint32_t index) const;

        [[nodiscard]] const bittorrent::Bitfield &GetBitfield() const { return bitset_; }

    private:
        void thread(); // TODO вызывать этот метод из конструктора!

        void fill_files();

        Torrent &torrent_;

        std::mutex manager_m_;

        std::condition_variable cv_;

        struct PieceHandler {   // TODO нужно написать аналог FileInfo, и также хранить здесь дескриптор файла!
            Piece piece;
            std::mutex mut;
        };
        std::map<size_t, PieceHandler> active_pieces_;

        size_t piece_size_{};

        size_t last_piece_size_{};

        const std::filesystem::path path_to_download_;

        long double total_size_GB_;

        std::map<size_t, std::vector<FileInfo>> pieces_by_files_; // TODO наверное надо сделать map с ключами в виде piece_index, чтобы быстро записывать в файлы!

        bittorrent::Bitfield bitset_;
    };
} // namespace bittorrent

#endif // CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
