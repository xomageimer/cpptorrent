#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
    #define OS_WIN
#endif

#include "Tracker.h"
#include "Torrent.h"

using namespace std;

int main() {
    bittorrent::Torrent torrent (std::filesystem::current_path()/"aboba.torrent");
    if (!torrent.TryConnect(bittorrent::launch::any, tracker::Event::Empty))    // TODO при первом вызове вызываем best
                                                                                            // TODO мы сохраняем статус торрента куда-нибудь и при повторном скачивании проверяем метаинфу и тогда вызывает any (лучшие трекеры уже будут спереди)
        return EXIT_SUCCESS;
    std::cout << "make connect" << std::endl;
    // TODO отсюда надо обрабатываться лучший трекер
    return EXIT_SUCCESS;
}