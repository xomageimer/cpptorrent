#include "KRPC.h"

#include "RouteTable.h"

network::KRPCQuery::KRPCQuery(bencode::Document doc, dht::RouteTable &route_table) : doc_(std::move(doc)), route_table_(route_table){};
