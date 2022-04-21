#include "TorrentFilesManager.h"
#include "Torrent.h"

#include "auxiliary.h"

#include <utility>

bittorrent::TorrentFilesManager::TorrentFilesManager(Torrent &torrent, std::filesystem::path path)
    : torrent_(torrent), path_to_download(std::move(path)) {
    fill_files();
    bitset_.Resize(torrent_.GetPieceCount());

    LOG("Torrent pieces count: ", torrent_.GetPieceCount());
    LOG("Torrent total size: ", total_size_GB, " GB");
    LOG("Last peace size: ", last_piece_size, " bytes");
}

void bittorrent::TorrentFilesManager::fill_files() {
    auto file_info = torrent_.GetMeta()["info"];
    piece_size = file_info["piece length"].AsNumber();
    if (file_info.TryAt("length")) {
        auto bytes_size = file_info["length"].AsNumber();
        total_size_GB = BytesToGiga(bytes_size);
        files.emplace_back(FileInfo{file_info["name"].AsString(), 0, 0, bytes_size});

        last_piece_size = bytes_size % file_info["piece length"].AsNumber();
    } else {
        const long long piece_length = file_info["piece length"].AsNumber();
        total_size_GB = 0;
        files.reserve(file_info["files"].AsArray().size());

        long long cur_piece_begin = 0;
        size_t cur_piece_index = 0;
        for (auto &file : file_info["files"].AsArray()) {
            auto cur_file_path = path_to_download;
            for (auto const &path_el : file["path"].AsArray()) {
                cur_file_path = cur_file_path / path_el.AsString();
            }
            auto bytes_size = file["length"].AsNumber();

            files.emplace_back(FileInfo{cur_file_path.string(), cur_piece_index, cur_piece_begin, bytes_size});

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

            total_size_GB += BytesToGiga(bytes_size);
        }
        last_piece_size = cur_piece_begin;
    }
}

void bittorrent::TorrentFilesManager::SetPieceBlock(uint32_t piece_idx, uint32_t index, Block block) {
    std::lock_guard lk(pieces_lock); // TODO мб мьютекс должен быть у каждого piece
    auto & piece = active_pieces[piece_idx];
    piece.blocks[index] = std::move(block);
    ++piece.current_block;
    if (piece.current_block == piece.block_count){
        // TODO записываем его в файл и удаляем из active_pieces
    }
}


bool bittorrent::TorrentFilesManager::PieceDone(uint32_t index) const {
    return bitset_.Test(index);
}
