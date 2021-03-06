#include "BittorrentStrategy.h"

#include "Torrent.h"
#include "Network/BitTorrent/PeerClient.h"
#include "random_generator.h"

std::optional<size_t> bittorrent::BittorrentStrategy::ChoosePiece(MasterPeer &peer_owner, const Peer &peer_neigh) {
    Bitfield available_bitfield;
    {
        auto owner = peer_owner.GetBitfield();
        auto current = peer_neigh.GetBitfield();
        available_bitfield = GetMismatchedBitfield(owner.bits, current.bits);
    }

    if (!available_bitfield.Popcount()) return std::nullopt;

    std::vector<uint32_t> pieces_ids;
    for (size_t i = 0; i < available_bitfield.Size(); i++) {
        if (available_bitfield.Test(i) && !peer_owner.IsPieceRequested(i)) pieces_ids.push_back(i);
    }
    if (pieces_ids.empty()) return std::nullopt;

    auto chosen_piece = pieces_ids[random_generator::Random().GetNumberBetween<size_t>(0, pieces_ids.size() - 1)];
    peer_owner.MarkRequestedPiece(chosen_piece);
    return chosen_piece;
}

void bittorrent::BittorrentStrategy::OnPieceDownloaded(size_t id, Torrent &torrent) {
    std::lock_guard<std::mutex> lock(out_mutex);
    if (!start_) {
        start_ = std::chrono::steady_clock::now();
    }

    size_t total_piece_count = torrent.GetPieceCount();
    size_t pieces_already_downloaded = torrent.DownloadedPieceCount();
    auto percent = (double)pieces_already_downloaded / ((double)total_piece_count / 100.f);
    std::cerr << '\r' << std::setfill(' ') << std::setw(4);
    std::cout << std::fixed << std::setprecision(2) << percent << "%"
              << " downloaded"
              << "(in " << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - *start_).count()
              << " seconds with " << torrent.ActivePeersCount() << " peers-distributors)";
    LOG(pieces_already_downloaded, " out of ", total_piece_count, " pieces downloaded after ",
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - *start_).count(), " seconds from starting");

    if (total_piece_count == pieces_already_downloaded) {
        LOG("File(s) successfully downloaded in ",
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - *start_).count(), " seconds");
        std::cerr << std::endl
                  << "File(s) downloaded in "
                  << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - *start_).count() << " seconds"
                  << std::endl;
    }
}

void bittorrent::BittorrentStrategy::OnChoked(std::shared_ptr<network::PeerClient> peer) {
    peer->Post([=] {
        if (peer->active_piece_.has_value()) {
            peer->UnbindRequest(peer->active_piece_.value().first.index);
            peer->active_piece_.reset();
        }
    });
}

void bittorrent::BittorrentStrategy::OnUnchoked(std::shared_ptr<network::PeerClient> peer) {
    peer->TryToRequestPiece();
}

void bittorrent::BittorrentStrategy::OnInterested(std::shared_ptr<network::PeerClient> peer) {
    if (peer->IsClientChoked() && peer->master_peer_.CanUnchokePeer(peer->GetPeerData().GetIP())) peer->send_unchoke();

    if (!peer->IsRemoteChoked()) {
        peer->TryToRequestPiece();
    }
}

void bittorrent::BittorrentStrategy::OnNotInterested(std::shared_ptr<network::PeerClient> peer) {
    if (!peer->IsClientChoked() && peer->IsRemoteChoked()) {
        peer->send_choke();
    }
}

void bittorrent::BittorrentStrategy::OnHave(std::shared_ptr<network::PeerClient> peer, uint32_t i) {
    if (i >= peer->TotalPiecesCount()) {
        LOG(peer->GetStrIP(), " : get index ", i, ", but pieces count is ", peer->TotalPiecesCount());
        peer->Disconnect();
        return;
    }

    if (!peer->PieceDone(i)) {
        peer->TryToRequestPiece();
    }
}

