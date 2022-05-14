#if defined(WIN) || defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
#define OS_WIN
#endif

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>

#include <random>
#include <sstream>

#include "Torrent.h"
#include "TorrentFilesManager.h"

#include "auxiliary.h"

#include "logger.h"

bittorrent::Torrent::Torrent(boost::asio::io_service &service, std::filesystem::path const &torrent_file_path, std::filesystem::path const &download_path, size_t listener_port)
    : service_(service), port_(listener_port) {
    std::fstream torrent_file(torrent_file_path.c_str(), std::ios::in | std::ios::binary);
    if (!torrent_file.is_open()) {
        throw std::logic_error("can't open file\n");
    }
    auto doc = bencode::Deserialize::Load(torrent_file);
    meta_info_.dict = doc.GetRoot();

    auto &info_hash_bencode = meta_info_.dict["info"];
    std::stringstream hash_to_s;
    bencode::Serialize::MakeSerialize(info_hash_bencode, hash_to_s);
    auto hash_info_str = hash_to_s.str();
    meta_info_.info_hash = GetSHA1(std::string(hash_info_str.begin(), hash_info_str.end()));

    file_manager_ = std::make_shared<TorrentFilesManager>(*this, download_path);
    master_peer_ = std::make_shared<MasterPeer>(*this);

    FillTrackers();
}

bool bittorrent::Torrent::TryConnect(bittorrent::Launch policy, bittorrent::Event event) {
    if (!HasTrackers())
        return false;
    bittorrent::Query query = GetDefaultTrackerQuery();
    query.event = event;

#ifdef OS_WIN
    SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
#endif
    LOG("create requests");
    try {
        std::vector<boost::future<bittorrent::Response>> results;
        for (auto &tracker: active_trackers_) {
            results.push_back(tracker->Request(query));
        }

        LOG("requests were created");
        switch (policy) {
            case Launch::Any: {
                boost::promise<bittorrent::Response> total_res;
                struct DoneCheck {
                private:
                    boost::promise<bittorrent::Response> &result;

                public:
                    explicit DoneCheck(boost::promise<bittorrent::Response> &res) : result(res) {}
                    void operator()(boost::future<std::vector<boost::future<bittorrent::Response>>> result_args) {
                        auto results = result_args.get();
                        auto any_res = std::find_if(results.begin(), results.end(), [](const boost::future<bittorrent::Response> &f) {
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
                data_from_tracker_ = total_res.get_future().get();
                break;
            }
            case Launch::Best: {
                data_from_tracker_ = boost::when_all(std::make_move_iterator(results.begin()), std::make_move_iterator(results.end()))
                                            .then([&](boost::future<std::vector<boost::future<bittorrent::Response>>> ready_results) {
                                                auto results = ready_results.get();
                                                bittorrent::Response resp;
                                                int i = 0, j = 0;
                                                for (auto &el: results) {
                                                    if (!el.has_exception()) {
                                                        auto cur_resp = el.get();
                                                        if (cur_resp.peers.size() > resp.peers.size()) {
                                                            resp = cur_resp;

                                                            auto to_swap = std::next(active_trackers_.begin(), i);
                                                            active_trackers_.splice(active_trackers_.begin(), active_trackers_, to_swap);
                                                        }
                                                    } else j++;
                                                    i++;
                                                }
                                                if (j == results.size())
                                                    throw std::logic_error("All get exceptions ");
                                                return resp;
                                            })
                                            .get();
                break;
            }
        }
        std::cerr << "got tracker" << std::endl;
        for (auto & tracker : active_trackers_) {
            tracker->Stop();
        }

        return true;
    } catch (boost::exception &excp) {
        std::cerr << "got exception" << std::endl;

        std::cerr << boost::diagnostic_information(excp) << std::endl;
        return false;
    }
}

void bittorrent::Torrent::StartCommunicatingPeers() {
    if (!GetPeersSize())
        return;

    LOG ("Get ", GetResponse().peers.size(), " peers");

    std::cout << GetResponse().peers.size() << std::endl;
    master_peer_->InitiateJob(GetService(), GetResponse().peers);
}

void bittorrent::Torrent::ReceivePieceBlock(uint32_t idx, uint32_t begin, Block block) {
    uint32_t blockIndex = begin / bittorrent_constants::most_request_size;
    file_manager_->SetPieceBlock(idx, blockIndex, std::move(block));
}

bittorrent::Query bittorrent::Torrent::GetDefaultTrackerQuery() const {
    return bittorrent::Query{.event = bittorrent::Event::Empty, .uploaded = uploaded_, .downloaded = downloaded_, .left = left_, .compact = true};
}

boost::asio::io_service &bittorrent::Torrent::GetService() const {
    return service_;
}

const bittorrent::Response &bittorrent::Torrent::GetResponse() const {
    if (!data_from_tracker_) {
        throw std::logic_error("Trackers have not yet been polled");
    }
    return *data_from_tracker_;
}

bool bittorrent::Torrent::FillTrackers() {
    std::vector<std::shared_ptr<bittorrent::Tracker>> trackers;
    auto make_tracker = [&](const std::string &announce_url) {
        trackers.emplace_back(std::make_shared<bittorrent::Tracker>(
            announce_url,
            *this));
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
    } catch (std::exception &exc) {
        std::cerr << exc.what() << std::endl;
        return false;
    }

    std::shuffle(trackers.begin(), trackers.end(), random_generator::Random());
    active_trackers_.insert(active_trackers_.end(), std::make_move_iterator(trackers.begin()), std::make_move_iterator(trackers.end()));
    return true;
}