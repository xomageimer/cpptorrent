#ifndef CPPTORRENT_AUXILIARY_H
#define CPPTORRENT_AUXILIARY_H

#include <climits>
#include <optional>
#include <string>

#include "constants.h"

std::string GetSHA1(const std::string &p_arg);
std::string UrlEncode(std::string const &url_to_encode);
int IpToInt(std::string const &ip_address);
std::string IpToStr(size_t ip);

std::string BytesToHumanReadable(uint32_t bytes);
double long BytesToGiga(long long bytes);
unsigned long long GigaToBytes(long double gigabytes);
unsigned char ReverseByte(unsigned char b);

template <typename T> struct endian_changer {
    static_assert(CHAR_BIT == bittorrent_constants::byte_size, "CHAR_BIT != 8");

private:
    union value_type {
        T full;
        unsigned char u8[sizeof(T)];
    } dest;

public:
    explicit endian_changer(T u) {
        value_type source{};
        source.full = u;

        for (size_t k = 0; k < sizeof(T); k++) {
            dest.u8[k] = source.u8[sizeof(T) - k - 1];
        }
    }
    explicit endian_changer(const char *arr) {
        value_type source{};
        for (size_t k = 0; k < sizeof(T); k++) {
            source.u8[k] = arr[k];
        }

        for (size_t k = 0; k < sizeof(T); k++) {
            dest.u8[k] = source.u8[sizeof(T) - k - 1];
        }
    }

    void AsArray(uint8_t *arr) const {
        for (size_t k = 0; k < sizeof(T); k++) {
            arr[k] = dest.u8[k];
        }
    }
    [[nodiscard]] T AsValue() const { return dest.full; }
};

bool is_little_endian();

template <typename T> T BigToNative(T value) {
    if (is_little_endian())
        return endian_changer(value).AsValue();
    else return value;
}

template <typename T> T NativeToBig(T value) {
    if (is_little_endian())
        return endian_changer(value).AsValue();
    else return value;
}

template <typename T> T LittleToNative(T value) {
    if (!is_little_endian())
        return endian_changer(value).AsValue();
    else return value;
}

template <typename T> T NativeToLittle(T value) {
    if (!is_little_endian())
        return endian_changer(value).AsValue();
    else return value;
}

template <typename T> void ValueToArray(T value, uint8_t *arr) {
    union value_type {
        T full;
        unsigned char u8[sizeof(T)];
    } smart_value{value};
    memcpy(arr, &smart_value.u8, sizeof(T));
}

template <typename T> T ArrayToValue(const uint8_t *arr) {
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
