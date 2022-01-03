#include "TrackerRequester.h"

#include <iostream>
#include <utility>
#include <iterator>

#include "auxiliary.h"

void network::TrackerRequester::SetResponse() {
//    std::string resp_str{(std::istreambuf_iterator<char>(&response)), std::istreambuf_iterator<char>()} ;
//    std::vector<std::string> urls_parts;
//    boost::regex expression(
//            "^d(?:.|\\n)*e\\Z"
//    );
//    if (!boost::regex_split(std::back_inserter(urls_parts), resp_str, expression))
//    {
//        return;
//    }
//    std::cout << resp_str << std::endl;
//
//    std::stringstream ss (urls_parts.front()) ;
//    auto bencoder = bencode::Deserialize::Load(ss);
//    const auto& bencode_resp = bencoder.GetRoot();
//
    tracker::Response resp;
//    if (auto tracker_id_opt = bencode_resp.TryAt("tracker_id"); tracker_id_opt)
//        resp.tracker_id = tracker_id_opt.value().get().AsString();
//    if (auto interval_opt = bencode_resp.TryAt("interval"); interval_opt)
//        resp.interval = std::chrono::seconds(interval_opt.value().get().AsNumber());
//    if (auto min_interval_opt = bencode_resp.TryAt("min_interval"); min_interval_opt)
//        resp.min_interval = std::chrono::seconds(min_interval_opt.value().get().AsNumber());

//    resp.complete = bencode_resp["complete"].AsNumber();
//    resp.incomplete = bencode_resp["incomplete"].AsNumber();

    // TODO додеалть респоунс

    promise_of_resp.set_value(std::move(resp));
//    socket_.close();
}

void network::TrackerRequester::SetException(const std::string &exc) {
    promise_of_resp.set_exception(network::BadConnect(exc));
//    socket_.close();
}

void network::httpRequester::Connect(const tracker::Query &query) {
    std::ostream request_stream(&request_);

    auto tracker_ptr = tracker_.lock();
//    std::string request_str = "/announce?info_hash=%AFWo%87r%DA%82%B7%F5%8B%B6%21%1C%2AF%3B%F8%7BV%20&peer_id=16246984497716370399&port=6881&uploaded=0&downloaded=0&left=0&compact=1";
    request_stream << "GET /" << tracker_ptr->GetUrl().Path.value_or("") << "?"

                   << "info_hash=" << tracker_ptr->GetInfoHash()
                   << "&peer_id=" << tracker_ptr->GetMasterPeerId()
                   << "&port=" << tracker_ptr->GetPort() << "&uploaded=" << query.uploaded << "&downloaded="
                   << query.downloaded << "&left=" << query.left << "&compact=1"

                   << " HTTP/1.0\r\n"
                   << "Host: " << tracker_ptr->GetUrl().Host << "\r\n"
                   << "Accept: */*\r\n"
                   << "Connection: close\r\n\r\n";

//    boost::asio::streambuf::const_buffers_type bufs = request_.data();
//    std::string str(boost::asio::buffers_begin(bufs),
//                    boost::asio::buffers_begin(bufs) + request_.size());
//    std::cout << str << std::endl;
    do_resolve();
}

void network::httpRequester::do_resolve() {
    resolver_.async_resolve(tracker_.lock()->GetUrl().Host, tracker_.lock()->GetUrl().Port,
                            [this](boost::system::error_code const & ec,
                                          ba::ip::tcp::resolver::iterator endpoints){
                                if (!ec) {
                                    do_connect(std::move(endpoints));
                                } else {
                                    SetException(ec.message());
                                }
                            });
}


void network::httpRequester::do_connect(ba::ip::tcp::resolver::iterator endpoints) {
    boost::asio::async_connect(socket_, endpoints,
                               [this](boost::system::error_code const & ec, ba::ip::tcp::resolver::iterator){
                                    if (!ec) {
                                        do_request();
                                    } else {
                                        SetException(ec.message());
                                    }
                               });
}

void network::httpRequester::do_request() {
    boost::asio::async_write(socket_, request_, [this](boost::system::error_code ec, std::size_t /*length*/){
        if (!ec) {
            do_read_response_status();
        } else {
            SetException(ec.message());
        }
    });
}

void network::httpRequester::do_read_response_status() {
    boost::asio::async_read_until(socket_,
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
    boost::asio::async_read_until(socket_,
                                  response_,
                                  "\r\n\r\n",
                                  [this](boost::system::error_code ec, std::size_t /*length*/)
                                  {
                                      if (!ec)
                                      {
                                          do_read_response_body();
                                      }
                                      else if (ec != boost::asio::error::eof)
                                      {
                                          SetException(ec.message());
                                      }
                                  });
}

void network::httpRequester::do_read_response_body() {
    boost::asio::async_read(socket_,
                            response_,
                            [this](boost::system::error_code ec, std::size_t bytes_transferred/*length*/)
                            {
                                if (!ec)
                                {
                                    do_read_response_header();
                                }
                                else if (ec == boost::asio::error::eof)
                                {
                                    SetResponse();
                                } else {
                                    SetException(ec.message());
                                }
                            });
}