void bittorrent::BittorrentStrategy::OnBitfield(std::shared_ptr<network::PeerClient> peer) {
    {
        auto owner = peer->master_peer_.GetBitfield();
        auto current = peer->GetPeerBitfield();
        if (GetMismatchedBitfield(owner.bits, current.bits).Popcount()) {
            peer->send_interested();
        }
    }
    peer->TryToRequestPiece();
}

void bittorrent::BittorrentStrategy::OnRequest(std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, uint32_t length) {
    if (index >= peer->TotalPiecesCount()) {
        LOG(peer->GetStrIP(), " : get index ", index, ", but pieces count is ", peer->TotalPiecesCount());
        peer->Disconnect();
        return;
    }

    LOG(peer->GetStrIP(), " : size of message:", BytesToHumanReadable(length));

    if (peer->PieceDone(index)) {
        peer->BindUpload(peer->GetTorrent().UploadPieceBlock({peer, index, begin, length}));
    }
}

void bittorrent::BittorrentStrategy::OnPieceBlock(
    std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, const uint8_t *data, uint32_t size) {
    if (index >= peer->TotalPiecesCount()) {
        LOG(peer->GetStrIP(), " : get index ", index, ", but pieces count is ", peer->TotalPiecesCount());
        peer->Disconnect();
        return;
    }

    if (size > bittorrent_constants::most_request_size) {
        LOG(peer->GetStrIP(), " : ", "more than allowed to get, current message size is ", size, ", but the maximum size can be ",
            bittorrent_constants::most_request_size);
        return;
    }

    if (!peer->PieceRequested(index)) {
        LOG(peer->GetStrIP(), " : received piece ", index, " which didn't asked");
        return;
    }

    if (peer->PieceDone(index)) {
        LOG(peer->GetStrIP(), " : received piece ", index, " which already done");
        peer->cancel_piece(index);
    } else {
        peer->piece_wait_timeout_.cancel();
        LOG(peer->GetStrIP(), " : received piece ", index, " was asked early");

        peer->Post([=] {
            auto &cur_piece = peer->active_piece_.value().first;

            uint32_t blockIndex = begin / bittorrent_constants::most_request_size;
            cur_piece.blocks[blockIndex] = std::move(Block{data, size, begin});

            cur_piece.current_blocks_num++;
            LOG(cur_piece.current_blocks_num, " of ", cur_piece.block_count, " blocks added to piece ", cur_piece.index, " from ",
                peer->GetStrIP());
            if (cur_piece.current_blocks_num == cur_piece.block_count) {
                LOG(cur_piece.index, " piece ready to download", " from ", peer->GetStrIP());

                peer->GetTorrent().DownloadPiece({peer, index, std::move(cur_piece)});
                peer->ClearAndTryRequest();
            } else if (cur_piece.current_blocks_num % bittorrent_constants::REQUEST_MAX_QUEUE_SIZE == 0) {
                peer->TryToRequestPiece();
            }
        });
    }
}

void bittorrent::BittorrentStrategy::OnCancel(std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, uint32_t length) {
    if (index >= peer->TotalPiecesCount()) {
        LOG(peer->GetStrIP(), " : get index ", index, ", but pieces count is ", peer->TotalPiecesCount());
        peer->Disconnect();
        return;
    }

    if (peer->PieceUploaded(index)) {
        peer->GetTorrent().CancelBlockUpload({peer, index, begin, length});
    }
}

void bittorrent::BittorrentStrategy::OnBlockReadyToSend(
    std::shared_ptr<network::PeerClient> peer, uint32_t pieceIdx, uint32_t offset, bittorrent::Block block) {
    if (pieceIdx >= peer->TotalPiecesCount()) {
        LOG(peer->GetStrIP(), " : get index ", pieceIdx, ", but pieces count is ", peer->TotalPiecesCount());
        peer->Disconnect();
        return;
    }

    LOG(peer->GetStrIP(), " make piece : ", pieceIdx, ", which block started from ", offset, "; ready to send");
    peer->send_block(pieceIdx, offset, block.data_.data(), block.data_.size());
}

