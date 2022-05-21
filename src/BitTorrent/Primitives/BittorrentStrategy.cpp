#include "BittorrentStrategy.h"

#include "random_generator.h"

bittorrent::BittorrentStrategy::BittorrentStrategy() {
    start_ = std::chrono::steady_clock::now();
}

std::optional<size_t> bittorrent::BittorrentStrategy::ChoosePiece(const Peer &peer_owner, const Peer &peer_neigh) {
    auto available_bitfield = GetMismatchedBitfield(peer_owner.GetBitfield(), peer_neigh.GetBitfield());

    if (!available_bitfield.Popcount()) return std::nullopt;

    std::vector<uint32_t> pieces_ids;
    for (size_t i = 0; i < available_bitfield.Size(); i++) {
        if (available_bitfield.Test(i)) pieces_ids.push_back(i);
    }
    return pieces_ids[random_generator::Random().GetNumberBetween<size_t>(0, pieces_ids.size() - 1)];
}

// TODO тут можно реализовать EndGame
void bittorrent::BittorrentStrategy::OnPieceDownloaded(size_t total_piece_count, size_t pieces_already_downloaded, Torrent &torrent) {
    if (total_piece_count == pieces_already_downloaded) {
        LOG("File(s) successfully downloaded in ",
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_).count(), " seconds");
        std::cerr << "File(s) downloaded in "
                  << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_).count() << " seconds"
                  << std::endl;
        return;
    }
    LOG(pieces_already_downloaded, " out of ", total_piece_count, " pieces downloaded after ",
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_).count(), " seconds from starting");
}