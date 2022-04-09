#ifndef CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
#define CPPTORRENT_TORRENTFILESTRUCTMANAGER_H

#include "bt/Bitfield.h"
#include "bt/Piece.h"

#include <queue>
#include <unordered_map>
#include <fstream>
#include <filesystem>

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
        [[nodiscard]] long double GetFilesSize() const { return total_size_GB; }; // GigaBytes
    private:
        Torrent &torrent_;

        // TODO мб queue не подойдет
        std::queue<std::pair<size_t, Piece>> pieces; // TODO не должно хранить все pieces
        long long last_piece_size {};

        const std::filesystem::path path_to_download;
        long double total_size_GB;

        std::vector<FileInfo> files;
        bittorrent::Bitfield bitset_;

        void fill_files();
    };
} // namespace bittorrent

#endif // CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
