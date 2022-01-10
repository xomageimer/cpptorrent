#include "auxiliary.h"

#include <algorithm>

#include <boost/function_output_iterator.hpp>
#include <boost/compute/detail/sha1.hpp>

bool is_little_endian()
{
    int num = 1;
    return (*reinterpret_cast<char *>(&num) == 1);
}

std::string GetSHA1(const std::string &p_arg){
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(p_arg.data(), p_arg.size());
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

    if (is_little_endian()) {
        for (auto &el: hash)
            el = swap_endian(el).AsValue();
    }

    char str_hash[sizeof(unsigned) * 5];

    memcpy(str_hash, (char *)&hash, sizeof(unsigned) * 5);
    return {str_hash, std::size(str_hash)};
}

static std::string encode_impl(std::string::value_type symb) {
    if (isalnum(symb) || symb == '-' || symb == '_' || symb == '.' || symb == '~')
        return {symb};

    std::stringstream enc_ss;
    enc_ss << '%' << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
           << static_cast<unsigned int>(static_cast<unsigned char>(symb));
    return std::move(enc_ss.str());
}

std::string UrlEncode(const std::string &url_to_encode){
    std::string qstr;
    std::transform(url_to_encode.begin(), url_to_encode.end(),
                   boost::make_function_output_iterator([&](const std::string& byte) -> void {
                       qstr.append(byte);
                   }),
                   encode_impl);
    return std::move(qstr);
}

int IpToInt(const std::string &ip_address) {
    return 0;
}

