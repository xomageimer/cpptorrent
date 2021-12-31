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
    service.reset();
    tracker::Query query {event, t_uploaded, t_downloaded, t_left};

    SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    std::thread t;
    try {
        std::vector<boost::future<tracker::Response>> results;
        for (auto & tracker : active_trackers) {
            results.push_back(tracker->Request(service, query));
        }

        t = std::thread([&]{ SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)); service.run(); });
        switch (policy) {
            case launch::any: {
                struct DoneCheck {
                public:
                    tracker::Response operator()(boost::future<std::vector<boost::future<tracker::Response>>> result_args){
                        auto results = result_args.get();
                        auto any_res = results.begin();
                        if (!any_res->has_exception() || results.empty()) {
                            return std::move(any_res->get());
                        } else {
                            results.erase(results.begin(), results.begin() + 1);
                            return std::move(boost::when_any(results.begin(), results.end())
                                    .then(DoneCheck{*this}).get());
                        }
                    }
                };
                data_from_tracker = boost::when_any(results.begin(), results.end())
                        .then(DoneCheck{}).get();
            }
            case launch::best: {
                data_from_tracker = boost::when_all(results.begin(), results.end())
                        .then([&](boost::future<std::vector<boost::future<tracker::Response>>> ready_results){
                    auto results = ready_results.get();
                    tracker::Response resp;
                    int i = 0;
                    for (auto & el : results){
                        if (el.get().peers.size() > resp.peers.size()) {
                            auto to_swap = std::next(active_trackers.begin(), i);
                            active_trackers.splice(active_trackers.begin(), active_trackers, to_swap);
                            resp = el.get();
                        }
                        i++;
                    }
                    return resp;
                }).get();
            }
        }
        service.stop();
        t.join();

        std::cout << data_from_tracker.complete << std::endl;
        return true;
    } catch (network::BadConnect const &bc) {
        service.stop();
        t.join();

        std::cerr << bc.what() << std::endl;
        return false;
    }
}

bool bittorrent::Torrent::FillTrackers() {
    std::vector<std::shared_ptr<tracker::Tracker>> trackers;
    auto make_tracker = [&](const std::string& announce_url) {
        trackers.emplace_back(std::make_shared<tracker::Tracker>(
                announce_url,
                *this
        ));
        trackers.back()->MakeRequester();
    };

    try {
        if (auto announce_list_opt = GetMeta().TryAt("announce-list"); announce_list_opt) {
            for (auto &announce: announce_list_opt.value().get().AsArray()) {
                if (std::holds_alternative<std::string>(announce))
                    make_tracker(announce.AsString());
                else {
                    for (auto &announce_str: announce.AsArray()) {
                        make_tracker(announce_str.AsString());
                    }
                }
            }
        } else if (auto announce_opt = GetMeta().TryAt("announce"); announce_opt) {
            make_tracker(announce_opt.value().get().AsString());
        } else {
            return false;
        }
    } catch (std::exception & exc) {
        std::cerr << exc.what() << std::endl;
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