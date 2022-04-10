//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "TrackerRequester.h"

#include <iostream>
#include <utility>
#include <variant>

#include "Tracker.h"
#include "auxiliary.h"
#include "random_generator.h"

// TODO лучше всю работу с распарсиванием ответа убрать в класс Tracker, а промис будет от строки или бенкода
// TODO все логи отметить их url'ми чтобы удобнее читать вывод логера

// HTTP
void network::httpRequester::SetResponse() {
    if (is_set) return;

    LOG(tracker_.GetUrl().Host, " : ", "Set response!");

    std::string resp_str{(std::istreambuf_iterator<char>(&response_)), std::istreambuf_iterator<char>()};
    std::vector<std::string> bencode_response;
    boost::regex expression("(^d(?:.|\\n)*e\\Z)");
    if (!boost::regex_split(std::back_inserter(bencode_response), resp_str, expression)) {
        SetException("Bad response, can't split to bencode format");
    }

    std::stringstream ss(bencode_response.back());
    auto bencoder = bencode::Deserialize::Load(ss);
    const auto &bencode_resp = bencoder.GetRoot().AsDict();

    bittorrent::Response resp;

    if (bencode_resp.count("tracker_id")) resp.tracker_id = bencode_resp.at("tracker_id").AsString();
    if (bencode_resp.count("interval")) resp.interval = std::chrono::seconds(bencode_resp.at("interval").AsNumber());
    if (bencode_resp.count("complete")) resp.complete = bencode_resp.at("complete").AsNumber();
    if (bencode_resp.count("incomplete")) resp.incomplete = bencode_resp.at("incomplete").AsNumber();

    if (auto it = bencode_resp.begin(); (it = bencode_resp.find("min_interval"), it != bencode_resp.end()) ||
                                        (it = bencode_resp.find("min interval"), it != bencode_resp.end()))
        resp.min_interval = std::chrono::seconds(it->second.AsNumber());
    if (auto it = bencode_resp.begin(); (it = bencode_resp.find("warning_message"), it != bencode_resp.end()) ||
                                        (it = bencode_resp.find("warning message"), it != bencode_resp.end()))
        resp.warning_message = it->second.AsString();

    auto bencode_peers = bencode_resp.at("peers");
    if (bencode_peers.IsString()) {
        for (size_t i = 0; bencode_peers.AsString()[i] != (uint8_t)'\0'; i += 6) {
            uint8_t peer[6];
            for (size_t j = 0; j != std::size(peer); j++) {
                peer[j] = bencode_peers.AsString()[i + j];
            }
            std::string bin = std::string(std::begin(peer), std::end(peer)); // raw data from tracker

            /* ip as little endian */
            uint32_t ip = SwapEndian(*(uint32_t *)&peer[0]);
            /* port as little endian */
            uint16_t port = SwapEndian(*(uint16_t *)&peer[4]);

            bittorrent::PeerImage pi{bittorrent::Peer{ip, port}, bin};
            resp.peers.push_back(std::move(pi));
        }
    } else if (bencode_peers.IsArray()) {
        for (auto &p : bencode_peers.AsArray()) {
            bittorrent::Peer peer;

            char peer_as_array[26];
            ValueToArray(p["ip"].AsNumber(), reinterpret_cast<uint8_t *>(&peer_as_array[0]));
            ValueToArray(p["port"].AsNumber(), reinterpret_cast<uint8_t *>(&peer_as_array[4]));

            /* port and peer id as little endian */
            uint32_t ip = SwapEndian(p["ip"].AsNumber());
            uint16_t port = SwapEndian(p["port"].AsNumber());

            if (p.TryAt("peer id")) {
                const char *key = p["peer id"].AsString().data();
                std::memcpy(&peer_as_array[6], key, 20);
                peer = bittorrent::Peer(ip, port, reinterpret_cast<const uint8_t *>(key));
            } else
                peer = bittorrent::Peer(ip, port);

            resp.peers.push_back({peer, std::string(std::begin(peer_as_array), std::end(peer_as_array))});
        }
    } else {
        SetException("Invalid peers");
    }

    LOG(tracker_.GetUrl().Host, " : ", "Peers size is: ", std::dec, resp.peers.size());

    promise_of_resp.set_value(std::move(resp));
    Disconnect();
}

