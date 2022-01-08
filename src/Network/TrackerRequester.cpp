#define BOOST_ASIO_ENABLE_HANDLER_TRACKING

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

template <typename T>
struct swap_endian {
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");
private:
    union value_type
    {
        T full;
        unsigned char u8[sizeof(T)];
    } dest;

public:
    explicit swap_endian(T u) {
        value_type source{};
        source.full = u;

        for (size_t k = 0; k < sizeof(T); k++){
            dest.u8[k] = source.u8[sizeof(T) - k - 1];
        }
    }
    explicit swap_endian(const unsigned char * u_arr) {
        value_type source{};
        for (size_t k = 0; k < sizeof(T); k++) {
            source.u8[k] = u_arr[k];
        }

        for (size_t k = 0; k < sizeof(T); k++){
            dest.u8[k] = source.u8[sizeof(T) - k - 1];
        }
    }

    void AsArray(uint8_t * arr) const {
        for (size_t k = 0; k < sizeof(T); k++) {
            arr[k] = dest.u8[k];
        }
    }
    [[nodiscard]] T AsValue() const {
        return dest.full;
    }
};


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

                                    make_connect_request();
                                    make_announce_request();

                                    do_try_connect();
                                } else {
                                    SetException("Unable to resolve host: " + ec.message());
                                }
                            });
}


void network::udpRequester::do_try_connect() {
    if (endpoints_it_ == ba::ip::udp::resolver::iterator())
        return;

    if (attempts_ >= MAX_CONNECT_ATTEMPTS) {
        UpdateEndpoint();
    }
    if (endpoints_it_ != ba::ip::udp::resolver::iterator()) {
        attempts_++;
        std::cerr << "connect attempt: " << attempts_ << std::endl;
        do_connect();
        connect_timeout_.async_wait([this](boost::system::error_code const & ec) {
            if (!ec) {
                connect_deadline();
            }
        });
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
            [this] (boost::system::error_code const & ec, size_t bytes_transferred) {
                if (ec) {
                    std::cerr << ec.message() << std::endl;
                    return;
                }
                uint32_t value;
                std::memcpy(&value, &buff[4], sizeof(connect_request::transaction_id));

                std::cerr << "receive successfull completly" << std::endl;

                std::cerr << bytes_transferred << std::endl;
                std::cerr << value << " == " << c_req.transaction_id << " -> " << ( value == c_req.transaction_id) << std::endl;

                if (!ec && bytes_transferred == sizeof(connect_response) && value == c_req.transaction_id && buff[0] == 0) {
                    connect_timeout_.cancel();

                    std::memcpy(&c_resp, &buff, sizeof(connect_response));
                    do_try_announce();
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
    } else {
        announce_attempts_++;
        do_announce();
        announce_timeout_.async_wait([this](boost::system::error_code const & ec){
            if (!ec) {
                announce_deadline();
            }
        });
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
                if (ec){
                    std::cerr << ec.message() << std::endl;
                    return;
                }

                std::cerr << "announce get successfull completly" << std::endl;
                uint32_t val;
                std::memcpy(&val, &response[4], sizeof(connect_response::transaction_id));

                std::cerr << "_________________________________"
                          << "\n!ec: " << !ec
                          << "\nbytes_transferred: " << bytes_transferred
                          << "\nval == c_resp.transaction_id.value(): " << (val == c_resp.transaction_id)
                          << "\nresponse[0] != 0: " << (swap_endian<uint32_t>(&response[0]).AsValue() != 0)
                          << "\nresponse[0] = " << std::hex << swap_endian<uint32_t>(&response[0]).AsValue()
                          << "\n________________________________" << std::endl;
                if (!ec && bytes_transferred >= 20 && val == c_resp.transaction_id && swap_endian<uint32_t>(&response[0]).AsValue() == 1) {
                    announce_timeout_.cancel();

                    SetResponse();
                } else if (!ec && bytes_transferred != 0 && val == c_resp.transaction_id && swap_endian<uint32_t>(&response[0]).AsValue() == 3){
                    std::cerr << "catch error from tracker announce: " << static_cast<const unsigned char*>(&response[8]) << std::endl;
                }
            }
    );
}

void network::udpRequester::make_connect_request() {
    c_req.transaction_id = swap_endian( static_cast<uint32_t>(random_generator::Random().GetNumber<size_t>())).AsValue();
    c_req.protocol_id = swap_endian( static_cast<uint64_t>(0x41727101980)).AsValue();
}

void network::udpRequester::make_announce_request() {
    swap_endian((uint32_t)1).AsArray(&request[8]);

    std::memcpy(&request[16], tracker_.lock()->GetInfoHash().c_str(), 20);
    std::memcpy(&request[36], GetSHA1(std::to_string(tracker_.lock()->GetMasterPeerId())).c_str(), 20);

    swap_endian(query_.downloaded).AsArray(&request[56]);
    swap_endian(query_.left).AsArray(&request[64]);
    swap_endian(query_.uploaded).AsArray(&request[72]);
    swap_endian(static_cast<int>(query_.event)).AsArray(&request[80]);
    swap_endian((query_.ip ? IpToInt(query_.ip.value()) : 0)).AsArray(&request[84]);
    swap_endian((query_.key ? std::stoi(query_.key.value()) : -1)).AsArray(&request[88]);
    swap_endian((query_.numwant ? static_cast<int>(query_.numwant.value()) : -1)).AsArray(&request[92]);
    swap_endian( (query_.trackerid ? std::stoi(query_.trackerid.value()) : 0)).AsArray(&request[96]);
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
    } else {
        connect_timeout_.async_wait([this](boost::system::error_code const &ec) {
            if (!ec) {
                connect_deadline();
            }
        });
    }
}

void network::udpRequester::announce_deadline() {
    std::cerr << "announce timer" << std::endl;

    if (endpoints_it_ ==  ba::ip::udp::resolver::iterator())
        return;

    if (announce_timeout_.expires_at() <=  ba::deadline_timer::traits_type::now())
    {
        socket_->cancel();
        do_try_announce();
    } else {
        announce_timeout_.async_wait([this](boost::system::error_code const &ec) {
            if (!ec) {
                announce_deadline();
            }
        });
    }
}