#include <iostream>
#include <iterator>
#include <fstream>
#include <unordered_map>
#include <algorithm>

#include <boost/asio.hpp>

#include "auxiliary.h"
#include "Tracker.h"
#include "Torrent.h"
#include "bencode_lib.h"

using namespace std;

int main() {
    bittorrent::Torrent torrent (std::filesystem::current_path()/"WarhammerRegicide.torrent");
    for (auto tracker_it : bittorrent::AnnouncesRange(torrent)){
        if (tracker_it.TryConnect(tracker::Event::Empty))
            break;
    }
    // TODO отсюда надо обрабатываться лучший трекер
    return EXIT_SUCCESS;
}