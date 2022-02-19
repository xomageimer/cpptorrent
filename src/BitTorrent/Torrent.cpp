#if defined(WIN) || defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
#define OS_WIN
#endif

#include <random>
#include <sstream>

#include "Torrent.h"

#include "auxiliary.h"
#include "PortChecker.h"

bittorrent::Torrent::Torrent(std::filesystem::path const & torrent_file_path) {
    network::PortChecker pc(GetService());
    auto new_port = pc(port, max_port_number);
    if (!new_port) {
        throw std::logic_error("Ports from 6881 to 6889 are busy!\n");
    }
    port = new_port.value();

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

    master_peer = std::make_shared<MasterPeer>(*this);

    FillTrackers();
}

bool bittorrent::Torrent::TryConnect(bittorrent::launch policy, tracker::Event event) {
    if (!HasTrackers())
        return false;
    service.reset();
    tracker::Query query {event, t_uploaded, t_downloaded, t_left};

#ifdef OS_WIN
    SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
#endif
    std::thread t;
    try {
        std::vector<boost::future<tracker::Response>> results;
        for (auto & tracker : active_trackers) {
            results.push_back(tracker->Request(service, query));
        }

        t = std::thread([&]{
#ifdef OS_WIN
            SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
#endif
            service.run();
            std::cout << "service stopped" << std::endl;
        });
        switch (policy) {
            case launch::any: {
                boost::promise<tracker::Response> total_res;
                struct DoneCheck {
                private:
                    boost::promise<tracker::Response> & result;
                public:
                    explicit DoneCheck(boost::promise<tracker::Response> & res) : result(res) {}
                    void operator()(boost::future<std::vector<boost::future<tracker::Response>>> result_args) {
                        auto results = result_args.get();
                        auto any_res = std::find_if(results.begin(), results.end(), [](const boost::future<tracker::Response>& f)
                        {
                            return f.is_ready();
                        });
                        if (!any_res->has_exception()) {
                            result.set_value(any_res->get());
                        } else {
                            results.erase(any_res);
                            if (!results.empty())
                                boost::when_any(results.begin(), results.end())
                                        .then(std::move(*this));
                            else {
                                result.set_exception(any_res->get_exception_ptr());
                            }
                        }
                    }
                };
                boost::when_any(std::make_move_iterator(results.begin()), std::make_move_iterator(results.end()))
                        .then(DoneCheck{total_res});
                data_from_tracker = total_res.get_future().get();
                break;
            }
            case launch::best: {
                data_from_tracker = boost::when_all(std::make_move_iterator(results.begin()), std::make_move_iterator(results.end()))
                        .then([&](boost::future<std::vector<boost::future<tracker::Response>>> ready_results){
                    auto results = ready_results.get();
                    tracker::Response resp;
                    int i = 0;
                    for (auto & el : results){
                        if (!el.has_exception()) {
                            auto cur_resp = el.get();
                            if (cur_resp.peers.size() > resp.peers.size()) {
                                resp = cur_resp;

                                auto to_swap = std::next(active_trackers.begin(), i);
                                active_trackers.splice(active_trackers.begin(), active_trackers, to_swap);
                            }
                        }
                        i++;
                    }
                    return resp;
                }).get();
                break;
            }
        }
        service.stop();
        t.join();

        return true;
    } catch (boost::exception & excp) {
        service.stop();
        if (t.joinable())
            t.join();

        std::cerr << boost::diagnostic_information(excp) << std::endl;
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

tracker::Query bittorrent::Torrent::GetDefaultTrackerQuery() const {
    return tracker::Query{.event = tracker::Event::Empty, .uploaded = t_uploaded, .downloaded = t_downloaded, .left = t_left};
}

boost::asio::io_service & bittorrent::Torrent::GetService() const {
    return service;
}

const tracker::Response & bittorrent::Torrent::GetResponse() const {
    return data_from_tracker;
}
