#define TORRENT_RELEASE
#define TORRENT_DEBUG
#ifdef TORRENT_RELEASE
    #undef TORRENT_DEBUG
#endif

#include "Tracker.h"
#include "Torrent.h"

using namespace std;

int main() {
    bittorrent::Torrent torrent (std::filesystem::current_path()/"total-war-warhammer-2.torrent");
    if (!torrent.TryConnect(bittorrent::launch::any, tracker::Event::Empty))    // TODO при первом вызове вызываем best
                                                                                            // TODO мы сохраняем статус торрента куда-нибудь и при повторном скачивании проверяем метаинфу и тогда вызывает any (лучшие трекеры уже будут спереди)
        return EXIT_SUCCESS;
    std::cout << "make connect" << std::endl;
    // TODO отсюда надо обрабатываться лучший трекер
    return EXIT_SUCCESS;
}