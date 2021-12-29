#include "Torrent.h"

#include <sstream>

#include "auxiliary.h"

bittorrent::Torrent::Torrent(std::filesystem::path const & torrent_file_path) {
    std::fstream torrent_file(torrent_file_path.c_str(), std::ios::in | std::ios::binary);
    if (!torrent_file.is_open()) {
        throw std::logic_error("can't open file\n");
    }
    auto doc = bencode::Deserialize::Load(torrent_file);
    meta_info.dict = doc.GetRoot();

    auto& info_hash_bencode = meta_info.dict["info"];
    std::stringstream hash_to_s;
    bencode::Serialize::MakeSerialize(info_hash_bencode, hash_to_s);
    auto hash_info_str = hash_to_s.str();
    meta_info.info_hash = GetSHA1(std::string(hash_info_str.begin(), hash_info_str.end()));

    std::fstream out("aboba.txt", std::ios::out | std::ios::binary);
    out << meta_info.info_hash;
    out.close();

    master_peer = std::make_shared<Peer>();

    FillTrackers();
}

bool bittorrent::Torrent::TryConnect(tracker::Event event) {
    if (!HasTrackers())
        return false;

    tracker::Query query {event, t_uploaded, t_downloaded, t_left};
    tracker::Response response;
    try {
        response = active_trackers.back()->Request(query);
        if (response.warning_message)
            std::cerr << response.warning_message.value() << std::endl;
    } catch (tracker::BadConnect const & bc) {
        std::cerr << bc.what() << std::endl;
        return false;
    }

    std::cout << response.tracker_id << std::endl; // в случае успеха получаем ID
    return true;
}

bool bittorrent::Torrent::FillTrackers() {
    // TODO как-то надо хендлить исключения (если они вообще должны тут быть) (могут быть при emplace_back)
    auto make_tracker = [&](const std::string& announce_url) {
        active_trackers.emplace_back(std::make_shared<tracker::Tracker>(
                announce_url,
                port,
                *this
        ));
    };
    if (auto announce_list_opt = GetMeta().TryAt("announce-list"); announce_list_opt){
//        for (auto & announce : announce_list_opt.value().get().AsArray()) {
            make_tracker(announce_list_opt.value().get()[12][0].AsString());
//        }
    } else if (auto announce_opt = GetMeta().TryAt("announce"); announce_opt){
        make_tracker(announce_opt.value().get().AsString());
    } else {
        return false;
    }
    return true;
}

std::shared_ptr<bittorrent::Peer> bittorrent::Torrent::GetMasterPeer() const {
    return master_peer;
}
