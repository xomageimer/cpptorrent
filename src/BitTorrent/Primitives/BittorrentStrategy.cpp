#include "BittorrentStrategy.h"

#include "Torrent.h"
#include "PeerClient.h"
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

void bittorrent::BittorrentStrategy::OnUnchoked(std::shared_ptr<network::PeerClient> peer) {
    if (peer->IsClientChoked()) peer->send_unchoke();
    peer->TryToRequest();
}

void bittorrent::BittorrentStrategy::OnInterested(std::shared_ptr<network::PeerClient> peer) {
    if (peer->IsClientChoked() && peer->master_peer_.CanUnchokePeer(peer->GetPeerData().GetIP())) peer->send_unchoke();

    if (!peer->IsRemoteChoked()) {
        peer->TryToRequest();
    }
}

void bittorrent::BittorrentStrategy::OnNotInterested(std::shared_ptr<network::PeerClient> peer) {
    if (!peer->IsClientChoked()) {
        peer->send_choke();
        // TODO что-то сделать если задушили пир
    }
}

void bittorrent::BittorrentStrategy::OnHave(std::shared_ptr<network::PeerClient> peer, uint32_t i) {
    if (!peer->PieceDone(i)) {
        peer->TryToRequest();
    }
}

void bittorrent::BittorrentStrategy::OnBitfield(std::shared_ptr<network::PeerClient> peer) {
    if (GetMismatchedBitfield(peer->GetOwnerBitfield(), peer->GetPeerBitfield()).Popcount()) {
        peer->send_interested();
    } else peer->TryToRequest();
}

void bittorrent::BittorrentStrategy::OnRequest(std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, uint32_t length) {
    LOG(peer->GetStrIP(), " : size of message:", BytesToHumanReadable(length));

    if (peer->PieceDone(index)) {
        peer->BindUpload(peer->GetTorrent().UploadPieceBlock({peer, index, begin, length}));
    }
}

void bittorrent::BittorrentStrategy::OnPieceBlock(
    std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, uint8_t *data, uint32_t size) {
    if (!peer->PieceRequested(index)) {
        LOG(peer->GetStrIP(), " : received piece ", std::to_string(index), " which didn't asked");
        return;
    }

    if (peer->PieceDone(index)) {
        LOG(peer->GetStrIP(), " : received piece ", std::to_string(index), " which already done");
        peer->cancel_piece(index);
        peer->active_pieces_.erase(index);
    } else {
        auto &cur_piece = peer->active_pieces_.at(index);

        uint32_t blockIndex = begin / bittorrent_constants::most_request_size;
        cur_piece.blocks[blockIndex] = std::move(Block{data, size, begin});
        cur_piece.cur_pos += size;

        cur_piece.current_blocks_num++;
        if (cur_piece.current_blocks_num == cur_piece.block_count) {
            peer->GetTorrent().DownloadPiece({peer, index, std::move(cur_piece)});
            peer->active_pieces_.erase(index);
        } else {
            auto first_block_size =
                std::min((cur_piece.index == peer->TotalPiecesCount() - 1 ? peer->GetTorrent().GetLastPieceSize() : peer->GetTorrent().GetPieceSize()) -
                             cur_piece.cur_pos,
                    bittorrent_constants::most_request_size);
            peer->send_request(cur_piece.index, cur_piece.cur_pos, first_block_size);
        }
    }
}

void bittorrent::BittorrentStrategy::OnCancel(std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, uint32_t length) {
    if (peer->PieceUploaded(index)) {
        peer->GetTorrent().CancelBlockUpload({peer, index, begin, length});
    }
}