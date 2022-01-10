#include "auxiliary.h"

#include <algorithm>

#include <boost/function_output_iterator.hpp>
#include <boost/compute/detail/sha1.hpp>

std::string GetSHA1(const std::string &p_arg){
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(p_arg.data(), p_arg.size());
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

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

void SHA1toBE(const std::string &sha1_str, uint8_t * arr) {
    uint8_t arr_hash[20];
    size_t j = 0;
    for (auto el : sha1_str){
        arr_hash[j++] = el;
    }
    for (size_t i = 0; i < 20; i+=4){
        swap_endian<int32_t>(&arr_hash[i]).AsArray(&arr[i]);
    }
}

