#include "TrackerRequester.h"

#include <iostream>
#include <utility>

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
    socket_.close();
}

void network::TrackerRequester::SetException(const network::BadConnect &exc) {
    promise_of_resp.set_exception(exc);
    socket_.close();
}

void network::httpRequester::Connect(const tracker::Query &query) {
    resolver_.async_resolve(tracker_.lock()->GetUrl().Host, tracker_.lock()->GetUrl().Port,
                            [&](boost::system::error_code const & ec,
                                    ba::ip::tcp::endpoint enpoints){
        if (!ec) {

        }
    });
}