void network::httpRequester::Connect(const bittorrent::Query &query) {
    LOG(tracker_.GetUrl().Host, " : ", "Make http request ");

    std::ostream request_stream(&request_);

    auto my_hash = UrlEncode(tracker_.GetInfoHash());
    request_stream << "GET /" << tracker_.GetUrl().Path.value_or("") << "?"

                   << "info_hash=" << UrlEncode(tracker_.GetInfoHash())
                   << "&peer_id=" << UrlEncode(std::string(reinterpret_cast<const char *>(tracker_.GetMasterPeerId()), 20))
                   << "&port=" << tracker_.GetPort() << "&uploaded=" << query.uploaded << "&downloaded=" << query.downloaded
                   << "&left=" << query.left << "&compact=" << query.compact
                   << ((query.event != bittorrent::Event::Empty) ? std::string("&event=") + bittorrent::events_str.at(query.event) : "")

                   << " HTTP/1.0\r\n"
                   << "Host: " << tracker_.GetUrl().Host << "\r\n"
                   << "Accept: */*\r\n"
                   << "Connection: close\r\n\r\n";

    post(resolver_.get_executor(), [this] { do_resolve(); });
}

void network::httpRequester::do_resolve() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    resolver_.async_resolve(
        tracker_.GetUrl().Host, tracker_.GetUrl().Port, [this](boost::system::error_code ec, ba::ip::tcp::resolver::iterator endpoints) {
            if (!ec) {
                do_connect(std::move(endpoints));
                timeout_.async_wait([this](boost::system::error_code const &ec) {
                    if (!ec) {
                        deadline();
                    }
                });
            } else {
                SetException("Unable to resolve host: " + ec.message());
            }
        });
}

void network::httpRequester::do_connect(ba::ip::tcp::resolver::iterator endpoints) {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ba::async_connect(
        socket_, std::move(endpoints), [this](boost::system::error_code ec, [[maybe_unused]] const ba::ip::tcp::resolver::iterator &) {
            if (!ec) {
                do_request();
            } else {
                SetException(ec.message());
            }
        });
    timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
}

void network::httpRequester::do_request() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ba::async_write(socket_, request_, [this](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec) {
            do_read_response_status();
        } else {
            SetException(ec.message());
        }
    });
}

void network::httpRequester::do_read_response_status() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ba::async_read_until(socket_, response_, "\r\n", [this](boost::system::error_code ec, std::size_t bytes_transferred /*length*/) {
        if (!ec) {
            std::istream response_stream(&response_);
            std::string http_version;
            response_stream >> http_version;

            LOG(tracker_.GetUrl().Host, " : ", " get response (", tracker_.GetUrl().Port, " ", http_version, ")");

            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);

            if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                SetException("Invalid response\n");
            } else if (status_code != 200) {
                SetException("Response returned with status code " + std::to_string(status_code) + "\n");
            } else {

                LOG(tracker_.GetUrl().Host, " : ", " correct http status");
                do_read_response_header();
            }
        } else {
            SetException(ec.message());
        }
    });
}

void network::httpRequester::do_read_response_header() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ba::async_read_until(socket_, response_, "\r\n\r\n", [this](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec) {

            LOG(tracker_.GetUrl().Host, " : ", " read response header");

            do_read_response_body();
        } else if (ec != ba::error::eof) {
            SetException(ec.message());
        }
    });
}

void network::httpRequester::do_read_response_body() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ba::async_read(socket_, response_, [this](boost::system::error_code ec, std::size_t bytes_transferred /*length*/) {
        if (!ec) {

            LOG(tracker_.GetUrl().Host, " : ", " read response body");

            do_read_response_body();
        } else if (ec == ba::error::eof) {
            timeout_.cancel();
            SetResponse();
        } else {
            SetException(ec.message());
        }
    });
}

void network::httpRequester::deadline() {
    LOG(tracker_.GetUrl().Host, " : ", "deadline was called");

    if (timeout_.expires_at() <= ba::deadline_timer::traits_type::now()) {
        SetException("timeout for " + std::string(tracker_.GetUrl().Host));
    } else {
        timeout_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                deadline();
            }
        });
    }
}

// TODO сделать так чтобы каждый из айпишников от одного урла сразу асихнронно тоже обрабатывался
// UDP
void network::udpRequester::SetResponse() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (is_set) return;

    bittorrent::Response resp;

    resp.interval = std::chrono::seconds(as_big_endian<uint32_t>(&response[8]).AsValue());

    for (size_t i = 20; response[i] != (uint8_t)'\0'; i += 6) {
        uint8_t peer[6];
        for (size_t j = 0; j != std::size(peer); j++) {
            peer[j] = response[i + j];
        }
        std::string bin = std::string(std::begin(peer), std::end(peer)); // raw data from tracker

        /* ip as little endian */
        uint32_t ip = SwapEndian(*(uint32_t *)&peer[0]);
        /* port as little endian */
        uint16_t port = SwapEndian(*(uint16_t *)&peer[4]);

        bittorrent::PeerImage pi{bittorrent::Peer{ip, port}, bin};
        resp.peers.push_back(std::move(pi));
    }

    LOG(tracker_.GetUrl().Host, " : ", "Peers size is: ", std::dec, resp.peers.size());

    promise_of_resp.set_value(std::move(resp));
    Disconnect();
}

