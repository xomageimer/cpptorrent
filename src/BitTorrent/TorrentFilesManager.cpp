#include "TorrentFilesManager.h"
#include "Torrent.h"

#include "auxiliary.h"

#include <algorithm>
#include <utility>

bittorrent::TorrentFilesManager::TorrentFilesManager(Torrent &torrent, std::filesystem::path path, size_t thread_count)
    : torrent_(torrent), a_worker_(thread_count), path_to_download_(std::move(path)) {
    fill_files();
    bitset_.Resize(torrent_.GetPieceCount());

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

size_t bittorrent::TorrentFilesManager::GetPieceSize(size_t idx) {
    if (idx == bitset_.Size() - 1) {
        return last_piece_size_;
    }
    return piece_size_;
}

bool bittorrent::TorrentFilesManager::PieceDone(uint32_t index) const {
    auto lock = std::shared_lock(manager_mut_);
    return bitset_.Test(index);
}

bool bittorrent::TorrentFilesManager::PieceRequested(uint32_t index) const {
    auto lock = std::shared_lock(manager_mut_);
    return active_pieces_.count(index);
}

void bittorrent::TorrentFilesManager::WritePieceToFile(size_t piece_num) {}

void bittorrent::TorrentFilesManager::CancelBlock(ReadRequest req) {
    auto lock = std::unique_lock(manager_mut_);

    if (!active_pieces_.count(req.piece_index)) return;

    auto &piece_handler = active_pieces_.at(req.piece_index);
    auto &cur_piece = piece_handler.piece;
    auto &blocks_id = piece_handler.workers_id_;

    auto block_id = std::tuple{req.remote_peer, req.begin, req.length};
    if (blocks_id.count(block_id)) {
        auto it = std::find_if(cur_piece.blocks.begin(), cur_piece.blocks.end(),
            [&req](const Block &block) { return block.begin_ == req.begin && block.data_.size() == req.length; });
        if (it != cur_piece.blocks.end()) cur_piece.blocks.erase(it);
    }
}

void bittorrent::TorrentFilesManager::UploadBlock(bittorrent::ReadRequest req) {
    a_worker_.Enqueue([this] {

    });
}

void bittorrent::TorrentFilesManager::DownloadBlock(bittorrent::WriteRequest req) {
    auto lock = std::unique_lock(manager_mut_);

    if (!active_pieces_.count(req.piece_index)) {
        active_pieces_.emplace(
            req.piece_index, PieceHandler{Piece{GetPieceSize(req.piece_index),
                                              static_cast<size_t>(std::ceil(static_cast<double>(GetPieceSize(req.piece_index)) /
                                                                            static_cast<double>(bittorrent_constants::most_request_size)))},
                                 {}});
    }

    auto &piece_handler = active_pieces_.at(req.piece_index);
    auto &cur_piece = piece_handler.piece;
    auto &blocks_id = piece_handler.workers_id_;

    uint32_t blockIndex = req.begin / bittorrent_constants::most_request_size;
    cur_piece.blocks.emplace_back(std::move(req.block));
    blocks_id.emplace(req.remote_peer, req.begin, req.block.data_.size());

    cur_piece.current_block++;
    if (cur_piece.current_block == cur_piece.block_count) {
        a_worker_.Enqueue([this, piece_id = req.piece_index] {
            // TODO прочекать валидность куска
            // TODO записываем его в файл и удаляем из active_pieces
            bitset_.Set(piece_id);
        });
    }
}
