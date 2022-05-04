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

    std::string resp_str(reinterpret_cast<const char *>(msg_.GetDataPointer()), msg_.TotalLength());

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
            uint32_t ip = BigToNative(ArrayToValue<uint32_t>(&peer[0]));
            /* port as little endian */
            uint16_t port = BigToNative(ArrayToValue<uint16_t>(&peer[4]));

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
            uint32_t ip = BigToNative(p["ip"].AsNumber());
            uint16_t port = BigToNative(p["port"].AsNumber());

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

    std::ostringstream request_sstream;

    auto my_hash = UrlEncode(tracker_.GetInfoHash());
    request_sstream << "GET /" << tracker_.GetUrl().Path.value_or("") << "?"

                    << "info_hash=" << UrlEncode(tracker_.GetInfoHash())
                    << "&peer_id=" << UrlEncode(std::string(reinterpret_cast<const char *>(tracker_.GetMasterPeerId()), 20))
                    << "&port=" << tracker_.GetPort() << "&uploaded=" << query.uploaded << "&downloaded=" << query.downloaded
                    << "&left=" << query.left << "&compact=" << query.compact
                    << ((query.event != bittorrent::Event::Empty) ? std::string("&event=") + bittorrent::events_str.at(query.event) : "")

                    << " HTTP/1.0\r\n"
                    << "Host: " << tracker_.GetUrl().Host << "\r\n"
                    << "Accept: */*\r\n"
                    << "Connection: close\r\n\r\n";

    TCPSocket::Connect(
        tracker_.GetUrl().Host, tracker_.GetUrl().Port,
        [this, req_str = std::move(request_sstream.str())]() mutable { do_request(std::move(req_str)); },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
    Await(connect_waiting_);
}

void network::httpRequester::do_request(std::string request_str) {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    Send(
        {reinterpret_cast<uint8_t *>(request_str.data()), request_str.size()}, [this](size_t /*length*/) { do_read_response_status(); },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
}

void network::httpRequester::do_read_response_status() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    // TODO мувать в msg_ из sv
    ReadUntil(
        "\r\n",
        [this](Data data) {
            msg_.Reset(data.size());
            std::memcpy(msg_.GetDataPointer(), data.data(), data.size());

            std::string http_version;
            http_version = msg_.GetString();

            LOG(tracker_.GetUrl().Host, " : ", " get response (", tracker_.GetUrl().Port, " ", http_version, ")");

            unsigned int status_code;
            msg_ >> status_code;
            std::string status_message = msg_.GetLine();

            if (!msg_.Empty() || http_version.substr(0, 5) != "HTTP/") {
                SetException("Invalid response\n");
            } else if (status_code != 200) {
                SetException("Response returned with status code " + std::to_string(status_code) + "\n");
            } else {

                LOG(tracker_.GetUrl().Host, " : ", " correct http status");
                do_read_response_header();
            }
        },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
}

void network::httpRequester::do_read_response_header() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ReadUntil(
        "\r\n\r\n",
        [this](Data data) {
            LOG(tracker_.GetUrl().Host, " : ", " read response header");
            do_read_response_body();
        },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
}

void network::httpRequester::do_read_response_body() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    Read(
        bittorrent_constants::MTU,
        [this](Data data) {
            LOG(tracker_.GetUrl().Host, " : ", " read response body");
            msg_.Add(reinterpret_cast<const uint8_t *>(data.data()), data.size());
            do_read_response_body();
        },
        [this](boost::system::error_code ec) {
            if (ec == ba::error::eof) {
                SetResponse();
            } else {
                SetException(ec.message());
            }
        });
}

// TODO сделать так чтобы каждый из айпишников от одного урла сразу асихнронно тоже обрабатывался
// UDP
void network::udpRequester::SetResponse() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (is_set) return;

    bittorrent::Response resp;

    resp.interval = std::chrono::seconds(NativeToBig(ArrayToValue<uint32_t>(&msg_.GetDataPointer()[8])));

    for (size_t i = 20; msg_.GetDataPointer()[i] != (uint8_t)'\0'; i += 6) {
        uint8_t peer[6];
        for (size_t j = 0; j != std::size(peer); j++) {
            peer[j] = msg_.GetDataPointer()[i + j];
        }
        std::string bin = std::string(std::begin(peer), std::end(peer)); // raw data from tracker

        /* ip as little endian */
        uint32_t ip = BigToNative(ArrayToValue<uint32_t>(&peer[0]));
        /* port as little endian */
        uint16_t port = BigToNative(ArrayToValue<uint16_t>(&peer[4]));

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

    UDPSocket::Connect(
        tracker_.GetUrl().Host, tracker_.GetUrl().Port,
        [this] {
            make_connect_request();
            make_announce_request();
            do_try_connect();
        },
        [this](boost::system::error_code ec) { SetException("Unable to resolve host: " + ec.message()); });
}

