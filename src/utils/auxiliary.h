#ifndef CPPTORRENT_AUXILIARY_H
#define CPPTORRENT_AUXILIARY_H

#include <optional>
#include <string>

std::string GetSHA1(const std::string& p_arg);
std::string UrlEncode(std::string const & url_to_encode);
int IpToInt(std::string const & ip_address);

template <typename T>
struct swap_endian {
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");
private:
    union value_type
    {
        T full;
        unsigned char u8[sizeof(T)];
    } dest;

public:
    explicit swap_endian(T u) {
        value_type source{};
        source.full = u;

        for (size_t k = 0; k < sizeof(T); k++){
            dest.u8[k] = source.u8[sizeof(T) - k - 1];
        }
    }
    explicit swap_endian(const unsigned char * u_arr) {
        value_type source{};
        for (size_t k = 0; k < sizeof(T); k++) {
            source.u8[k] = u_arr[k];
        }

        for (size_t k = 0; k < sizeof(T); k++){
            dest.u8[k] = source.u8[sizeof(T) - k - 1];
        }
    }

    void AsArray(uint8_t * arr) const {
        for (size_t k = 0; k < sizeof(T); k++) {
            arr[k] = dest.u8[k];
        }
    }
    [[nodiscard]] T AsValue() const {
        return dest.full;
    }
};


template <typename T, typename Enable = void>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

#endif //CPPTORRENT_AUXILIARY_H
