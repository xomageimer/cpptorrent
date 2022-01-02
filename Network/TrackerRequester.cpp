#include "TrackerRequester.h"

#include <iostream>
#include <utility>

#include "auxiliary.h"

void network::TrackerRequester::SetResponse() {
    std::string resp_str{(std::istreambuf_iterator<char>(&response)), std::istreambuf_iterator<char>()} ;
//    std::vector<std::string> urls_parts;
//    boost::regex expression(
//            "^d(?:.|\\n)*e\\Z"
//    );
//    if (!boost::regex_split(std::back_inserter(urls_parts), resp_str, expression))
//    {
//        return;
//    }
    std::cout << resp_str << std::endl;
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
    socket_.close();
}

void network::TrackerRequester::SetException(const network::BadConnect &exc) {
    promise_of_resp.set_exception(exc);
    socket_.close();
}

void network::httpRequester::Connect(const tracker::Query &query) {
    resolver_.async_resolve(tracker_.lock()->GetUrl().Host, tracker_.lock()->GetUrl().Port, [query, this](boost::system::error_code ec,
                                                         ba::ip::tcp::resolver::iterator endpoint_iterator) {
        if (!ec) {
            do_connect(endpoint_iterator, query);
        } else {
            SetException(BadConnect(ec.message()));
        }
    });
}

void network::httpRequester::do_connect(
        ba::ip::tcp::resolver::iterator const & endpoints, const tracker::Query &query) {
    boost::asio::async_connect(socket_, endpoints,
                               [query, this](boost::system::error_code ec, ba::ip::tcp::resolver::iterator){
                                   if (!ec){
                                       do_request(query);
                                   } else {
                                       SetException(BadConnect(std::string(ec.message())));
                                   }
                               });
}

void network::httpRequester::do_request(const tracker::Query &query) {
    ba::streambuf request_query;
    std::ostream request_stream(&request_query);
    request_stream << "GET /" << tracker_.lock()->GetUrl().Path.value_or("") << "?"

                   << "info_hash=" << UrlEncode(tracker_.lock()->GetInfoHash())
                   << "&peer_id=" << UrlEncode(std::to_string(tracker_.lock()->GetMasterPeerId()))
                   << "&port=" << tracker_.lock()->GetPort() << "&uploaded=" << query.uploaded << "&downloaded="
                   << query.downloaded << "&left=" << query.left << "&compact=1"
                   << ((query.event != tracker::Event::Empty) ? "&event=" + tracker::events_str.at(query.event) : "")

                   << " HTTP/1.0\r\n"
                   << "Host: " << tracker_.lock()->GetUrl().Host << "\r\n"
                   << "Accept: */*\r\n"
                   << "Connection: close\r\n\r\n";

    boost::asio::async_write(socket_, request_query,  [this](boost::system::error_code ec, std::size_t /*length*/){
        if (!ec) {
            do_read_response_status();
        } else {
            SetException(BadConnect(std::string(ec.message())));
        }
    });
}

void network::httpRequester::do_read_response_status() {
    boost::asio::async_read_until(socket_,
                                  response,
                                  "\r\n",
                                  [this](boost::system::error_code ec, std::size_t bytes_transferred/*length*/)
                                  {
                                      if (!ec)
                                      {
                                          std::istream response_stream(&response);
                                          std::string http_version;
                                          response_stream >> http_version;
                                          std::cerr << http_version << std::endl;
                                          unsigned int status_code;
                                          response_stream >> status_code;
                                          std::string status_message;
                                          std::getline(response_stream, status_message);

                                          if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
                                              SetException(BadConnect("Invalid response\n"));
                                          } else if (status_code != 200) {
                                              SetException(BadConnect("Response returned with status code " + std::to_string(status_code) + "\n"));
                                          } else do_read_response_header();
                                      }
                                      else
                                      {
                                          SetException(BadConnect(std::string(ec.message())));
                                      }
                                  });
}

void network::httpRequester::do_read_response_header()  {
    boost::asio::async_read_until(socket_,
                                  response,
                                  "\r\n\r\n",
                                  [this](boost::system::error_code ec, std::size_t /*length*/)
                                  {
                                      if (!ec)
                                      {
                                          do_read_response_body();
                                      }
                                      else if (ec != boost::asio::error::eof)
                                      {
                                          SetException(BadConnect(std::string(ec.message())));
                                      }
                                  });
}

void network::httpRequester::do_read_response_body() {
    boost::asio::async_read(socket_,
                            response,
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
                                    SetException(BadConnect(std::string(ec.message())));
                                }
                            });
}