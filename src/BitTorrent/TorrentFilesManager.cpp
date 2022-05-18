#include "TorrentFilesManager.h"
#include "Torrent.h"

#include "auxiliary.h"

#include <utility>

bittorrent::TorrentFilesManager::TorrentFilesManager(Torrent &torrent, std::filesystem::path path)
    : torrent_(torrent), path_to_download_(std::move(path)) {
    fill_files();
    bitset_.Resize(torrent_.GetPieceCount());

    LOG("Torrent pieces count: ", torrent_.GetPieceCount());
    LOG("Torrent total size: ", total_size_GB_, " GB");
    LOG("Last peace size: ", last_piece_size_, " bytes");

    thread();
}

void bittorrent::TorrentFilesManager::thread() {

}

void bittorrent::TorrentFilesManager::fill_files() {
    auto file_info = torrent_.GetMeta()["info"];
    piece_size_ = file_info["piece length"].AsNumber();
    if (file_info.TryAt("length")) {                                                            // single-file mode torrent
        auto bytes_size = file_info["length"].AsNumber();
        total_size_GB_ = BytesToGiga(bytes_size);
        files_.emplace_back(FileInfo{file_info["name"].AsString(), 0, 0, bytes_size});

        last_piece_size_ = bytes_size % file_info["piece length"].AsNumber();
    } else {                                                                                    // multi-file mode torrent
        const long long piece_length = file_info["piece length"].AsNumber();
        total_size_GB_ = 0;
        files_.reserve(file_info["files_"].AsArray().size());

        long long cur_piece_begin = 0;
        size_t cur_piece_index = 0;
        for (auto &file : file_info["files_"].AsArray()) {
            auto cur_file_path = path_to_download_;
            for (auto const &path_el : file["path"].AsArray()) {
                cur_file_path = cur_file_path / path_el.AsString();
            }
            auto bytes_size = file["length"].AsNumber();

            files_.emplace_back(FileInfo{cur_file_path.string(), cur_piece_index, cur_piece_begin, bytes_size});

            cur_piece_index = ((cur_piece_begin + bytes_size) < piece_length) ? cur_piece_index : cur_piece_index + 1;
            if ((cur_piece_begin + bytes_size) < piece_length) {
                cur_piece_begin = cur_piece_begin + bytes_size;
            } else {
                cur_piece_begin = bytes_size - (piece_length - cur_piece_begin);
                while (cur_piece_begin > piece_length) {
                    cur_piece_begin = cur_piece_begin - piece_length;
                    cur_piece_index++;
                }
            }

            total_size_GB_ += BytesToGiga(bytes_size);
        }
        last_piece_size_ = cur_piece_begin;
    }
}

bool bittorrent::TorrentFilesManager::PieceDone(uint32_t index) const {
    return bitset_.Test(index);
}

// TODO мб надо иначе
bool bittorrent::TorrentFilesManager::PieceRequested(uint32_t index) const {
    // TODO нужен мьютекс по верх, глянуть concurrent_map
    return active_pieces_.count(index);
}

void bittorrent::TorrentFilesManager::WritePieceToFile(size_t piece_num) {}

void bittorrent::TorrentFilesManager::UploadBlock(const bittorrent::ReadRequest &) {

}

// TODO хочу где-нибудь здесь conditional_variable! надо думать!

void bittorrent::TorrentFilesManager::DownloadBlock(const bittorrent::WriteRequest &) {
    // TODO если такого нет, то добавить в active_pieces
//    auto &piece_handler = active_pieces_[piece_idx];
//    {
//        std::lock_guard<std::mutex> lock(piece_handler.mut);
//        auto &piece = piece_handler.piece;
//        piece.blocks[index] = std::move(block);
//        ++piece.current_block;
//        if (piece.current_block == piece.block_count) {
            // TODO записываем его в файл и удаляем из active_pieces
//        }
        // TODO устанавливаем в bitset'е его позицию в true
//    }
}
