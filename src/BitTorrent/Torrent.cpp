#if defined(WIN) || defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
#define OS_WIN
#endif

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>

#include <sstream>
#include <utility>

#include "Torrent.h"
#include "TorrentFilesManager.h"

#include "auxiliary.h"

#include "logger.h"

bittorrent::Torrent::Torrent(boost::asio::io_service &service, std::filesystem::path const &torrent_file_path,
    std::filesystem::path const &download_path, size_t listener_port, bool is_completed, std::shared_ptr<BittorrentStrategy> strategy)
    : service_(service), port_(listener_port), strategy_(std::move(strategy)) {
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

    std::string backup_name = "Noname";
    auto pos = torrent_file_path.filename().string().find_last_of('.');
    if (pos != std::string::npos) backup_name = torrent_file_path.filename().string().substr(0, pos);
    auto name_value = GetMeta()["info"].TryAt("name");

    auto directory_name =
        (name_value.has_value() && meta_info_.dict["info"].TryAt("files")) ? name_value.value().get().AsString() : backup_name;
    std::filesystem::create_directories(download_path / directory_name);

    file_manager_ = std::make_shared<TorrentFilesManager>(*this, download_path / directory_name,
        std::thread::hardware_concurrency() > 2 ? std::thread::hardware_concurrency() / 2 : 1); // ������� �����, ����� ������ ���

    master_peer_ = std::make_shared<MasterPeer>(*this);
    boost::asio::ip::udp::resolver   resolver(service_);
    boost::asio::ip::udp::resolver::query query( boost::asio::ip::udp::v4(), "google.com", "");
    boost::asio::ip::udp::resolver::iterator endpoints = resolver.resolve(query);
    boost::asio::ip::udp::endpoint ep = *endpoints;
    boost::asio::ip::udp::socket socket(service_);
    socket.connect(ep);
    master_peer_->ip = IpToInt(socket.local_endpoint().address().to_string());
    if (is_completed) {
        auto ready_pieces = master_peer_->GetBitfield();
        for (size_t i = 0; i < ready_pieces.bits.Size(); ++i) {
            ready_pieces.bits.Set(i);
        }
        ready_pieces_num_ = ready_pieces.bits.Size();
    }

    fill_trackers();
}

bool bittorrent::Torrent::TryConnect(bittorrent::Launch policy, bittorrent::Event event) {
    if (!HasTrackers()) return false;

    bittorrent::Query query = GetDefaultTrackerQuery();
    query.event = event;


//    bittorrent::Peer peer {86198815, 6881};
//    data_from_tracker_.emplace();
//    data_from_tracker_->peers.push_back({peer, {}});
//    file_manager_->Process();
//    return true;


#ifdef OS_WIN
    SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
#endif
    LOG("create requests");
    try {
        std::vector<boost::future<bittorrent::Response>> results;
        for (auto &tracker : active_trackers_) {
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
                        auto any_res = std::find_if(
                            results.begin(), results.end(), [](const boost::future<bittorrent::Response> &f) { return f.is_ready(); });
                        if (!any_res->has_exception()) {
                            result.set_value(any_res->get());
                        } else {
                            results.erase(any_res);
                            if (!results.empty())
                                boost::when_any(results.begin(), results.end()).then(std::move(*this));
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
                                             for (auto &el : results) {
                                                 if (!el.has_exception()) {
                                                     auto cur_resp = el.get();
                                                     if (cur_resp.peers.size() > resp.peers.size()) {
                                                         resp = cur_resp;

                                                         auto to_swap = std::next(active_trackers_.begin(), i);
                                                         active_trackers_.splice(active_trackers_.begin(), active_trackers_, to_swap);
                                                     }
                                                 } else
                                                     j++;
                                                 i++;
                                             }
                                             if (j == results.size()) throw std::logic_error("All get exceptions ");
                                             return resp;
                                         })
                                         .get();
                break;
            }
        }
        std::cerr << "got tracker" << std::endl;
        for (auto &tracker : active_trackers_) {
            tracker->Stop();
        }
        file_manager_->Process();

        std::cerr << "Get peers " << GetResponse().peers.size() << std::endl;

        return true;
    } catch (boost::exception &excp) {
        std::cerr << "got exception" << std::endl;

        std::cerr << boost::diagnostic_information(excp) << std::endl;
        return false;
    }
}

bool bittorrent::Torrent::TryConnectDHT() {
    if (!dht_) return false;
    return true;
}

bool bittorrent::Torrent::CouldReconnect() const {
//    return false;
    for (auto &tracker : active_trackers_) {
        if (tracker && tracker->CouldConnect()) return true;
    }
    return false;
}

void bittorrent::Torrent::ProcessMeetingPeers() {
    if (!GetPeersSize()) return;

    LOG("Get ", GetResponse().peers.size(), " peers");

    master_peer_->InitiateJob(GetService(), GetResponse().peers);
}

void bittorrent::Torrent::DownloadPiece(WriteRequest req) {
    file_manager_->DownloadPiece(std::move(req));
}

size_t bittorrent::Torrent::UploadPieceBlock(ReadRequest req) {
    return file_manager_->UploadBlock(std::move(req));
}

void bittorrent::Torrent::CancelBlockUpload(ReadRequest req) {
    file_manager_->CancelBlock(std::move(req));
}

bittorrent::Query bittorrent::Torrent::GetDefaultTrackerQuery() const {
    return bittorrent::Query{
        .event = bittorrent::Event::Empty, .uploaded = uploaded_, .downloaded = downloaded_, .left = left_, .compact = true};
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

bool bittorrent::Torrent::fill_trackers() {
    std::vector<std::shared_ptr<bittorrent::Tracker>> trackers;
    auto make_tracker = [&](const std::string &announce_url) {
        trackers.emplace_back(std::make_shared<bittorrent::Tracker>(announce_url, *this));
    };

    try {
        if (auto announce_list_opt = GetMeta().TryAt("announce-list"); announce_list_opt) {
            for (auto &announce : announce_list_opt.value().get().AsArray()) {
                if (std::holds_alternative<std::string>(announce))
                    make_tracker(announce.AsString());
                else {
                    for (auto &announce_str : announce.AsArray()) {
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

std::optional<size_t> bittorrent::Torrent::DetermineNextPiece(const Peer &peer) {
    ADD_DURATION(determine_timer);
    auto val = strategy_->ChoosePiece(*master_peer_, peer);
    return std::move(val);
}

void bittorrent::Torrent::OnPieceDownloaded(size_t id) {
    ready_pieces_num_++;
    strategy_->OnPieceDownloaded(id, *this);
}

void bittorrent::Torrent::Have(size_t piece_num) {
    master_peer_->GetBitfield().bits.Set(piece_num);
    master_peer_->SendHaveToAll(piece_num);
}

size_t bittorrent::Torrent::ActivePeersCount() const {
    return master_peer_->DistributorsCount();
}