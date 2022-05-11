//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include "TrackerRequester.h"

#include <iostream>
#include <utility>
#include <variant>

#include "Tracker.h"
#include "auxiliary.h"
#include "random_generator.h"

// HTTP
void network::httpRequester::SetResponse(DataPtr data_ptr) {
    if (is_set) return;

    LOG(tracker_.GetUrl().Host, " : ", "Set response!");

    std::string resp_str;
    resp_str.resize(data_ptr->size());
    size_t size = data_ptr->size();
    data_ptr->CopyTo(resp_str.data(), resp_str.size());

    std::vector<std::string> bencode_response;
    boost::regex expression("(^d(?:.|\\n)*e\\Z)");
    if (!boost::regex_split(std::back_inserter(bencode_response), resp_str, expression)) {
        SetException("Bad response, can't split to bencode format");
        return;
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

    auto my_hash = UrlEncode(tracker_.GetInfoHash());
    (*msg_) << "GET /" << tracker_.GetUrl().Path.value_or("") << "?"

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
        tracker_.GetUrl().Host, tracker_.GetUrl().Port, [this]() mutable { do_request(); },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
    Await(connect_waiting_);
}

void network::httpRequester::do_request() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    Send(
        msg_, [this](size_t /*length*/) { do_read_response_status(); },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
}

void network::httpRequester::do_read_response_status() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ReadUntil(
        "\r\n",
        [this](const DataPtr &data_ptr) {
            auto &data = *StreamBufPtr(data_ptr);

            std::istream is(&data);
            std::string http_version;
            is >> http_version;

            LOG(tracker_.GetUrl().Host, " : ", " get response (", tracker_.GetUrl().Port, " ", http_version, ")");

            uint32_t status_code;
            is >> status_code;

            std::string status_message = data_ptr->GetLine();

            if (http_version.substr(0, 5) != "HTTP/") {
                SetException("Invalid response\n");
            } else if (status_code != 200) {
                SetException("Response returned with status code " + std::to_string(status_code) + "\n");
            } else {
                LOG(tracker_.GetUrl().Host, " : ", " correct http status");
                do_read_response_header();
            }
        },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
    Await(connect_waiting_);
}

void network::httpRequester::do_read_response_header() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ReadUntil(
        "\r\n\r\n",
        [this](const DataPtr &data_ptr) {
            LOG(tracker_.GetUrl().Host, " : ", " read response header");

            msg_->Clear();
            msg_->CopyFrom(*data_ptr);
            do_read_response_body();
        },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
    Await(connect_waiting_);
}

void network::httpRequester::do_read_response_body() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    ReadToEof(
        [this](const DataPtr &data_ptr) {
            LOG(tracker_.GetUrl().Host, " : ", " read response body");
            msg_->CopyFrom(*data_ptr);
            do_read_response_body();
        },
        [this](const DataPtr &data_ptr) {
            msg_->CopyFrom(*data_ptr);
            SetResponse(msg_);
        },
        [this](boost::system::error_code ec) { SetException(ec.message()); });
    Await(connect_waiting_);
}