void network::udpRequester::do_try_connect() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (!connect_attempts_--) {
        update_endpoint();
    } else {
        LOG(tracker_.GetUrl().Host, " : ", "connect attempt: ", connect_attempts_);

        connect_req_msg_.Reset(sizeof(connect_request));
        connect_req_msg_ << c_req_.protocol_id;
        connect_req_msg_ << c_req_.action;
        connect_req_msg_ << c_req_.transaction_id;

        boost::posix_time::ptime time_before = boost::posix_time::microsec_clock::local_time();
        Send(
            connect_req_msg_,
            [this, time_before](size_t bytes_transferred) {
                if (bytes_transferred < 16) {
                    connect_attempts_ = 0;
                    do_try_connect_delay(boost::posix_time::microsec_clock::local_time() - time_before);
                } else {
                    LOG(tracker_.GetUrl().Host, " : ", "send successfull completly");
                    do_connect_response();
                }
            },
            [this](boost::system::error_code ec) {
                connect_attempts_ = 0;
                do_try_connect();
            });
    }
}

void network::udpRequester::do_try_connect_delay(boost::posix_time::time_duration dur) {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    auto new_dur = connect_waiting_ - dur;
    Await(new_dur.is_negative() ? new_dur : boost::posix_time::milliseconds(0),
        [this] {do_try_connect();});
}

void network::udpRequester::do_connect_response() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);
    boost::posix_time::ptime time_before = boost::posix_time::microsec_clock::local_time();
    Read(
        bittorrent_constants::short_buff_size,
        [this, time_before](Data data) {
            if (data.size() < 16) {
                LOG(tracker_.GetUrl().Host, " : ", "get low size: ", data.size());
                do_try_connect_delay(boost::posix_time::microsec_clock::local_time() - time_before);
            } else {
                msg_.Reset(data.size());
                std::memcpy(msg_.GetDataPointer(), data.data(), data.size());

                uint32_t action, value;
                msg_ >> action;
                msg_ >> value;

                LOG(tracker_.GetUrl().Host, " : ", "\nreceive successfull completly ", data.size(), '\n', value,
                    " == ", c_req_.transaction_id, " -> ", (value == c_req_.transaction_id));

                if (value == c_req_.transaction_id && action == 0) {
                    c_resp_.connection_id = ArrayToValue<uint64_t>(&msg_.GetDataPointer()[8]);
                    c_resp_.transaction_id = ArrayToValue<uint32_t>(&msg_.GetDataPointer()[16]);

                    do_try_announce();
                } else {
                    do_try_connect_delay(boost::posix_time::microsec_clock::local_time() - time_before);
                }
            }
        },
        [this](boost::system::error_code ec) {
            LOG(tracker_.GetUrl().Host, " : ", "get error ", ec.message());
            do_try_connect();
        });
    Await(connect_waiting_);
}

void network::udpRequester::do_try_announce() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (!announce_attempts_--) {
        announce_attempts_ = bittorrent_constants::MAX_ANNOUNCE_ATTEMPTS;
        do_try_connect();
    } else {
        std::memcpy(&announce_req_msg_.GetDataPointer()[0], &c_resp_.connection_id, sizeof(c_resp_.connection_id));
        std::memcpy(&announce_req_msg_.GetDataPointer()[12], &c_resp_.transaction_id, sizeof(c_resp_.transaction_id));

        boost::posix_time::ptime time_before = boost::posix_time::microsec_clock::local_time();
        Send(
            announce_req_msg_,
            [this, time_before](size_t bytes_transferred) {
                LOG(tracker_.GetUrl().Host, " : received ", bytes_transferred);
                if (bytes_transferred != bittorrent_constants::middle_buff_size) {
                    announce_attempts_ = connect_attempts_ = 0;
                    do_try_announce_delay(boost::posix_time::microsec_clock::local_time() - time_before);
                } else {
                    LOG(tracker_.GetUrl().Host, " : ", "announce send successfull completly");
                    do_announce_response();
                }
            },
            [this](boost::system::error_code ec) { do_try_announce(); });
    }
}

void network::udpRequester::do_try_announce_delay(boost::posix_time::time_duration dur) {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    auto new_dur = announce_waiting_ - dur;
    Await(new_dur.is_negative() ? new_dur : boost::posix_time::milliseconds(0),
        [this] {do_try_announce();});
}

