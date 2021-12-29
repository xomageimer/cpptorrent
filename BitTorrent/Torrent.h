#ifndef QTORRENT_TORRENT_H
#define QTORRENT_TORRENT_H

#include <string>
#include <memory>
#include <fstream>
#include <filesystem>
#include <queue>
#include <utility>
#include <thread>

#include <boost/asio.hpp>

#include "bencode_lib.h"
#include "Tracker.h"
#include "random_generator.h"

// Объект с котором работает клиентский код, следовательно -> поменьше исключений | обрабатывать исключения

namespace bittorrent {
    struct meta_info_file {
        std::string info_hash;
        bencode::Node dict;
    };

    struct Torrent;
    template <typename Iter>
    struct AnnounceIterator {
    public:
        explicit AnnounceIterator(boost::asio::io_service & service, Iter iterator, tracker::Query query) : m_service(service), m_iterator(iterator), m_query(std::move(query)) {}
        bool TryConnect(tracker::Event event = tracker::Event::Empty);
        bool operator!=(AnnounceIterator<Iter> const& other) const { return m_iterator != other.m_iterator; }
        bool operator==(AnnounceIterator<Iter> const& other) const { return m_iterator == other.m_iterator; }
        AnnounceIterator<Iter> operator*() const { return *this;}
        AnnounceIterator<Iter>& operator++() { m_iterator++; return *this;}
    private:
        boost::asio::io_service & m_service;
        Iter m_iterator;
        tracker::Query m_query;
    };
    struct AnnouncesRange {
        explicit AnnouncesRange(Torrent & torrent) : m_torrent(torrent) {
//            std::thread([&] {service.run();} ).detach();
        };
        [[nodiscard]] AnnounceIterator<std::vector<std::shared_ptr<tracker::Tracker>>::const_iterator> begin() const;
        [[nodiscard]] AnnounceIterator<std::vector<std::shared_ptr<tracker::Tracker>>::const_iterator> end() const;
    private:
        Torrent & m_torrent; // TODO пусть io_service живет тут и до конца используется в отдельном потоке (а подключения все асинхронные, чтобы быстрее работало)
        mutable boost::asio::io_service service;
    };

    struct Torrent {
    public:
        explicit Torrent(std::filesystem::path const & torrent_file_path);
        bool SyncConnect(tracker::Event event = tracker::Event::Empty);

        [[nodiscard]] std::string const & GetInfoHash() const { return meta_info.info_hash;}
        [[nodiscard]] bencode::Node const & GetMeta() const { return meta_info.dict;}
        [[nodiscard]] size_t GetPort() const { return port; }
        [[nodiscard]] tracker::Query GetDefaultTrackerQuery() const;
        [[nodiscard]] std::shared_ptr<bittorrent::Peer> GetMasterPeer() const;

        [[nodiscard]] bool HasTrackers() const { return !active_trackers.empty(); }
    private:
        size_t t_uploaded {};
        size_t t_downloaded {};
        size_t t_left {};

        friend class AnnouncesRange;
        std::vector<std::shared_ptr<tracker::Tracker>> active_trackers;
        size_t port = 6881;                    // TODO надо иначе хендлить и создавать порты
        meta_info_file meta_info;
        std::shared_ptr<bittorrent::Peer> master_peer;
    private:
        bool FillTrackers();
    };

    template <typename T>
    bool bittorrent::AnnounceIterator<T>::TryConnect(tracker::Event event) {
        m_query.event = event;
        tracker::Response response;
        auto & tracker = *m_iterator;
        std::cerr << tracker->GetUrl().Host << ": " << std::endl;
        try {
            response = tracker->Request(m_service, m_query);
            if (response.warning_message)
                std::cerr << response.warning_message.value() << std::endl;
        } catch (tracker::BadConnect & bc) {
            std::cerr << bc.what() << std::endl;
            return false;
        }

        std::cerr << "Connected!" << std::endl;
        return true;
    }
}


#endif //QTORRENT_TORRENT_H
