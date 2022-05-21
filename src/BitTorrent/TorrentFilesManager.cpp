#include "TorrentFilesManager.h"
#include "Torrent.h"

#include "PeerClient.h"
#include "Primitives/Message.h"

#include "auxiliary.h"

#include <algorithm>
#include <utility>

bittorrent::TorrentFilesManager::TorrentFilesManager(Torrent &torrent, std::filesystem::path path, size_t thread_count)
    : torrent_(torrent), a_worker_(thread_count), path_to_download_(std::move(path)) {
    fill_files();

    //    LOG("____________________________________________\n");
    //    for (auto & [piece, files] : pieces_by_files_) {
    //        LOG("piece id " , piece , ":");
    //        size_t i = 0;
    //        for (auto & file : files) {
    //            LOG("\tfile #", i++ , " : " , file.path.filename());
    //            LOG("\tstarted from " , file.begin , " piece position");
    //        }
    //        LOG ("\n");
    //    }
    //    LOG("____________________________________________");

    a_worker_.Start();

    LOG("Torrent pieces count: ", torrent_.GetPieceCount());
    LOG("Torrent total size: ", total_size_GB_, " GB");
    LOG("Last peace size: ", last_piece_size_, " bytes");
}

void bittorrent::TorrentFilesManager::fill_files() {
    auto file_info = torrent_.GetMeta()["info"];
    piece_size_ = file_info["piece length"].AsNumber();
    if (file_info.TryAt("length")) { // single-file mode torrent
        auto bytes_size = file_info["length"].AsNumber();
        total_size_GB_ = BytesToGiga(bytes_size);
        pieces_by_files_[0].emplace_back(FileInfo{file_info["name"].AsString(), 0, 0, bytes_size});

        last_piece_size_ = bytes_size % file_info["piece length"].AsNumber();
    } else { // multi-file mode torrent
        const long long piece_length = file_info["piece length"].AsNumber();
        total_size_GB_ = 0;

        long long cur_piece_begin = 0;
        size_t cur_piece_index = 0;
        for (auto &file : file_info["files"].AsArray()) {
            auto cur_file_path = path_to_download_;
            for (auto const &path_el : file["path"].AsArray()) {
                cur_file_path = cur_file_path / path_el.AsString();
            }
            auto bytes_size = file["length"].AsNumber();

            pieces_by_files_[cur_piece_index].emplace_back(FileInfo{cur_file_path.string(), cur_piece_index, cur_piece_begin, bytes_size});

            cur_piece_index = ((cur_piece_begin + bytes_size) < piece_length) ? cur_piece_index : cur_piece_index + 1;
            if ((cur_piece_begin + bytes_size) < piece_length) {
                cur_piece_begin = cur_piece_begin + bytes_size;
            } else {
                cur_piece_begin = bytes_size - (piece_length - cur_piece_begin);
                while (cur_piece_begin > piece_length) {
                    cur_piece_begin = cur_piece_begin - piece_length;
                    cur_piece_index++;
                    pieces_by_files_[cur_piece_index].emplace_back(FileInfo{cur_file_path.string(), cur_piece_index, 0, bytes_size});
                }
            }

            total_size_GB_ += BytesToGiga(bytes_size);
        }
        last_piece_size_ = cur_piece_begin;
    }
}

void bittorrent::TorrentFilesManager::WritePieceToFile(Piece piece) {
    // TODO записываем его в файл и удаляем из active_pieces
}

void bittorrent::TorrentFilesManager::CancelBlock(ReadRequest req) {
    auto lock = std::lock_guard(manager_mut_);
    if (active_requests_.count(req)) {
        a_worker_.Erase(active_requests_.at(req));
        active_requests_.erase(req);
        req.remote_peer->UnbindUpload(req.piece_index);
    }
}

size_t bittorrent::TorrentFilesManager::UploadBlock(bittorrent::ReadRequest req) {
    auto lock = std::lock_guard(manager_mut_);
    auto it = active_requests_.emplace(req, a_worker_.Enqueue([this, req = std::move(req)]() mutable {
        active_requests_.erase(req);
        req.remote_peer->UnbindUpload(req.piece_index);

        // TODO make upload
    }));
    return it.first->second;
}

void bittorrent::TorrentFilesManager::DownloadPiece(bittorrent::WriteRequest req) {
    a_worker_.Enqueue([this, req = std::move(req)]() mutable {
        // TODO прочекать валидность куска
        WritePieceToFile(std::move(req.piece));
        SetPiece(req.piece_index);
    });
}

void bittorrent::TorrentFilesManager::SetPiece(size_t piece_index) {
    ready_pieces_num_++;
    torrent_.SayHave(piece_index);
    torrent_.OnPieceDownloaded(ready_pieces_num_.load());
}
