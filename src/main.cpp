#define TORRENT_RELEASE
#define TORRENT_DEBUG
#ifdef TORRENT_RELEASE
#undef TORRENT_DEBUG
#endif

#include "Tracker.h"
#include "Torrent.h"

#include "logger.h"

using namespace std;

int main() {
    auto start = std::chrono::steady_clock::now();
    try {
        bittorrent::Torrent torrent(std::filesystem::current_path() / "total-war-warhammer-2.torrent");
        if (!torrent.TryConnect(bittorrent::launch::any,
                                tracker::Event::Empty))    // TODO сначала вызывается any, после чего мы уже сразу можем начать скачивать файлы и параллельно вызвать best, чтобы подменить на наиболее лучший
            return EXIT_SUCCESS;
        std::cout << torrent.GetResponse().peers.size() << std::endl;
        std::cout << "make connect" << std::endl;
    } catch (...) {
        std::cerr << "some error caught, terminate!" << std::endl;
        return EXIT_FAILURE;
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << "ms " << std::endl;
    // TODO отсюда надо обрабатываться лучший трекер
    return EXIT_SUCCESS;
}