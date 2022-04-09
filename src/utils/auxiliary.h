#ifndef CPPTORRENT_AUXILIARY_H
#define CPPTORRENT_AUXILIARY_H

#include <climits>
#include <optional>
#include <string>

#include "constants.h"

std::string GetSHA1(const std::string &p_arg);
std::string UrlEncode(std::string const &url_to_encode);
int IpToInt(std::string const &ip_address);

std::string BytesToHumanReadable(uint32_t bytes);
double long BytesToGiga(long long bytes);
unsigned long long GigaToBytes(long double gigabytes);

// TODO сделать специальный message кт и будет отправляться и котоырй будет определять порядок байт при конструировании, например: \
    OutputMessage out(ByteOrder::BigEndian, 17);
bool is_little_endian();
// TODO мб неудачное название! + переделать по виду функций типа ReadBE(...) / WriteLE(...) и тп.
template <typename T> struct as_big_endian {
    static_assert(CHAR_BIT == bittorrent_constants::byte_size, "CHAR_BIT != 8");

private:
    union value_type {
        T full;
        unsigned char u8[sizeof(T)];
    } dest;
    static inline const bool is_native_are_little = is_little_endian();

public:
    explicit as_big_endian(T u) {
        value_type source{};
        source.full = u;

        for (size_t k = 0; k < sizeof(T); k++) {
            if (is_native_are_little)
                dest.u8[k] = source.u8[sizeof(T) - k - 1];
            else
                dest.u8[k] = source.u8[k];
        }
    }
    explicit as_big_endian(const unsigned char *u_arr) {
        value_type source{};
        for (size_t k = 0; k < sizeof(T); k++) {
            source.u8[k] = u_arr[k];
        }

        for (size_t k = 0; k < sizeof(T); k++) {
            if (is_native_are_little)
                dest.u8[k] = source.u8[sizeof(T) - k - 1];
            else
                dest.u8[k] = source.u8[k];
        }
    }

    void AsArray(uint8_t *arr) const {
        for (size_t k = 0; k < sizeof(T); k++) {
            arr[k] = dest.u8[k];
        }
    }
    [[nodiscard]] T AsValue() const { return dest.full; }
};

template <typename T> T SwapEndian(T value) {
    return as_big_endian(value).AsValue();
}
template <typename T> void ValueToArray(T value, uint8_t *arr) {
    while (value) {
        *arr = value % 10;
        arr++;
        value /= 10;
    }
}
template <typename T> T ArrayToValue(uint8_t *arr) {
    union value_type {
        T full;
        unsigned char u8[sizeof(T)];
    } smart_value;
    for (size_t i = 0; i < sizeof(T); i++) {
        smart_value.u8[i] = arr[i];
    }
    return smart_value.full;
}

template <typename T, typename Enable = void> struct is_optional : std::false_type {};

template <typename T> struct is_optional<std::optional<T>> : std::true_type {};

#endif // CPPTORRENT_AUXILIARY_H
