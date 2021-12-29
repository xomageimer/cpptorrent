#ifndef QTORRENT_BENCODE_LIB_H
#define QTORRENT_BENCODE_LIB_H

#include <iostream>
#include <vector>
#include <map>
#include <functional>
#include <optional>
#include <variant>

namespace bencode {
    struct Node;
    using variant_inheritance = std::variant<long long, std::string, std::vector<Node>, std::map<std::string, Node>>;

    struct Node : public bencode::variant_inheritance {
    public:
        using variant_inheritance::variant_inheritance;

        [[nodiscard]] const bencode::variant_inheritance &GetOrigin() const {
            return *this;
        }

        template <typename T>
        std::optional<T> const & TryAs() const {
            return (std::holds_alternative<T>(*this)) ? std::get<T>(*this) : std::nullopt;
        }

        [[nodiscard]] long long AsNumber() const {
            return std::get<long long>(*this);
        }

        [[nodiscard]] std::string const &AsString() const {
            return std::get<std::string>(*this);
        }

        [[nodiscard]] std::vector<Node> const &AsArray() const {
            return std::get<std::vector<Node>>(*this);
        }

        [[nodiscard]] std::map<std::string, Node> const &AsDict() const {
            return std::get<std::map<std::string, Node>>(*this);
        }

        [[nodiscard]] const Node & operator[](std::string const & key) const {
            return AsDict().at(key);
        }

        [[nodiscard]] std::optional<std::reference_wrapper<const Node>> TryAt(std::string const & key) const {
            if (AsDict().count(key))
                return std::ref(AsDict().at(key));
            return std::nullopt;
        }

        [[nodiscard]] const Node & operator[](size_t i) const {
            return AsArray().at(i);
        }
    };

    struct Document {
    private:
        Node root;
    public:
        explicit Document(Node new_root) : root(std::move(new_root)) {};

        [[nodiscard]] inline Node const &GetRoot() const {
            return root;
        }
    };

    namespace Deserialize {
        Node LoadNode(std::istream &input);

        Node LoadDict(std::istream &input);

        Node LoadArray(std::istream &input);

        Node LoadStr(std::istream &input);

        Node LoadNumber(std::istream &input);

        inline Document Load(std::istream & input) {
            return std::move(Document{Deserialize::LoadNode(input)});
        }
    }

    namespace Serialize {
        template <typename T>
        void MakeSerialize(const T & bencode_node, std::ostream &out = std::cout) {
            std::visit([&out](auto const & arg){
                bencode::Serialize::MakeSerialize<std::decay_t<decltype(arg)>>(arg, out);
            }, bencode_node.GetOrigin());
        }

        template <>
        void MakeSerialize<long long>(const long long &bencode_int, std::ostream &out);

        template <>
        void MakeSerialize<std::string>(const std::string & bencode_str, std::ostream &out);

        template <>
        void MakeSerialize<std::vector<Node>>(const std::vector<Node> & bencode_arr, std::ostream &out);

        template <>
        void MakeSerialize<std::map<std::string, Node>>(const std::map<std::string, Node> & bencode_dict, std::ostream &out);
    }
}


#endif //QTORRENT_BENCODE_LIB_H
