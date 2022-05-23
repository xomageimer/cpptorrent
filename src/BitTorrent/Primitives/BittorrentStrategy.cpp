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
    peer->TryToRequestPiece();
}

void bittorrent::BittorrentStrategy::OnInterested(std::shared_ptr<network::PeerClient> peer) {
    if (peer->IsClientChoked() && peer->master_peer_.CanUnchokePeer(peer->GetPeerData().GetIP())) peer->send_unchoke();

    if (!peer->IsRemoteChoked()) {
        peer->TryToRequestPiece();
    }
}

void bittorrent::BittorrentStrategy::OnNotInterested(std::shared_ptr<network::PeerClient> peer) {
    if (peer->IsClientChoked() && !peer->IsRemoteChoked()) {
        peer->send_choke();
        // TODO что-то сделать если задушили пир
    }
}

void bittorrent::BittorrentStrategy::OnHave(std::shared_ptr<network::PeerClient> peer, uint32_t i) {
    if (!peer->PieceDone(i)) {
        peer->TryToRequestPiece();
    }
}

void bittorrent::BittorrentStrategy::OnBitfield(std::shared_ptr<network::PeerClient> peer) {
    if (GetMismatchedBitfield(peer->GetOwnerBitfield(), peer->GetPeerBitfield()).Popcount()) {
        peer->send_interested();
    }
    peer->TryToRequestPiece();
}

void bittorrent::BittorrentStrategy::OnRequest(std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, uint32_t length) {
    LOG(peer->GetStrIP(), " : size of message:", BytesToHumanReadable(length));

    if (peer->PieceDone(index)) {
        peer->BindUpload(peer->GetTorrent().UploadPieceBlock({peer, index, begin, length}));
    }
}

void bittorrent::BittorrentStrategy::OnPieceBlock(
    std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, const uint8_t *data, uint32_t size) {
    if (size > bittorrent_constants::most_request_size) {
       LOG(peer->GetStrIP(), " : ", "more than allowed to get, current message size is ", size,
                     ", but the maximum size can be ", bittorrent_constants::most_request_size);
        return;
    }

    if (!peer->PieceRequested(index)) {
        LOG(peer->GetStrIP(), " : received piece ", index, " which didn't asked");
        return;
    }

    if (peer->PieceDone(index)) {
        LOG(peer->GetStrIP(), " : received piece ", index, " which already done");
        if (peer->active_piece_.has_value() && peer->active_piece_.value().index == index) {
            peer->cancel_piece(index);
            peer->active_piece_.reset();
        }
    } else {
        LOG(peer->GetStrIP(), " : received piece ", index, " was asked early");

        auto &cur_piece = peer->active_piece_.value();

        uint32_t blockIndex = begin / bittorrent_constants::most_request_size;
        cur_piece.blocks[blockIndex] = std::move(Block{data, size, begin});
        cur_piece.cur_pos += size;

        cur_piece.current_blocks_num++;
        LOG (cur_piece.current_blocks_num, " of ", cur_piece.block_count, " blocks added to piece ", cur_piece.index, " from ", peer->GetStrIP());
        if (cur_piece.current_blocks_num == cur_piece.block_count) {
            LOG (cur_piece.index, " piece ready to download", " from ", peer->GetStrIP());
            peer->GetTorrent().DownloadPiece({peer, index, std::move(cur_piece)});

            peer->active_piece_.reset();
            peer->TryToRequestPiece();
        }
    }
}

void bittorrent::BittorrentStrategy::OnCancel(std::shared_ptr<network::PeerClient> peer, uint32_t index, uint32_t begin, uint32_t length) {
    if (peer->PieceUploaded(index)) {
        peer->GetTorrent().CancelBlockUpload({peer, index, begin, length});
    }
}