// UDP
void network::udpRequester::SetResponse(DataPtr data_ptr) {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (is_set) return;

    auto &msg_data = *data_ptr;
    bittorrent::Response resp;

    resp.interval = std::chrono::seconds(NativeToBig(ArrayToValue<uint32_t>(&msg_data[8])));

    for (size_t i = 20; msg_data[i] != (uint8_t)'\0'; i += 6) {
        uint8_t peer[6];
        for (size_t j = 0; j != std::size(peer); j++) {
            peer[j] = msg_data[i + j];
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
    Await(connect_waiting_);
}

void network::udpRequester::do_try_connect() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    if (!connect_attempts_--) {
        update_endpoint();
    } else {
        LOG(tracker_.GetUrl().Host, " : ", "connect attempt: ", connect_attempts_);

        msg_->Clear();
        auto &connect_req_msg_data_ = *msg_;

        connect_req_msg_data_ << c_req_.protocol_id;
        connect_req_msg_data_ << c_req_.action;
        connect_req_msg_data_ << c_req_.transaction_id;

        boost::posix_time::ptime time_before = boost::posix_time::microsec_clock::local_time();
        Send(
            msg_,
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
    if (!new_dur.is_negative()) {
        Await(boost::posix_time::milliseconds(new_dur.total_milliseconds()), [this] { do_try_connect(); });
    } else {
        do_try_connect();
    }
}

void network::udpRequester::do_connect_response() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);
    boost::posix_time::ptime time_before = boost::posix_time::microsec_clock::local_time();
    Read(
        bittorrent_constants::short_buff_size,
        [this, time_before](const DataPtr &data_ptr) {
            if (data_ptr->size() < 16) {
                LOG(tracker_.GetUrl().Host, " : ", "get low size: ", data_ptr->size());
                do_try_connect_delay(boost::posix_time::microsec_clock::local_time() - time_before);
            } else {
                auto &data = *data_ptr;

                uint32_t action, value;
                data >> action;
                data >> value;

                LOG(tracker_.GetUrl().Host, " : ", "\nreceive successfull completly ", data.size(), '\n', value,
                    " == ", c_req_.transaction_id, " -> ", (value == c_req_.transaction_id));

                if (value == c_req_.transaction_id && action == 0) {
                    c_resp_.connection_id = ArrayToValue<uint64_t>(&data[8]);
                    c_resp_.transaction_id = value;

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
        std::memcpy(&announce_req_[0], &c_resp_.connection_id, sizeof(c_resp_.connection_id));
        std::memcpy(&announce_req_[12], &c_resp_.transaction_id, sizeof(c_resp_.transaction_id));

        msg_->Clear();
        msg_->CopyFrom(announce_req_, bittorrent_constants::middle_buff_size);

        boost::posix_time::ptime time_before = boost::posix_time::microsec_clock::local_time();
        Send(
            msg_,
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
    if (!new_dur.is_negative()) {
        Await(boost::posix_time::milliseconds(new_dur.total_milliseconds()),
            [this] { do_try_announce(); });
    } else {
        do_try_announce();
    }
}

void network::udpRequester::do_announce_response() {
    LOG(tracker_.GetUrl().Host, " : ", __FUNCTION__);

    boost::posix_time::ptime time_before = boost::posix_time::microsec_clock::local_time();
    Read(
        bittorrent_constants::MTU,
        [this, time_before](const DataPtr &data_ptr) {
            auto &data = *data_ptr;
            if (data.size() < 8) {
                do_try_announce_delay(boost::posix_time::microsec_clock::local_time() - time_before);
            } else {
                LOG(tracker_.GetUrl().Host, " : ", "announce successful completion receive ", data.size(), " bytes");

                auto action = BigToNative(ArrayToValue<uint32_t>(&data[0]));
                auto val = ArrayToValue<uint32_t>(&data[4]);

                LOG(tracker_.GetUrl().Host, " : ", "\nbytes_transferred: ", std::dec, data.size(),
                    "\nval == c_resp.transaction_id.value(): ", (val == c_resp_.transaction_id),
                    "\nresponse[0] != 0: ", (NativeToBig(ArrayToValue<uint32_t>(&data[0])) != 0), "\nresponse[0] = ", std::hex,
                    NativeToBig(ArrayToValue<uint32_t>(&data[0])));

                if (data.size() >= 20 && val == c_resp_.transaction_id && action == 1) {
                    data << '\0';
                    SetResponse(data_ptr);
                } else if (!data.size() && val == c_resp_.transaction_id && action == 3) {
                    LOG(tracker_.GetUrl().Host, " : ", "catch error from tracker announce: ", static_cast<const unsigned char *>(&data[8]));
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

    std::memcpy(&announce_req_[16], tracker_.GetInfoHash().c_str(), 20);
    std::memcpy(&announce_req_[36], tracker_.GetMasterPeerId(), 20);

    ValueToArray(NativeToBig((uint32_t)1), &announce_req_[8]);
    ValueToArray(NativeToBig(query_.downloaded), &announce_req_[56]);
    ValueToArray(NativeToBig(query_.left), &announce_req_[64]);
    ValueToArray(NativeToBig(query_.uploaded), &announce_req_[72]);
    ValueToArray(NativeToBig(static_cast<int>(query_.event)), &announce_req_[80]);
    ValueToArray(NativeToBig((query_.ip ? IpToInt(query_.ip.value()) : 0)), &announce_req_[84]);
    ValueToArray(NativeToBig((query_.key ? std::stoi(query_.key.value()) : 0)), &announce_req_[88]);
    ValueToArray(NativeToBig<int>((query_.numwant ? static_cast<int>(query_.numwant.value()) : -1)), &announce_req_[92]);
    ValueToArray(NativeToBig(static_cast<uint16_t>((query_.trackerid ? std::stoi(query_.trackerid.value()) : 0))), &announce_req_[96]);
}