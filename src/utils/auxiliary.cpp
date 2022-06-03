#include "auxiliary.h"

#include <algorithm>
#include <cstring>
#include <cmath>

#include <boost/compute/detail/sha1.hpp>
#include <boost/function_output_iterator.hpp>

bool is_little_endian() {
    static bool is_little = [] {
        int num = 1;
        return (*reinterpret_cast<char *>(&num) == 1);
    }();
    return is_little;
}

std::string GetSHA1(const std::string &p_arg) {
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(p_arg.data(), p_arg.size());
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

    union value_type {
        unsigned full;
        unsigned char u8[sizeof(unsigned)];
    } dest{};
    for (auto &el : hash) {
        value_type source{};
        source.full = el;

        for (size_t k = 0; k < sizeof(unsigned); k++) {
            dest.u8[k] = source.u8[sizeof(unsigned) - k - 1];
        }
        el = dest.full;
    }

    char str_hash[sizeof(unsigned) * 5];

    memcpy(str_hash, (char *)&hash, sizeof(unsigned) * 5);
    return {str_hash, std::size(str_hash)};
}

static std::string encode_impl(std::string::value_type symb) {
    if (isalnum(symb) || symb == '-' || symb == '_' || symb == '.' || symb == '~') return {symb};

    std::stringstream enc_ss;
    enc_ss << '%' << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
           << static_cast<unsigned int>(static_cast<unsigned char>(symb));
    return std::move(enc_ss.str());
}

std::string UrlEncode(const std::string &url_to_encode) {
    std::string qstr;
    std::transform(url_to_encode.begin(), url_to_encode.end(),
        boost::make_function_output_iterator([&](const std::string &byte) -> void { qstr.append(byte); }), encode_impl);
    return std::move(qstr);
}

int IpToInt(const std::string &ip_address) {
    int res{};
    std::istringstream ssin(ip_address);
    int val{};
    char delim{};

    do {
        ssin >> val;
        res <<= CHAR_BIT;
        res |= val;
    } while (ssin >> delim);

    return res;
}

std::string IpToStr(size_t ip) {
    std::vector<uint8_t> ip_address;
    while (ip) {
        ip_address.push_back(ip & 255);
        ip >>= 8;
    }
    std::reverse(ip_address.begin(), ip_address.end());

    std::string ip_address_str;
    bool is_first = true;
    for (auto &el : ip_address) {
        if (!is_first) {
            ip_address_str += '.';
        }
        is_first = false;
        ip_address_str += std::to_string(el);
    }

    return std::move(ip_address_str);
}

std::string BytesToHumanReadable(uint32_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";

    auto exp = static_cast<size_t>(std::log(bytes) / std::log(1024));
    const char *ci = "kMGTPE";

    std::ostringstream os;
    os << static_cast<double>(bytes / std::pow(1024, exp)) << " ";
    os << ci[exp - 1] << "B";

    return os.str();
}

long double BytesToGiga(long long bytes) {
    return static_cast<long double>(bytes) / powf(1024.f, 3);
}

unsigned long long GigaToBytes(long double gigabytes) {
    return gigabytes * powf(1024.f, 3);
}

unsigned char ReverseByte(unsigned char b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}