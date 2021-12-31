#include "auxiliary.h"
#include "Tracker.h"
#include "Torrent.h"

using namespace std;

int main() {
    bittorrent::Torrent torrent (std::filesystem::current_path()/"No Country for Old Men.torrent");
    if (!torrent.TryConnect(bittorrent::launch::any, tracker::Event::Empty))    // TODO при первом вызове вызываем best
                                                                                            // TODO мы сохраняем статус торрента куда-нибудь и при повторном скачивании проверяем метаинфу и тогда вызывает any (лучшие трекеры уже будут спереди)
        return EXIT_FAILURE;
    // TODO отсюда надо обрабатываться лучший трекер
    return EXIT_SUCCESS;
}