void network::udpRequester::Connect(const bittorrent::Query &query) {
    LOG(tracker_.GetUrl().Host, " : ", "Make udp request");

    query_ = query;
    post(resolver_.get_executor(), [this] { do_resolve(); });
}

void network::udpRequester::do_resolve() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    resolver_.async_resolve(
        tracker_.GetUrl().Host, tracker_.GetUrl().Port, [this](boost::system::error_code ec, ba::ip::udp::resolver::iterator endpoints) {
            if (!ec) {
                endpoints_it_ = std::move(endpoints);
                ba::ip::udp::endpoint endpoint = *endpoints_it_;

                socket_.open(endpoint.protocol());

                make_connect_request();
                make_announce_request();

                do_try_connect();
            } else {
                SetException("Unable to resolve host: " + ec.message());
            }
        });
}

void network::udpRequester::do_try_connect() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) return;

    if (attempts_ >= bittorrent_constants::MAX_CONNECT_ATTEMPTS) {
        UpdateEndpoint();
    }
    if (endpoints_it_ != ba::ip::udp::resolver::iterator()) {
        attempts_++;

        LOG(tracker_.GetUrl().Host, " : ", "connect attempt: ", attempts_);

        do_connect();
        connect_timeout_.async_wait([this](boost::system::error_code const &ec) {
            if (!ec) {
                connect_deadline();
            }
        });
    }
}

void network::udpRequester::do_connect() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) return;

    std::memcpy(&buff, &c_req, sizeof(connect_request));
    ba::ip::udp::endpoint endpoint = *endpoints_it_;
    socket_.async_send_to(ba::buffer(buff, sizeof(buff)), endpoint, [this](boost::system::error_code ec, size_t bytes_transferred) {
        if (ec || bytes_transferred < 16) {
            attempts_ = bittorrent_constants::MAX_CONNECT_ATTEMPTS;
        } else {
            LOG(tracker_.GetUrl().Host, " : ", "send successfull completly");

            do_connect_response();
        }
    });
    connect_timeout_.expires_from_now(bittorrent_constants::connection_waiting_time + bittorrent_constants::epsilon);
}

void network::udpRequester::do_connect_response() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) return;

    ba::ip::udp::endpoint endpoint = *endpoints_it_;
    socket_.async_receive_from(ba::buffer(buff, sizeof(buff)), endpoint, [this](boost::system::error_code ec, size_t bytes_transferred) {
        if (ec || bytes_transferred < 16) {
            LOG(tracker_.GetUrl().Host, " : ", ec.message());

            return;
        }
        uint32_t value;
        std::memcpy(&value, &buff[4], sizeof(connect_request::transaction_id));

        LOG(tracker_.GetUrl().Host, " : ", "\nreceive successfull completly ", bytes_transferred, '\n', value, " == ", c_req.transaction_id,
            " -> ", (value == c_req.transaction_id));

        if (!ec && value == c_req.transaction_id && buff[0] == 0) {
            connect_timeout_.cancel();

            std::memcpy(&c_resp, buff, sizeof(c_resp));

            do_try_announce();
        }
    });
}

void network::udpRequester::do_try_announce() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) return;

    if (announce_attempts_ >= bittorrent_constants::MAX_ANNOUNCE_ATTEMPTS) {
        announce_attempts_ = 0;

        do_try_connect();
    } else {
        announce_attempts_++;
        do_announce();
        announce_timeout_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                announce_deadline();
            }
        });
    }
}

