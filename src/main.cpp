#include "Listener.h"
#include "Torrent.h"
#include "Tracker.h"
#include "logger.h"

using namespace std;

// TODO добавить менеджера кт будет взаимодействовать с данными из какого-нибудь файлика с информацией о том какие файлы есть у нас для раздачи (сидироваения) и какие мы докачиваем!
int main() {
    LOG("Start");
    auto start = std::chrono::steady_clock::now();
    boost::asio::io_service service;
    try {
        auto listener = std::make_shared<network::Listener>(boost::asio::make_strand(service));
        bittorrent::Torrent torrent(service, std::filesystem::current_path() / "Mount_and_Blade_II_Bannerlord_1.7.0.torrent", std::filesystem::current_path(), listener->GetPort());// TODO config from console

        if (!torrent.TryConnect(bittorrent::Launch::Best,
                                bittorrent::Event::Empty))// TODO сначала вызывается Any, после чего мы уже сразу можем начать скачивать файлы и параллельно вызвать Best, чтобы подменить на наиболее лучший
            return EXIT_SUCCESS;
        std::cout << "make connect" << std::endl;
        torrent.StartCommunicatingPeers();
        std::cout << "gonna die" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << "sec " << std::endl;
    LOG("Finish");
    // TODO отсюда надо обрабатывать лучший трекер
    return EXIT_SUCCESS;
}