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
    bittorrent::Torrent torrent (std::filesystem::current_path()/"No Country for Old Men.torrent");
    if (!torrent.TryConnect()) {
        std::cerr << "Can't connect" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;


//    std::fstream file ("aboba.txt", std::ios::in | std::ios::binary);
//    std::cout << get_sha1(std::string(std::istream_iterator<char>(file), std::istream_iterator<char>()));

//    return 0;
//    std::fstream fs("cyberpunk.torrent", std::ios::in | std::ios::binary);
//    std::fstream fs("WarhammerRegicide.torrent", std::ios::in | std::ios::binary);
//    std::fstream fs("No Country for Old Men.torrent", std::ios::in | std::ios::binary);
//
//    if (!fs.is_open()) {
//        throw std::logic_error("can't open file\n");
//    }
//
//    auto doc = bencode::Deserialize::Load(fs);
//    const auto & bencode_input = doc.GetRoot();
//
//    std::string announce;
//    if (auto announce_opt = bencode_input.TryAt("announce"); announce_opt) {
//        announce = announce_opt.value().get().AsString();
//    } else if (announce_opt = bencode_input.TryAt("announce-list"); announce_opt && !announce_opt.value().get().AsArray().empty()) {
//        announce = announce_opt.value().get().AsArray()[12][0].AsString();
//    } else {
//        announce = "!NONE!";
//    }
////    auto announce = bencode_input["announce"].AsString();
////    return 0;
//
//    auto& info_hash_bencode = bencode_input["info"];
//    std::stringstream hash_to_s;
//    bencode::Serialize::MakeSerialize(info_hash_bencode, hash_to_s);
//    auto hash_info_str = hash_to_s.str();
//
//   auto info_hash = GetSHA1(std::string(std::next(hash_info_str.begin()), std::prev(hash_info_str.end())));
//   std::cout << (announce + "?info_hash=" + UrlEncode(info_hash));

   return 0;
//    std::stringstream ss;
//    size_t count = 1;
//    for (int i = 0; i < info_hash.size();) {
//        ss.str(std::string());
//        for (int j = 0; j < 20; i++, j++) {
//            ss << '|' << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned int>(info_hash[i]);
//            if (j == 19)
//                ss << '|';
//        }
//        std::cout << count << ": " << std::string(ss.str()) << std::endl;
//        count++;
//    }

//    return 0;
}