#include "TrackerRequester.h"

#include <iostream>
#include <utility>

#include "Tracker.h"
#include "random_generator.h"
#include "auxiliary.h"

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


    promise_of_resp.set_value(std::move(resp));
    Disconnect();
}

void network::TrackerRequester::SetException(const std::string &exc) {
    promise_of_resp.set_exception(network::BadConnect(exc));
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

// TODO использовать query
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
                                    uint8_t buff[16];
                                    connect_request c_req;
                                    c_req.transaction_id = static_cast<int>(random_generator::Random().GetNumber<size_t>());
                                    std::memcpy(&buff, &c_req, sizeof(c_req));

                                    for (auto it = endpoints; it != ba::ip::udp::resolver::iterator(); it++) {
                                        do_connect(it, buff);
                                    }
                                } else {
                                    SetException("Unable to resolve host: " + ec.message());
                                }
                            });
}

void network::udpRequester::do_connect(ba::ip::udp::resolver::iterator endpoint_it, uint8_t * buff) {
    ba::ip::udp::endpoint endpoint = *endpoint_it;
    socket_->open(endpoint.protocol());

    socket_->async_send_to(
                ba::buffer(buff, sizeof(connect_request)), endpoint,
               [&] (boost::system::error_code const & ec, size_t bytes_transferred){
                    if (!ec && bytes_transferred == 16) {
                        do_read_connect_response();
                    }
                }
            );
}

void network::udpRequester::do_read_connect_response() {
    uint8_t buff[16];

}