#ifndef CPPTORRENT_AUXILIARY_H
#define CPPTORRENT_AUXILIARY_H

#include <optional>
#include <string>

std::string GetSHA1(const std::string& p_arg);
std::string UrlEncode(std::string const & url_to_encode);
int IpToInt(std::string const & ip_address);

template <typename T, typename Enable = void>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

#endif //CPPTORRENT_AUXILIARY_H
