//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include "TrackerRequester.h"

#include <iostream>
#include <utility>

#include "Tracker.h"
#include "random_generator.h"
#include "auxiliary.h"

// HTTP
void network::httpRequester::SetResponse() {
    std::string resp_str{(std::istreambuf_iterator<char>(&response_)), std::istreambuf_iterator<char>()} ;
    std::vector<std::string> bencode_response;
    boost::regex expression(
            "(^d(?:.|\\n)*e\\Z)"
    );
    if (!boost::regex_split(std::back_inserter(bencode_response), resp_str, expression))
    {
        SetException("Bad response, can't split to bencode format");
    }
    std::cout << bencode_response.back() << std::endl;

    std::stringstream ss (bencode_response.back()) ;
    auto bencoder = bencode::Deserialize::Load(ss);
    const auto& bencode_resp = bencoder.GetRoot().AsDict();

    tracker::Response resp;

    if (bencode_resp.count("tracker_id"))
        resp.tracker_id = bencode_resp.at("tracker_id").AsString();
    if (bencode_resp.count("interval"))
        resp.interval = std::chrono::seconds(bencode_resp.at("interval").AsNumber());
    if (bencode_resp.count("complete"))
        resp.complete = bencode_resp.at("complete").AsNumber();
    if (bencode_resp.count("incomplete"))
        resp.incomplete = bencode_resp.at("incomplete").AsNumber();

    if (auto it = bencode_resp.begin(); (it = bencode_resp.find("min_interval"), it != bencode_resp.end())
                                        || (it = bencode_resp.find("min interval"), it != bencode_resp.end()))
        resp.min_interval = std::chrono::seconds(it->second.AsNumber());
    if (auto it = bencode_resp.begin(); (it = bencode_resp.find("warning_message"), it != bencode_resp.end())
                                        || (it = bencode_resp.find("warning message"), it != bencode_resp.end()))
        resp.warning_message = it->second.AsString();

    // TODO добавлять пиров

    promise_of_resp.set_value(std::move(resp));
    Disconnect();
}

void network::httpRequester::Connect(ba::io_service & io_service, const tracker::Query &query) {
    std::ostream request_stream(&request_);

    auto tracker_ptr = tracker_.lock();
    request_stream << "GET /" << tracker_ptr->GetUrl().Path.value_or("") << "?"

                   << "info_hash=" << UrlEncode(tracker_ptr->GetInfoHash())
                   << "&peer_id=" << UrlEncode(GetSHA1(std::to_string(tracker_ptr->GetMasterPeerId())))
                   << "&port=" << tracker_ptr->GetPort() << "&uploaded=" << query.uploaded << "&downloaded="
                   << query.downloaded << "&left=" << query.left << "&compact=1"
                   << ((query.event != tracker::Event::Empty) ? tracker::events_str.at(query.event) : "")

                   << " HTTP/1.0\r\n"
                   << "Host: " << tracker_ptr->GetUrl().Host << "\r\n"
                   << "Accept: */*\r\n"
                   << "Connection: close\r\n\r\n";
//    ba::streambuf::const_buffers_type bufs = request_.data();
//    std::string str(ba::buffers_begin(bufs),
//                    ba::buffers_begin(bufs) + request_.size());
//    std::cout << str << std::endl;
    if (!socket_)
        socket_.emplace(io_service);
    do_resolve();
}

void network::httpRequester::do_resolve() {
    resolver_.async_resolve(tracker_.lock()->GetUrl().Host, tracker_.lock()->GetUrl().Port,
                            [this](boost::system::error_code const & ec,
                                   ba::ip::tcp::resolver::iterator endpoints){
                                if (!ec) {
                                    do_connect(std::move(endpoints));
                                } else {
                                    SetException("Unable to resolve host: " + ec.message());
                                }
                            });
}