void network::udpRequester::do_announce_response() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    boost::posix_time::ptime time_before = boost::posix_time::microsec_clock::local_time();
    Read(
        bittorrent_constants::MTU,
        [this, time_before](Data data) {
            msg_.Reset(data.size() + 1);
            std::memcpy(msg_.GetDataPointer(), data.data(), data.size()); // TODO сделать с out_pos_ (чтобы он сдвигался)

            if (data.size() < 8) {
                do_try_announce_delay(boost::posix_time::microsec_clock::local_time() - time_before);
            } else {
                LOG(tracker_.GetUrl().Host, " : ", "announce successful completion receive ", data.size(), " bytes");

                auto action = BigToNative(ArrayToValue<uint32_t>(&msg_.GetDataPointer()[0]));
                auto val = ArrayToValue<uint32_t>(&msg_.GetDataPointer()[4]);

                LOG(tracker_.GetUrl().Host, " : ", "\nbytes_transferred: ", std::dec, data.size(),
                    "\nval == c_resp.transaction_id.value(): ", (val == c_resp_.transaction_id),
                    "\nresponse[0] != 0: ", (NativeToBig(ArrayToValue<uint32_t>(&msg_.GetDataPointer()[0])) != 0),
                    "\nresponse[0] = ", std::hex, NativeToBig(ArrayToValue<uint32_t>(&msg_.GetDataPointer()[0])));

                if (data.size() >= 20 && val == c_resp_.transaction_id && action == 1) {
                    msg_.GetDataPointer()[msg_.TotalLength() - 1] = '\0';
                    SetResponse();
                } else if (!data.empty() && val == c_resp_.transaction_id && action == 3) {
                    LOG(tracker_.GetUrl().Host, " : ",
                        "catch error from tracker announce: ", static_cast<const unsigned char *>(&msg_.GetDataPointer()[8]));
                    do_try_announce_delay(boost::posix_time::microsec_clock::local_time() - time_before);
                } else {
                    do_try_announce_delay(boost::posix_time::microsec_clock::local_time() - time_before);
                }
            }
        },
        [this](boost::system::error_code ec) { do_try_announce(); });
    Await(announce_waiting_);
}

void network::udpRequester::update_endpoint() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    connect_attempts_ = bittorrent_constants::MAX_CONNECT_ATTEMPTS;
    Promote();

    if (IsEndpointsDried()) {
        LOG(tracker_.GetUrl().Host, " : ", "All endpoints was polled and nothing connected");
        SetException("all endpoints were polled but without success");
    } else {
        do_try_connect();
    }
}

void network::udpRequester::make_connect_request() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    c_req_.transaction_id = static_cast<uint32_t>(random_generator::Random().GetNumber<size_t>());
    c_req_.protocol_id = static_cast<uint64_t>(0x41727101980);
}

void network::udpRequester::make_announce_request() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    announce_req_msg_.Reset(bittorrent_constants::middle_buff_size);

    std::memcpy(&announce_req_msg_.GetDataPointer()[16], tracker_.GetInfoHash().c_str(), 20);
    std::memcpy(&announce_req_msg_.GetDataPointer()[36], tracker_.GetMasterPeerId(), 20);

    ValueToArray(NativeToBig((uint32_t)1), &announce_req_msg_.GetDataPointer()[8]);
    ValueToArray(NativeToBig(query_.downloaded), &announce_req_msg_.GetDataPointer()[56]);
    ValueToArray(NativeToBig(query_.left), &announce_req_msg_.GetDataPointer()[64]);
    ValueToArray(NativeToBig(query_.uploaded), &announce_req_msg_.GetDataPointer()[72]);
    ValueToArray(NativeToBig(static_cast<int>(query_.event)), &announce_req_msg_.GetDataPointer()[80]);
    ValueToArray(NativeToBig((query_.ip ? IpToInt(query_.ip.value()) : 0)), &announce_req_msg_.GetDataPointer()[84]);
    ValueToArray(NativeToBig((query_.key ? std::stoi(query_.key.value()) : 0)), &announce_req_msg_.GetDataPointer()[88]);
    ValueToArray(
        NativeToBig<int>((query_.numwant ? static_cast<int>(query_.numwant.value()) : -1)), &announce_req_msg_.GetDataPointer()[92]);
    ValueToArray(NativeToBig(static_cast<uint16_t>((query_.trackerid ? std::stoi(query_.trackerid.value()) : 0))), &announce_req_msg_.GetDataPointer()[96]);
}