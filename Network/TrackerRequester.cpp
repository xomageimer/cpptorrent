#include "TrackerRequester.h"

#include <iostream>
#include <utility>

#include "auxiliary.h"

void network::TrackerRequester::SetResponse() {
    std::string resp_str{(std::istreambuf_iterator<char>(&response)), std::istreambuf_iterator<char>()} ;
    std::vector<std::string> urls_parts;
    boost::regex expression(
            "^d(?:.|\\n)*e\\Z"
    );
    if (!boost::regex_split(std::back_inserter(urls_parts), resp_str, expression))
    {
        return;
    }
    std::cout << urls_parts.front() << std::endl;

    std::stringstream ss (urls_parts.front());
    auto bencoder = bencode::Deserialize::Load(ss);
    const auto& bencode_resp = bencoder.GetRoot();

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
}

void network::TrackerRequester::SetException(const network::BadConnect &exc) {
    promise_of_resp.set_exception(exc);
}

void network::httpRequester::Connect(const tracker::Query &query) {
    ba::ip::tcp::resolver::query resolver_query(tracker_.lock()->GetUrl().Host, tracker_.lock()->GetUrl().Port);
    resolver.async_resolve(resolver_query, [query, this](boost::system::error_code ec, ba::ip::tcp::resolver::iterator endpoint_iterator) {
        if (ec) {
            SetException(BadConnect(ec.message()));
            return;
        }
        do_connect(std::move(endpoint_iterator), query);
    });
}

void network::httpRequester::do_connect(
        boost::asio::ip::basic_resolver<boost::asio::ip::tcp, boost::asio::executor>::iterator endpoint_iterator, const tracker::Query &query) {
    boost::asio::async_connect(socket_, std::move(endpoint_iterator),
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
    std::cerr << tracker_.expired() << std::endl;
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

//    std::cout << request_stream.rdbuf() << std::endl;
    boost::asio::async_write(socket_, request_query,  [this](boost::system::error_code ec, std::size_t /*length*/){
        if (!ec) {
            do_read_response_status();
        } else {
            socket_.close();
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
                                              socket_.close();
                                              throw BadConnect("Invalid response\n");
                                          }
                                          if (status_code != 200) {
                                              socket_.close();
                                              throw BadConnect("Response returned with status code " + std::to_string(status_code) + "\n");
                                          }

                                          do_read_response_header();
                                      }
                                      else
                                      {
                                          socket_.close();
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
                                          socket_.close();
                                          SetException(BadConnect(std::string(ec.message())));
                                      } else {
                                          SetResponse();
                                          socket_.close();
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
                                else if (ec != boost::asio::error::eof)
                                {
                                    socket_.close();
                                    SetException(BadConnect(std::string(ec.message())));
                                } else {
                                    SetResponse();
                                    socket_.close();
                                }
                            });
}