void network::httpRequester::do_connect(ba::ip::tcp::resolver::iterator endpoints) {
    ba::async_connect(*socket_, endpoints,
                               [this](boost::system::error_code const & ec, [[maybe_unused]] const ba::ip::tcp::resolver::iterator&){
                                    if (!ec) {
                                        do_request();
                                    } else {
                                        SetException(ec.message());
                                    }
                               });
}

void network::httpRequester::do_request() {
    ba::async_write(*socket_, request_, [this](boost::system::error_code ec, std::size_t /*length*/){
        if (!ec) {
            do_read_response_status();
        } else {
            SetException(ec.message());
        }
    });
}

void network::httpRequester::do_read_response_status() {
    ba::async_read_until(*socket_,
                                  response_,
                                  "\r\n",
                                  [this](boost::system::error_code ec, std::size_t bytes_transferred/*length*/)
                                  {
                                      if (!ec)
                                      {
                                          std::istream response_stream(&response_);
                                          std::string http_version;
                                          response_stream >> http_version;
                                          std::cerr << tracker_.lock()->GetUrl().Host << ":" << tracker_.lock()->GetUrl().Port << " " << http_version << std::endl;
                                          unsigned int status_code;
                                          response_stream >> status_code;
                                          std::string status_message;
                                          std::getline(response_stream, status_message);

                                          if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                                              SetException("Invalid response\n");
                                          } else if (status_code != 200) {
                                              SetException("Response returned with status code " + std::to_string(status_code) + "\n");
                                          } else do_read_response_header();
                                      }
                                      else
                                      {
                                          SetException(ec.message());
                                      }
                                  });
}

void network::httpRequester::do_read_response_header() {
    ba::async_read_until(*socket_,
                                  response_,
                                  "\r\n\r\n",
                                  [this](boost::system::error_code ec, std::size_t /*length*/)
                                  {
                                      if (!ec)
                                      {
                                          do_read_response_body();
                                      }
                                      else if (ec != ba::error::eof)
                                      {
                                          SetException(ec.message());
                                      }
                                  });
}

void network::httpRequester::do_read_response_body() {
    ba::async_read(*socket_,
                            response_,
                            [this](boost::system::error_code ec, std::size_t bytes_transferred/*length*/)
                            {
                                if (!ec)
                                {
                                    do_read_response_body();
                                }
                                else if (ec == ba::error::eof)
                                {
                                    SetResponse();
                                } else {
                                    SetException(ec.message());
                                }
                            });
}

// UDP
void network::udpRequester::Connect(ba::io_service & io_service, const tracker::Query &query) {
    if (!socket_)
        socket_.emplace(io_service);

    query_ = query;
    do_resolve();
}

void network::udpRequester::do_resolve() {
    resolver_.async_resolve(tracker_.lock()->GetUrl().Host, tracker_.lock()->GetUrl().Port,
                            [this](boost::system::error_code const & ec,
                                   ba::ip::udp::resolver::iterator endpoints){
                                if (!ec) {
                                    endpoints_it_ = std::move(endpoints);
                                    ba::ip::udp::endpoint endpoint = *endpoints_it_;
                                    socket_->open(endpoint.protocol());

                                    make_announce_request();
                                    make_connect_request();

                                    do_try_connect();
                                    connect_timeout_.async_wait([this](boost::system::error_code const & ec) {
                                        if (!ec) {
                                            connect_deadline();
                                        }
                                    });
                                } else {
                                    SetException("Unable to resolve host: " + ec.message());
                                }
                            });
}


void network::udpRequester::do_try_connect() {
    std::cerr << "connect attempt: " << attempts_ << std::endl;
    if (endpoints_it_ == ba::ip::udp::resolver::iterator())
        return;

    if (attempts_ >= MAX_CONNECT_ATTEMPTS) {
        UpdateEndpoint();
    }
    if (endpoints_it_ != ba::ip::udp::resolver::iterator()) {
        do_connect();
        attempts_++;
    }
}

