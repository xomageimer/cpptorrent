#define TORRENT_RELEASE
#define TORRENT_DEBUG
#ifdef TORRENT_RELEASE
    #undef TORRENT_DEBUG
#endif

#include "Tracker.h"
#include "Torrent.h"

using namespace std;

int main() {
    auto start = std::chrono::steady_clock::now();
    {
        bittorrent::Torrent torrent(std::filesystem::current_path() / "WarhammerRegicide.torrent");
        if (!torrent.TryConnect(bittorrent::launch::any,
                                tracker::Event::Empty))    // TODO сначала вызывается any, после чего мы уже сразу можем начать скачивать файлы и параллельно вызвать best, чтобы подменить на наиболее лучший
            return EXIT_SUCCESS;
        std::cout << "make connect" << std::endl;
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << std::endl;
    // TODO отсюда надо обрабатываться лучший трекер
    return EXIT_SUCCESS;
}