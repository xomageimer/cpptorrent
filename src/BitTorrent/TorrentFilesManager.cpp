#include "TorrentFilesManager.h"
#include "Torrent.h"

#include "PeerClient.h"
#include "Primitives/Message.h"

#include "auxiliary.h"

#include <algorithm>
#include <utility>

bittorrent::TorrentFilesManager::TorrentFilesManager(Torrent &torrent, std::filesystem::path path, size_t thread_count)
    : torrent_(torrent), a_worker_(std::thread::hardware_concurrency() > 2 ? std::thread::hardware_concurrency() / 2 : 1), path_to_download_(std::move(path)) {
    fill_files();

    //    LOG("____________________________________________\n");
    //    for (auto &[piece, files] : pieces_by_files_) {
    //        LOG("piece id ", piece, ":");
    //        size_t i = 0;
    //        for (auto &file : files) {
    //            LOG("\tfile #", i++, " : ", file.path.filename(), "\n\tstarted from ", file.piece_begin,
    //                " piece position"
    //                "\n\tin ",
    //                file.file_begin, " file position");
    //        }
    //        LOG("\n");
    //    }
    //    LOG("____________________________________________");

    a_worker_.Start();

    LOG("Torrent pieces count: ", torrent_.GetPieceCount());
    LOG("Torrent total size: ", total_size_GB_, " GB");
    LOG("Last peace size: ", last_piece_size_, " bytes");
}

void bittorrent::TorrentFilesManager::fill_files() {
    auto & file_info = torrent_.GetMeta()["info"];
    piece_size_ = file_info["piece length"].AsNumber();
    if (file_info.TryAt("length")) { // single-file mode torrent
        auto bytes_size = file_info["length"].AsNumber();
        total_size_GB_ = BytesToGiga(bytes_size);
        long long file_beg = 0;
        for (size_t i = 0; i < torrent_.GetPieceCount(); i++) {
            pieces_by_files_[i].emplace_back(FileInfo{file_info["name"].AsString(), i, file_beg, bytes_size});
            file_beg += piece_size_;
        }
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
            long long file_beg = 0;

            pieces_by_files_[cur_piece_index].emplace_back(
                FileInfo{cur_file_path.string(), cur_piece_index, cur_piece_begin, file_beg, bytes_size});

            cur_piece_index = ((cur_piece_begin + bytes_size) < piece_length) ? cur_piece_index : cur_piece_index + 1;
            if ((cur_piece_begin + bytes_size) < piece_length) {
                cur_piece_begin = cur_piece_begin + bytes_size;
            } else {
                file_beg = piece_length - cur_piece_begin;
                cur_piece_begin = bytes_size - (piece_length - cur_piece_begin);
                while (cur_piece_begin > piece_length) {
                    cur_piece_begin = cur_piece_begin - piece_length;
                    cur_piece_index++;
                    pieces_by_files_[cur_piece_index].emplace_back(
                        FileInfo{cur_file_path.string(), cur_piece_index, 0, file_beg, bytes_size});
                    file_beg += piece_length;
                }
            }

            total_size_GB_ += BytesToGiga(bytes_size);
        }
        last_piece_size_ = cur_piece_begin;
    }
}

void bittorrent::TorrentFilesManager::WritePieceToFile(Piece piece) {
    auto &files = pieces_by_files_.at(piece.index);
    for (auto &file : files) {
        std::unique_lock lock(files_mut_);
        std::ofstream file_stream(file.path.string(), std::ios_base::in | std::ios_base::out | std::ios_base::ate);
        file_stream.seekp(file.file_begin, std::ios::beg);
        for (auto &block : piece.blocks) {
            file_stream.write(reinterpret_cast<const char*>(block.data_.data()), block.data_.size());
        }
    }
}

bool bittorrent::TorrentFilesManager::ArePieceValid(const WriteRequest & req) {
    auto & pieces_sha1 = torrent_.GetMeta()["info"]["pieces"];
    return GetSHA1FromPiece(req.piece) == std::string(&pieces_sha1.AsString()[req.piece_index * 20], 20);
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
        std::cerr << "Start download : " << req.piece_index << std::endl;
        std::sort(req.piece.blocks.begin(), req.piece.blocks.end(), [](auto & block1, auto & block2) {
            return block1.begin_ < block2.begin_;
        });
        if (ArePieceValid(req)) {
            WritePieceToFile(std::move(req.piece));
            SetPiece(req.piece_index);
        } else {
            std::cerr << req.piece_index << " : SHA1 DONT CORRECT" << std::endl;
        }
        req.remote_peer->UnbindRequest(req.piece_index);
    });
}

void bittorrent::TorrentFilesManager::SetPiece(size_t piece_index) {
    ready_pieces_num_++;
    torrent_.SayHave(piece_index);
    torrent_.OnPieceDownloaded(ready_pieces_num_.load());
}