void network::udpRequester::do_connect() {
    if (endpoints_it_ == ba::ip::udp::resolver::iterator())
        return;

    std::memcpy(&buff, &c_req, sizeof(connect_request));
    ba::ip::udp::endpoint endpoint = *endpoints_it_;
    socket_->async_send_to(
               ba::buffer(buff, sizeof(connect_request)), endpoint,
               [this] (boost::system::error_code const & ec, size_t bytes_transferred){
                   std::cerr << "send successfull completly" << std::endl;
                    if (ec || bytes_transferred != sizeof(connect_request)) {
                        attempts_ = MAX_CONNECT_ATTEMPTS;
                    } else {
                        do_connect_response();
                    }
                }
            );
    connect_timeout_.expires_from_now(connection_waiting_time + epsilon);
}

void network::udpRequester::do_connect_response() {
    if (endpoints_it_ == ba::ip::udp::resolver::iterator())
        return;

    ba::ip::udp::endpoint endpoint = *endpoints_it_;
    socket_->async_receive_from(
            ba::buffer(buff, sizeof(connect_response)), endpoint,
            [this] (boost::system::error_code const & ec, size_t bytes_trasferred) {
                std::cerr << "receive successfull completly" << std::endl;
                int val;
                std::memcpy(&val, &buff[4], 4);
                be::native_to_big_inplace(val);

                if (!ec && bytes_trasferred == sizeof(connect_response) && val == c_req.transaction_id.value() && buff[0] == 0) {
                    connect_timeout_.cancel();

                    std::memcpy(&c_resp, &buff, sizeof(connect_response));
                    do_try_announce();
                    announce_timeout_.async_wait( [this](boost::system::error_code const & ec){
                        if (!ec) {
                            announce_deadline();
                        }
                    });
                    connect_timeout_.expires_from_now(boost::posix_time::pos_infin);
                }
            }
    );
}

void network::udpRequester::do_try_announce() {
    if (endpoints_it_ == ba::ip::udp::resolver::iterator())
        return;

    if (announce_attempts_ >= MAX_ANNOUNCE_ATTEMPTS) {
        announce_attempts_ = 0;

        do_try_connect();
        connect_timeout_.async_wait([this](boost::system::error_code const & ec) {
            if (!ec) {
                connect_deadline();
            }
        });
        announce_timeout_.expires_from_now(boost::posix_time::pos_infin);
    } else {
        do_announce();

        announce_attempts_++;
    }
}

void network::udpRequester::do_announce() {
    if (endpoints_it_ == ba::ip::udp::resolver::iterator())
        return;

    std::memcpy(&request[0], &c_resp.connection_id, sizeof(c_resp.connection_id));
    std::memcpy(&request[12], &c_resp.transaction_id, sizeof(c_resp.transaction_id));

    ba::ip::udp::endpoint endpoint = *endpoints_it_;
    socket_->async_send_to(
            ba::buffer(request, sizeof(request)), endpoint,
            [this] (boost::system::error_code const & ec, size_t bytes_transferred){
                std::cerr << "announce send successfull completly" << std::endl;
                if (ec || bytes_transferred != sizeof(request)) {
                    announce_attempts_ = MAX_ANNOUNCE_ATTEMPTS;
                } else {
                    do_announce_response();
                }
            }
    );
    announce_timeout_.expires_from_now(announce_waiting_time + epsilon);
}

void network::udpRequester::do_announce_response() {
    if (endpoints_it_ == ba::ip::udp::resolver::iterator())
        return;

    ba::ip::udp::endpoint endpoint = *endpoints_it_;
    socket_->async_receive_from(
            ba::buffer(response, MTU), endpoint,
            [this] (boost::system::error_code const & ec, size_t bytes_transferred) {
                std::cerr << "announce get successfull completly" << std::endl;
                int val;
                std::memcpy(&val, &response[4], 4);
                be::native_to_big_inplace(val);

                std::cerr << "_________________________________"
                          << "\n!ec: " << !ec
                          << "\nbytes_transferred: " << bytes_transferred
                          << "\nval == c_req.transaction_id.value(): " << (val == c_req.transaction_id.value())
                          << "\nresponse[0] != 0: " << (response[0] != 0)
                          << "\nresponse[0] = " << std::hex << be::big_to_native(int32_t(response[0]))
                          << "\n_________________________________" << std::endl;
                if (!ec && bytes_transferred >= 20 && val == c_req.transaction_id.value() && response[0] != 0) {
                    announce_timeout_.cancel();
                    SetResponse();
                }
            }
    );
}

