#ifndef CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
#define CPPTORRENT_TORRENTFILESTRUCTMANAGER_H

#include <unordered_map>
#include <fstream>
#include <filesystem>

// TODO структура отвечает за правильную обработку файлов торрента
namespace bittorrent {
    struct Torrent;
    struct TorrentFilesManager {
    public:
        explicit TorrentFilesManager(Torrent &torrent, std::filesystem::path);

    private:
        const std::filesystem::path path_to_download;
        Torrent &torrent_;

        std::unordered_map<std::string, long double> files;
        long double total_size_GB;
        void fill_files();
    };
} // namespace bittorrent

#endif // CPPTORRENT_TORRENTFILESTRUCTMANAGER_H
