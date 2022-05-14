#ifndef CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
#define CPPTORRENT_TORRENTFILESTRUCTMANAGER_H

#include "bt/Bitfield.h"
#include "bt/Piece.h"

#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <mutex>

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

        void SetPieceBlock(uint32_t piece_idx, uint32_t index, Block block);

        [[nodiscard]] bool PieceDone(uint32_t index) const;

        [[nodiscard]] const bittorrent::Bitfield &GetBitfield() const { return bitset_; }

    private:
        void fill_files();

        Torrent &torrent_;

        std::mutex pieces_lock;
        std::map<size_t, Piece> active_pieces;

        size_t piece_size {};

        size_t last_piece_size{};

        const std::filesystem::path path_to_download;

        long double total_size_GB;

        std::vector<FileInfo> files;

        bittorrent::Bitfield bitset_;
    };
} // namespace bittorrent

#endif // CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
