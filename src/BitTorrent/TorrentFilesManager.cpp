#include "TorrentFilesManager.h"
#include "Torrent.h"

#include "auxiliary.h"

#include <utility>

bittorrent::TorrentFilesManager::TorrentFilesManager(Torrent & torrent, std::filesystem::path path) : torrent_(torrent), path_to_download(std::move(path)) {
    fill_files();
    LOG ("Torrent total size: ", total_size_GB, " GB");
}

void bittorrent::TorrentFilesManager::fill_files() {
    auto file_info = torrent_.GetMeta()["info"];
    if (file_info.TryAt("length")) {
        total_size_GB = file_info["length"].AsNumber();
        files.emplace(file_info["name"].AsString(), total_size_GB);
    } else {
        total_size_GB = 0;
        for (auto & file : file_info["files"].AsArray()) {
            auto cur_file_path = path_to_download;
            for (auto const & path_el : file["path"].AsArray()) {
                cur_file_path = cur_file_path / path_el.AsString();
            }
            auto bytes_size = file["length"].AsNumber();

            files.emplace(cur_file_path.string(), bytes_size);
            total_size_GB += BytesToGiga(bytes_size);
        }
    }
}