void network::udpRequester::do_announce() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) return;

    std::memcpy(&request[0], &c_resp.connection_id, sizeof(c_resp.connection_id));
    std::memcpy(&request[12], &c_resp.transaction_id, sizeof(c_resp.transaction_id));

    ba::ip::udp::endpoint endpoint = *endpoints_it_;
    socket_.async_send_to(ba::buffer(request, sizeof(request)), endpoint, [this](boost::system::error_code ec, size_t bytes_transferred) {
        if (ec || bytes_transferred != sizeof(request)) {
            LOG(tracker_.GetUrl().Host, " : ", "caught errors from announce");

            announce_attempts_ = bittorrent_constants::MAX_ANNOUNCE_ATTEMPTS;
            attempts_ = bittorrent_constants::MAX_CONNECT_ATTEMPTS;
        } else {
            LOG(tracker_.GetUrl().Host, " : ", "announce send successfull completly");

            do_announce_response();
        }
    });
    announce_timeout_.expires_from_now(bittorrent_constants::announce_waiting_time + bittorrent_constants::epsilon);
}

void network::udpRequester::do_announce_response() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) return;

    ba::ip::udp::endpoint endpoint = *endpoints_it_;
    socket_.async_receive_from(
        ba::buffer(response, bittorrent_constants::MTU), endpoint, [this](boost::system::error_code ec, size_t bytes_transferred) {
            if (ec || bytes_transferred < 8) {
                LOG(tracker_.GetUrl().Host, " : ", ec.message());
                return;
            }

            LOG(tracker_.GetUrl().Host, " : ", "announce get successfull completly");

            uint32_t val;
            std::memcpy(&val, &response[4], sizeof(connect_response::transaction_id));

            uint32_t action = as_big_endian<uint32_t>(&response[0]).AsValue();

            LOG(tracker_.GetUrl().Host, " : ", "\n!ec: ", (bool)!ec, "\nbytes_transferred: ", std::dec, bytes_transferred,
                "\nval == c_resp.transaction_id.value(): ", (val == c_resp.transaction_id),
                "\nresponse[0] != 0: ", (as_big_endian<uint32_t>(&response[0]).AsValue() != 0), "\nresponse[0] = ", std::hex,
                as_big_endian<uint32_t>(&response[0]).AsValue());

            if (!ec && bytes_transferred >= 20 && val == c_resp.transaction_id && action == 1) {
                announce_timeout_.cancel();

                response[bytes_transferred] = '\0';
                SetResponse();
            } else if (!ec && bytes_transferred != 0 && val == c_resp.transaction_id && action == 3) {
                LOG(tracker_.GetUrl().Host, " : ", "catch error from tracker announce: ", static_cast<const unsigned char *>(&response[8]));
            }
        });
}

void network::udpRequester::connect_deadline() {
    LOG(tracker_.GetUrl().Host, " : ", "connect timer");

    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) return;

    if (connect_timeout_.expires_at() <= ba::deadline_timer::traits_type::now()) {
        socket_.cancel();
        do_try_connect();
    } else {
        connect_timeout_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                connect_deadline();
            }
        });
    }
}

void network::udpRequester::announce_deadline() {
    LOG(tracker_.GetUrl().Host, " : ", "announce timer");

    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) return;

    if (announce_timeout_.expires_at() <= ba::deadline_timer::traits_type::now()) {
        socket_.cancel();
        do_try_announce();
    } else {
        announce_timeout_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                announce_deadline();
            }
        });
    }
}

void network::udpRequester::UpdateEndpoint() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    attempts_ = 0;
    endpoints_it_++;

    socket_.cancel();
    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) {
        SetException("all endpoints were polled but without success");
    }
}

void network::udpRequester::make_connect_request() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    c_req.transaction_id = as_big_endian(static_cast<uint32_t>(random_generator::Random().GetNumber<size_t>())).AsValue();
    c_req.protocol_id = as_big_endian(static_cast<uint64_t>(0x41727101980)).AsValue();
}

void network::udpRequester::make_announce_request() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    std::memcpy(&request[16], tracker_.GetInfoHash().c_str(), 20);
    std::memcpy(&request[36], tracker_.GetMasterPeerId(), 20);

    as_big_endian((uint32_t)1).AsArray(&request[8]);
    as_big_endian(query_.downloaded).AsArray(&request[56]);
    as_big_endian(query_.left).AsArray(&request[64]);
    as_big_endian(query_.uploaded).AsArray(&request[72]);
    as_big_endian(static_cast<int>(query_.event)).AsArray(&request[80]);
    as_big_endian((query_.ip ? IpToInt(query_.ip.value()) : 0)).AsArray(&request[84]);
    as_big_endian((query_.key ? std::stoi(query_.key.value()) : 0)).AsArray(&request[88]);
    as_big_endian<int>((query_.numwant ? static_cast<int>(query_.numwant.value()) : -1)).AsArray(&request[92]);
    as_big_endian((query_.trackerid ? std::stoi(query_.trackerid.value()) : 0)).AsArray(&request[96]);
}