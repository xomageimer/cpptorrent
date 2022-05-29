#include "Listener.h"
#include "Torrent.h"
#include "Tracker.h"
#include "logger.h"

using namespace std;

// TODO добавить менеджера кт будет взаимодействовать с данными из какого-нибудь файлика с информацией о том какие файлы есть у нас для \
//  раздачи (сидироваения) и какие мы докачиваем!

int main(int argc, char *argv[]) {
#ifdef OS_WIN
    SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
#endif
    LOG("Start");

    if (argc < 2) {
        std::cerr << "Usage: cpptorrent <input_path> [<output_path>]\n";
        return 1;
    }

    auto start = std::chrono::steady_clock::now();
    // TODO сделать количество потоков иначе!
    std::vector<std::thread> threads(std::thread::hardware_concurrency() > 2 ? std::thread::hardware_concurrency() / 2 : 1);
    boost::asio::io_service service;
    boost::asio::io_service::work worker(service);

    auto listener = std::make_shared<network::Listener>(boost::asio::make_strand(service));

    for (auto &thread : threads) {
        thread = std::thread([&] { service.run(); });
    }

    auto service_exit = [&] {
        service.stop();
        for (auto &thread : threads) {
            thread.join();
        }
    };

    std::string torrent_dir = argv[1];
    std::string output_dir = argv[2];

    auto torrent = std::make_shared<bittorrent::Torrent>(
        service, torrent_dir, output_dir, listener->GetPort()); // TODO config from console

    //    service_exit();
    //    return 0;

    listener->AddTorrent(torrent);

    std::thread main_thread;
    std::condition_variable cv;
    std::atomic_bool working = true;
    try {
        std::cerr << "Total piece count " << torrent->GetPieceCount() << std::endl;
        if (!torrent->TryConnect(bittorrent::Launch::Best,
                bittorrent::Event::Empty)) { // TODO сначала вызывается Any, после чего мы уже сразу можем начать скачивать файлы и
                                             // параллельно вызвать Best, чтобы подменить на наиболее лучший
            service_exit();
            return EXIT_SUCCESS;
        }
        std::cout << "make connect: " << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count()
                  << "sec " << std::endl;

        main_thread = std::thread([&] {
            std::mutex cur_thread;
            while (working.load()) {
                unique_lock<mutex> lock(cur_thread);
                torrent->ProcessMeetingPeers();
                cv.wait_for(lock, bittorrent_constants::sleep_time);
                if (torrent->CouldReconnect()) {
                    torrent->TryConnect(bittorrent::Launch::Best, bittorrent::Event::Empty);
                }
            }
        });
    } catch (std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        service_exit();
        return EXIT_FAILURE;
    }
    std::cout << "PRESS KEY TO CANCEL!" << std::endl;
    char c;
    std::cin >> c;
    working.store(false);
    service_exit();
    cv.notify_all();
    main_thread.join();

    std::cout << "gonna die" << std::endl;

    auto end = std::chrono::steady_clock::now();

    std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << "sec " << std::endl;
    LOG("Finish");

    // TODO отсюда надо обрабатывать лучший трекер
    return EXIT_SUCCESS;
}