void bittorrent::OptimalStrategy::OnPieceDownloaded(size_t id, bittorrent::Torrent &torrent) {
    {
        std::lock_guard<std::mutex> lock(out_mutex);
        if (!end_game_.load()) {
            size_t total_piece_count = torrent.GetPieceCount();
            size_t pieces_already_downloaded = torrent.DownloadedPieceCount();
            auto percent = (double)pieces_already_downloaded / ((double)total_piece_count / 100.f);
            if (percent >= bittorrent_constants::END_GAME_STARTED_FROM) {
                end_game_.store(true);
                std::cerr << "end game started" << std::endl;
            }
        } else {
            torrent.GetRootPeer()->CancelPiece(id);
        }
    }
    BittorrentStrategy::OnPieceDownloaded(id, torrent);
}

std::optional<size_t> bittorrent::OptimalStrategy::ChoosePiece(bittorrent::MasterPeer &peer_owner, const bittorrent::Peer &peer_neigh) {
    Bitfield available_bitfield;
    {
        auto owner = peer_owner.GetBitfield();
        auto current = peer_neigh.GetBitfield();
        available_bitfield = GetMismatchedBitfield(owner.bits, current.bits);
    }

    if (!available_bitfield.Popcount()) return std::nullopt;

    std::vector<uint32_t> pieces_ids;
    for (size_t i = 0; i < available_bitfield.Size(); i++) {
        if (available_bitfield.Test(i) && (end_game_.load() || !peer_owner.IsPieceRequested(i))) pieces_ids.push_back(i);
    }
    if (pieces_ids.empty()) return std::nullopt;

    auto chosen_piece = pieces_ids[random_generator::Random().GetNumberBetween<size_t>(0, pieces_ids.size() - 1)];

    if (!end_game_.load()) peer_owner.MarkRequestedPiece(chosen_piece);

    return chosen_piece;
}

void bittorrent::OptimalStrategy::OnPieceBlock(
    std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, const uint8_t *data, uint32_t size) {
    if (index >= peer->TotalPiecesCount()) {
        LOG(peer->GetStrIP(), " : get index ", index, ", but pieces count is ", peer->TotalPiecesCount());
        peer->Disconnect();
        return;
    }

    if (size > bittorrent_constants::most_request_size) {
        LOG(peer->GetStrIP(), " : ", "more than allowed to get, current message size is ", size, ", but the maximum size can be ",
            bittorrent_constants::most_request_size);
        return;
    }

    if (!end_game_.load() && !peer->PieceRequested(index)) {
        LOG(peer->GetStrIP(), " : received piece ", index, " which didn't asked");
        return;
    }

    if (peer->PieceDone(index)) {
        LOG(peer->GetStrIP(), " : received piece ", index, " which already done");
        peer->cancel_piece(index);
    } else {
        peer->piece_wait_timeout_.cancel();
        LOG(peer->GetStrIP(), " : received piece ", index, " was asked early");

        peer->Post([=] {
            auto &cur_piece = peer->active_piece_.value().first;

            uint32_t blockIndex = begin / bittorrent_constants::most_request_size;
            cur_piece.blocks[blockIndex] = std::move(Block{data, size, begin});

            cur_piece.current_blocks_num++;
            LOG(cur_piece.current_blocks_num, " of ", cur_piece.block_count, " blocks added to piece ", cur_piece.index, " from ",
                peer->GetStrIP());
            if (cur_piece.current_blocks_num == cur_piece.block_count) {
                LOG(cur_piece.index, " piece ready to download", " from ", peer->GetStrIP());

                peer->GetTorrent().DownloadPiece({peer, index, std::move(cur_piece)});
                peer->ClearAndTryRequest();
            } else if (cur_piece.current_blocks_num % bittorrent_constants::REQUEST_MAX_QUEUE_SIZE == 0) {
                peer->TryToRequestPiece();
            }
        });
    }
}
