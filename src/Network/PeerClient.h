#ifndef CPPTORRENT_PEERCLIENT_H
#define CPPTORRENT_PEERCLIENT_H

#include <memory>

#include "Tracker.h"

struct PeerClient {
public:

private:

    std::weak_ptr<tracker::Tracker> tracker_;
};

#endif //CPPTORRENT_PEERCLIENT_H
