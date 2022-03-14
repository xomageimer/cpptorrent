#include "bencode_lib.h"

#include <algorithm>

bencode::Node bencode::Deserialize::LoadNode(std::istream &input) {
    char c;
    c = input.peek();
    switch (c) {
        case 'i':
            return bencode::Deserialize::LoadNumber(input);
        case 'l':
            return bencode::Deserialize::LoadArray(input);
        case 'd':
        case EOF:
            return bencode::Deserialize::LoadDict(input);
        default:
            return bencode::Deserialize::LoadStr(input);
    }
}

bencode::Node bencode::Deserialize::LoadDict(std::istream &input) {
    std::map<std::string, Node> dict;
    if (input.peek() == EOF) return std::move(dict);

    input.get();
    for (;;) {
        if (input.peek() == 'e')
            break;
        std::string key = LoadStr(input).AsString();
        dict.emplace(key, LoadNode(input));
    }
    input.get();

    return std::move(dict);
}

bencode::Node bencode::Deserialize::LoadArray(std::istream &input) {
    std::vector<Node> arr;

    input.get();
    for (;;) {
        if (input.peek() == 'e')
            break;
        arr.emplace_back(LoadNode(input));
    }
    input.get();

    return std::move(arr);
}

bencode::Node bencode::Deserialize::LoadStr(std::istream &input) {
    std::string str;
    size_t str_size;

    input >> str_size;
    str.reserve(str_size);
    input.get();
    std::copy_n(std::istreambuf_iterator(input), str_size, std::back_inserter(str));
    input.get();

    return std::move(str);
}

bencode::Node bencode::Deserialize::LoadNumber(std::istream &input) {
    long long number;

    input.get();
    input >> number;
    input.get();

    return number;
}

template<>
void bencode::Serialize::MakeSerialize<std::string>(const std::string &bencode_str, std::ostream &out) {
    out << bencode_str.size() << ':' << bencode_str;
}

template<>
void bencode::Serialize::MakeSerialize<long long>(const long long &bencode_int, std::ostream &out) {
    out << 'i' << bencode_int << 'e';
}

template<>
void bencode::Serialize::MakeSerialize<std::vector<bencode::Node>>(const std::vector<Node> &bencode_arr, std::ostream &out) {
    out << 'l';
    for (auto &node_el: bencode_arr) {
        std::visit([&out](auto const &arg) {
            bencode::Serialize::MakeSerialize<std::decay_t<decltype(arg)>>(arg, out);
        },
                   node_el.GetOrigin());
    }
    out << 'e';
}

template<>
void bencode::Serialize::MakeSerialize<std::map<std::string, bencode::Node>>(const std::map<std::string, Node> &bencode_dict, std::ostream &out) {
    out << 'd';
    for (auto &[key, node_el]: bencode_dict) {
        bencode::Serialize::MakeSerialize(key, out);
        std::visit([&out](auto const &arg) {
            bencode::Serialize::MakeSerialize<std::decay_t<decltype(arg)>>(arg, out);
        },
                   node_el.GetOrigin());
    }
    out << 'e';
}