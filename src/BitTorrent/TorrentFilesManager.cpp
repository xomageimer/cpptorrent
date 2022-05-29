#include "TorrentFilesManager.h"
#include "Torrent.h"

#include "BitTorrent/PeerClient.h"
#include "Primitives/Message.h"

#include "auxiliary.h"

#include <algorithm>
#include <utility>

bittorrent::TorrentFilesManager::TorrentFilesManager(Torrent &torrent, std::filesystem::path path, size_t thread_count)
    : torrent_(torrent), a_worker_(thread_count),
      path_to_download_(std::move(path)) {
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
    //
//        for (size_t i = 0; i < torrent_.GetPieceCount(); i++) {
//            std::cout << "piece id " << i << ":" << std::endl;
//            for (auto &file : pieces_by_files_[i]) {
//                std::cout << file.piece_begin << std::endl;
//            }
//        }

    a_worker_.Start();

    LOG("Torrent pieces count: ", torrent_.GetPieceCount());
    LOG("Torrent total size: ", total_size_GB_, " GB");
    LOG("Last peace size: ", last_piece_size_, " bytes");
}

void bittorrent::TorrentFilesManager::fill_files() {
    auto &file_info = torrent_.GetMeta()["info"];
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
                pieces_by_files_[cur_piece_index].emplace_back(FileInfo{cur_file_path.string(), cur_piece_index, 0, file_beg, bytes_size});
                cur_piece_begin = bytes_size - (piece_length - cur_piece_begin);
                while (cur_piece_begin > piece_length) {
                    cur_piece_begin = cur_piece_begin - piece_length;
                    cur_piece_index++;
                    file_beg += piece_length;
                    pieces_by_files_[cur_piece_index].emplace_back(
                        FileInfo{cur_file_path.string(), cur_piece_index, 0, file_beg, bytes_size});
                }
            }

            files_muts_[cur_file_path];

            total_size_GB_ += BytesToGiga(bytes_size);
        }
        last_piece_size_ = cur_piece_begin;
    }
}

void bittorrent::TorrentFilesManager::WritePieceToFile(Piece piece) {
    ADD_DURATION(write_to_disk);

    auto &files = pieces_by_files_.at(piece.index);

    for (auto &file : files) {
        std::unique_lock lock(files_muts_[file.path]);
        if (!std::filesystem::exists(file.path.parent_path())) {
            std::filesystem::create_directories(file.path.parent_path());
        }
        std::ofstream file_stream(file.path.string(), std::ios::binary | std::ios_base::in | std::ios_base::out);
        if (!file_stream.is_open()) {
            file_stream.open(file.path.string(), std::ios::binary | std::ios_base::out | std::ios_base::trunc);
            file_stream.seekp(file.size, std::ios::beg);
        }

        file_stream.seekp(file.file_begin, std::ios::beg);
        auto cur_size = std::min(piece.size() - file.piece_begin, (unsigned long long)file.size);
        for (size_t i = file.piece_begin; i < cur_size + file.piece_begin; i++) {
            file_stream.write(reinterpret_cast<const char *>(&piece[i]), 1);
        }
    }
}

bittorrent::Block bittorrent::TorrentFilesManager::ReadPieceBlockFromFile(size_t idx, size_t block_beg, size_t length) {
    ADD_DURATION(read_from_disk);

    auto &piece_files = pieces_by_files_.at(idx);
    auto it = std::upper_bound(piece_files.begin(), piece_files.end(), block_beg, [](size_t block_req, const auto & file){
        return block_req < file.piece_begin;
    });
    it = std::prev(it);
    block_beg -= it->piece_begin;

    Block block;
    block.begin_ = block_beg;
    block.data_.reserve(length);
    for (; it != piece_files.end() && length; it++) {
        std::ifstream file_stream(it->path.string(),
            std::ios::binary | std::ios_base::in);
        if (!file_stream.is_open())
            throw std::logic_error("can't open file when read piece block from file!");

        file_stream.seekg(it->file_begin + block_beg, std::ios::beg);
        auto size = std::min(it->size - it->file_begin - block_beg,(unsigned long long)length);
        for (size_t j = 0; j < size; j++){
            file_stream.read(reinterpret_cast<char *>(block.data_.push_back(0), &block.data_.back()), 1);
        }
        length -= size;
        block_beg = 0;
    }
    return std::move(block);
}

bool bittorrent::TorrentFilesManager::ArePieceValid(const WriteRequest &req) {
    auto &pieces_sha1 = torrent_.GetMeta()["info"]["pieces"];
    auto downloaded_sha1 = GetSHA1FromPiece(req.piece);
//    std::cerr << downloaded_sha1 << " ~ " <<std::string(&pieces_sha1.AsString()[req.piece_index * 20], 20) << std::endl;
    return downloaded_sha1 == std::string(&pieces_sha1.AsString()[req.piece_index * 20], 20);
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
        {
            std::lock_guard<std::mutex> lock(manager_mut_);
            if (active_requests_.count(req)) return;
            active_requests_.erase(req);
        }
        auto block = std::move(ReadPieceBlockFromFile(req.piece_index, req.begin, req.length));
        torrent_.GetStrategy()->OnBlockReadyToSend(req.remote_peer, req.piece_index, req.begin, std::move(block));
        req.remote_peer->UnbindUpload(req.piece_index);
    }));
    return it.first->second;
}

void bittorrent::TorrentFilesManager::DownloadPiece(bittorrent::WriteRequest req) {
    a_worker_.Enqueue([this, req = std::move(req)]() mutable {
        LOG(req.piece_index, " piece start download");
        std::sort(
            req.piece.blocks.begin(), req.piece.blocks.end(), [](auto &block1, auto &block2) { return block1.begin_ < block2.begin_; });

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
    torrent_.OnPieceDownloaded(ready_pieces_num_.load()); // TODO сделать всё в таком же стиле!
}