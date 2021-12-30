#include "AsyncTracker.h"

#include <iostream>

#include "Tracker.h"
#include "auxiliary.h"

void network::AsyncTracker::Connect(const tracker::Query &query) {
    ba::ip::tcp::resolver::query resolver_query(tracker_->GetUrl().Host, tracker_->GetUrl().Port);
    boost::system::error_code ec;
    ba::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(resolver_query, ec);

    if (ec)
        throw BadConnect(ec.message());

    do_connect(endpoint_iterator, query);
}

void network::AsyncTracker::do_connect(
        const boost::asio::ip::basic_resolver<boost::asio::ip::tcp, boost::asio::executor>::iterator &endpoint_iterator, const tracker::Query &query) {
    boost::asio::async_connect(socket_, endpoint_iterator,
                               [&, this](boost::system::error_code ec, ba::ip::tcp::resolver::iterator){
                                   if (!ec){
                                       do_request(query);
                                   } else {
                                       throw BadConnect(std::string(ec.message()));
                                   }
                               });
}

void network::AsyncTracker::do_request(const tracker::Query &query) {
    ba::streambuf request_query;
    std::ostream request_stream(&request_query);
    request_stream << "GET /" << tracker_->GetUrl().Path.value_or("") << "?"

                   << "info_hash=" << UrlEncode(tracker_->GetInfoHash())
                   << "&peer_id=" << UrlEncode(std::to_string(tracker_->GetMasterPeerId()))
                   << "&port=" << tracker_->GetPort() << "&uploaded=" << query.uploaded << "&downloaded="
                   << query.downloaded << "&left=" << query.left << "&compact=1"
                   << ((query.event != tracker::Event::Empty) ? "&event=" + tracker::events_str.at(query.event) : "")

                   << " HTTP/1.0\r\n"
                   << "Host: " << tracker_->GetUrl().Host << "\r\n"
                   << "Accept: */*\r\n"
                   << "Connection: close\r\n\r\n";

    std::cerr << tracker_->GetUrl().Host << std::endl;
    boost::asio::async_write(socket_, request_query,  [this](boost::system::error_code ec, std::size_t /*length*/){
        if (!ec) {
            do_read_response_status();
        } else {
            socket_.close();
            throw BadConnect(std::string(ec.message()));
        }
    });
}

void network::AsyncTracker::do_read_response_status() {
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
                                          throw BadConnect(std::string(ec.message()));
                                      }
                                  });
}

void network::AsyncTracker::do_read_response_header()  {
    boost::asio::async_read_until(socket_,
                                  response,
                                  "\r\n\r\n",
                                  [this](boost::system::error_code ec, std::size_t /*length*/)
                                  {
                                      if (!ec)
                                      {
                                          std::istream response_stream(&response);
                                          std::string header;
                                          while (std::getline(response_stream, header) && header != "\r")
                                              std::cout << header << "\n" << std::flush;
                                          std::cout << "\n" << std::flush;

                                          if (response.size() > 0)
                                              std::cout << &response << std::flush;

                                          do_read_response_body();
                                      }
                                      else
                                      {
                                          socket_.close();
                                      }
                                  });
}

void network::AsyncTracker::do_read_response_body() {
    boost::asio::async_read(socket_,
                            response,
                            [this](boost::system::error_code ec, std::size_t bytes_transferred/*length*/)
                            {
                                if (!ec)
                                {
                                    do_read_response_header();
                                }
                                else
                                {
                                    socket_.close();
                                    throw BadConnect(std::string(ec.message()));
                                }
                            });
}