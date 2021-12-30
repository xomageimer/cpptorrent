#include "Torrent.h"

#include <random>
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

    master_peer = std::make_shared<Peer>();

    FillTrackers();
}

bool bittorrent::Torrent::TryConnect(bittorrent::launch policy, tracker::Event event) {
    if (!HasTrackers())
        return false;

    tracker::Query query {event, t_uploaded, t_downloaded, t_left};

    tracker::Response response;
    for (auto tracker_it = active_trackers.begin();
         tracker_it != active_trackers.end();) {
        try {
            auto tracker = *tracker_it;
            std::cerr << tracker->GetUrl().Host << ": " << std::endl;
            auto cur_resp = tracker->Request(GetService(), query);
            if (response.warning_message)
                std::cerr << response.warning_message.value() << std::endl;

            auto to_swap = tracker_it;
            tracker_it++;
            active_trackers.splice(active_trackers.begin(), active_trackers, to_swap);
            switch (policy) {               // в случае успеха получаем ID и ставим трекер вперед
                case launch::any: {
                    std::cout << cur_resp.tracker_id << std::endl;
                    return true;
                }
                case launch::best: {
                    // TODO сравнение респонсов
                }
            }
        } catch (network::BadConnect const &bc) {
            std::cerr << bc.what() << std::endl;
            tracker_it++;
            continue;
        }
    }
    return false;
}

bool bittorrent::Torrent::FillTrackers() {
    // TODO как-то надо хендлить исключения (если они вообще должны тут быть) (могут быть при emplace_back)
    std::vector<std::shared_ptr<tracker::Tracker>> trackers;
    auto make_tracker = [&](const std::string& announce_url) {
        trackers.emplace_back(std::make_shared<tracker::Tracker>(
                announce_url,
                *this
        ));
    };
    if (auto announce_list_opt = GetMeta().TryAt("announce-list"); announce_list_opt){
        for (auto & announce : announce_list_opt.value().get().AsArray()) {
            if (std::holds_alternative<std::string>(announce))
                make_tracker(announce.AsString());
            else {
                for (auto & announce_str : announce.AsArray()) {
                    make_tracker(announce_str.AsString());
                }
            }
        }
    } else if (auto announce_opt = GetMeta().TryAt("announce"); announce_opt){
        make_tracker(announce_opt.value().get().AsString());
    } else {
        return false;
    }

    std::shuffle(trackers.begin(), trackers.end(), random_generator::Random());
    active_trackers.insert(active_trackers.end(), std::make_move_iterator(trackers.begin()), std::make_move_iterator(trackers.end()));
    return true;
}

std::shared_ptr<bittorrent::Peer> bittorrent::Torrent::GetMasterPeer() const {
    return master_peer;
}

tracker::Query bittorrent::Torrent::GetDefaultTrackerQuery() const {
    return tracker::Query{.event = tracker::Event::Empty, .uploaded = t_uploaded, .downloaded = t_downloaded, .left = t_left};
}

boost::asio::io_service & bittorrent::Torrent::GetService() const {
    return service;
}