void network::udpRequester::make_connect_request() {
    c_req.transaction_id = be::native_to_big(static_cast<int>(random_generator::Random().GetNumber<size_t>()));
}

// TODO сделать более читабельно + убрать дубли + переделать всё в функции вида native_to_big
void network::udpRequester::make_announce_request() {
    request[8] = 1;
    std::memcpy(&request[16], tracker_.lock()->GetInfoHash().c_str(), 20);
    std::memcpy(&request[36], GetSHA1(std::to_string(tracker_.lock()->GetMasterPeerId())).c_str(), 20);

    static auto BE = [&](auto native_val) {
        auto a = native_val;
        be::native_to_big_inplace(a);
        return a;
    };

    size_t i64_tmp;
    std::memcpy(&request[56], (i64_tmp = BE(query_.downloaded), &i64_tmp), sizeof(query_.downloaded));
    std::memcpy(&request[64], (i64_tmp = BE(query_.left), &i64_tmp), sizeof(query_.left));
    std::memcpy(&request[72], (i64_tmp = BE(query_.uploaded), &i64_tmp), sizeof(query_.uploaded));

    int i32_tmp;
    std::memcpy(&request[80], (i32_tmp = BE(static_cast<int>(query_.event)), &i32_tmp), sizeof(i32_tmp));
    std::memcpy(&request[84], (query_.ip ? (i32_tmp = BE(IpToInt(query_.ip.value())), &i32_tmp) : (i32_tmp = BE(0), &i32_tmp)), sizeof(i32_tmp));
    std::memcpy(&request[88], (query_.key ? (i32_tmp = BE(std::stoi(query_.key.value())), &i32_tmp) : (i32_tmp = BE(0), &i32_tmp)), sizeof(i32_tmp));
    std::memcpy(&request[92], (query_.numwant ? (i32_tmp = BE(static_cast<int>(query_.numwant.value())), &i32_tmp) : (i32_tmp = BE(-1), &i32_tmp)), sizeof(i32_tmp));
    std::memcpy(&request[96], (query_.trackerid ? (i32_tmp = BE(std::stoi(query_.trackerid.value())), &i32_tmp) : (i32_tmp = BE(0), &i32_tmp)), sizeof(i32_tmp));
}

void network::udpRequester::UpdateEndpoint() {
    attempts_ = 0;
    endpoints_it_++;

    socket_->cancel();
    if (endpoints_it_ == ba::ip::udp::resolver::iterator()) {
        SetException("all endpoints were polled but without success");
    }
}

void network::udpRequester::connect_deadline() {
    std::cerr << "connect timer" << std::endl;

    if (endpoints_it_ == ba::ip::udp::resolver::iterator())
        return;

    if (connect_timeout_.expires_at() <= ba::deadline_timer::traits_type::now())
    {
        socket_->cancel();
        do_try_connect();
    }

    connect_timeout_.async_wait([this](boost::system::error_code const & ec) {
        if (!ec) {
            connect_deadline();
        }
    });
}

void network::udpRequester::announce_deadline() {
    std::cerr << "announce timer" << std::endl;

    if (endpoints_it_ ==  ba::ip::udp::resolver::iterator())
        return;

    if (announce_timeout_.expires_at() <=  ba::deadline_timer::traits_type::now())
    {
        socket_->cancel();
        do_try_announce();
    }

    announce_timeout_.async_wait( [this](boost::system::error_code const & ec){
        if (!ec) {
            announce_deadline();
        }
    });
}