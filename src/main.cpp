#define TORRENT_RELEASE
#define TORRENT_DEBUG
#ifdef TORRENT_RELEASE
#undef TORRENT_DEBUG
#endif

#include "Tracker.h"
#include "Torrent.h"

#include "logger.h"

using namespace std;

// TODO добавить менеджера кт будет взаимодействовать с данными из какого-нибудь файлика с информацией о том какие файлы есть у нас для раздачи (сидироваения) и какие мы докачиваем!
int main() {
    LOG ("Start");
    auto start = std::chrono::steady_clock::now();
    try {
        bittorrent::Torrent torrent(std::filesystem::current_path() / "total-war-warhammer-2.torrent"); // TODO config from console
        if (!torrent.TryConnect(bittorrent::launch::any,
                                tracker::Event::Empty))    // TODO сначала вызывается any, после чего мы уже сразу можем начать скачивать файлы и параллельно вызвать best, чтобы подменить на наиболее лучший
            return EXIT_SUCCESS;
        std::cout << "make connect" << std::endl;
        torrent.StartCommunicatingPeers();
    } catch (std::exception & e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << "sec " << std::endl;
    LOG ("Finish");
    // TODO отсюда надо обрабатывать лучший трекер
    return EXIT_SUCCESS;
}