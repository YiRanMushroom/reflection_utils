#pragma once

#include <memory>
#include <meta>
#include <numeric>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <tuple>
#include <vector>

#include "yaml-cpp/yaml.h"

namespace reflection_utils {
    namespace _details {
        consteval std::vector<std::meta::info> all_data_members_of(std::meta::info class_type,
                                                                   std::meta::access_context access_context =
                                                                           std::meta::access_context::current()) {
            auto bases = std::meta::bases_of(class_type, access_context)
                         | std::ranges::views::transform(std::meta::type_of);
            std::vector<std::meta::info> all_info;
            for (std::meta::info base: bases) {
                for (std::meta::info base_members: all_data_members_of(base, access_context)) {
                    all_info.push_back(base_members);
                }

                if (std::meta::is_virtual(base)) {
                    throw std::meta::exception("Virtual base classes are not supported for serialization.", class_type);
                }
            }

            for (std::meta::info my_members: std::meta::nonstatic_data_members_of(class_type, access_context)) {
                all_info.push_back(my_members);
            }

            return all_info;
        }

        template<typename T> requires std::is_class_v<T>
        consteval bool is_simple_accessible_class() {
            if constexpr (!std::is_default_constructible_v<T>) {
                return false;
            } else {
                try {
                    return all_data_members_of(^^T, std::meta::access_context::unprivileged()).size()
                           == all_data_members_of(^^T, std::meta::access_context::unchecked()).size();
                } catch (...) {
                    return false;
                }
            }
        }
    }

    namespace annotations {
        struct default_construct_type {};

        template<typename T>
        struct default_value_t : default_construct_type {
        public:
            T value{};

            consteval default_value_t(const T &t) requires std::copy_constructible<T> : value(t) {}

            consteval default_value_t(const T &t) requires (std::is_array_v<T> && std::is_trivially_copyable_v<T>) {
                std::ranges::copy(std::begin(t), std::end(t), value);
            }

            template<typename U> requires std::is_convertible_v<T, U>
            constexpr U construct() const {
                return U{value};
            }
        };

        template<>
        struct default_value_t<void> {
        public:
            template<typename U> requires std::is_default_constructible_v<U>
            constexpr static U construct() {
                return U{};
            }
        };

        template<typename T>
        consteval default_value_t<T> default_value(const T &t) {
            return default_value_t<T>(t);
        }

        consteval default_value_t<void> default_value() {
            return default_value_t<void>{};
        }

        template<typename Fn>
        struct default_construct_by_t : default_construct_type {
            Fn fn{};

            consteval default_construct_by_t(Fn f) : fn(std::move(f)) {}

            template<typename U> requires std::convertible_to<std::invoke_result_t<Fn>, U>
            constexpr U construct() const {
                return fn();
            }
        };

        template<typename Fn>
        consteval default_construct_by_t<Fn> default_construct_by(Fn fn) {
            return default_construct_by_t<Fn>(std::move(fn));
        }
    }

    using _details::all_data_members_of;
    using _details::is_simple_accessible_class;

    namespace views {
        struct to_optional_fn : std::ranges::range_adaptor_closure<to_optional_fn> {
            template<std::ranges::input_range R>
            consteval auto operator()(R &&r) const {
                auto vec = r | std::ranges::to<std::vector>();

                using value_type = std::ranges::range_value_t<R>;

                if (vec.empty()) {
                    return std::optional<value_type>{std::nullopt};
                }

                if (vec.size() > 1) {
                    throw std::runtime_error("Expected at most one element in the range, but got more than one.");
                }

                return std::optional<value_type>{vec[0]};
            }
        };

        inline constexpr to_optional_fn to_optional{};
    }
}

namespace YAML {
    template<typename T> requires (reflection_utils::is_simple_accessible_class<T>())
    struct convert<T> {
        static Node encode(const T &value) {
            Node node(NodeType::Map);
            constexpr static auto data_members = std::define_static_array(
                reflection_utils::all_data_members_of(^^T, std::meta::access_context::unprivileged()));

            template for (constexpr auto member: data_members) {
                node[std::meta::identifier_of(member)] = value.[:member:];
            }

            return node;
        }

        static bool decode(const Node &node, T &value) {
            if (!node.IsMap()) {
                return false;
            }

            constexpr static auto data_members = std::define_static_array(
                reflection_utils::all_data_members_of(^^T, std::meta::access_context::unprivileged()));

            T new_value{};

            template for (constexpr auto member: data_members) {
                constexpr std::optional<std::meta::info> default_value_optional =
                        std::meta::annotations_of(member)
                        | std::ranges::views::filter([](std::meta::info annotation_info)consteval {
                            return std::meta::is_base_of_type(^^reflection_utils::annotations::default_construct_type,
                                                              std::meta::type_of(annotation_info));
                        })
                        | reflection_utils::views::to_optional;


                if (!(node[std::meta::identifier_of(member)].IsDefined() &&
                      convert<typename [:std::meta::type_of(member):]>::decode(
                          node[std::meta::identifier_of(member)], new_value.[:member:]))) {
                    if constexpr (default_value_optional.has_value()) {
                        new_value.[:member:] = [:std::meta::constant_of(*default_value_optional):].template construct<
                            typename [:std::meta::type_of(member):]>();
                    } else {
                        return false;
                    }
                }
            }

            value = std::move(new_value);
            return true;
        }
    };

    template<typename... Ts>
    struct convert<std::tuple<Ts...>> {
        static Node encode(const std::tuple<Ts...> &value) {
            Node seq(NodeType::Sequence);
            template for (const auto vs: value) {
                seq.push_back(vs);
            }
            return seq;
        }

        static bool decode(const Node &node, std::tuple<Ts...> &value) {
            if (!node.IsSequence() || node.size() != sizeof...(Ts)) {
                return false;
            }

            std::tuple<Ts...> new_value;

            template for (constexpr auto idx: std::views::iota(std::size_t{0}, sizeof...(Ts))) {
                if (!convert<std::tuple_element_t<idx,
                    std::tuple<Ts...>>>::decode(node[idx], std::get<idx>(new_value))) {
                    return false;
                }
            }

            value = std::move(new_value);
            return true;
        }
    };

    template<typename T>
    struct convert<std::optional<T>> {
        static Node encode(const std::optional<T> &value) {
            if (value.has_value()) {
                return convert<T>::encode(value.value());
            }
            return Node(NodeType::Null);
        }

        static bool decode(const Node &node, std::optional<T> &value) {
            if (!node.IsDefined() || node.IsNull()) {
                value.reset();
                return true;
            }

            T temp{};

            if (!convert<T>::decode(node, temp)) {
                return false;
            }

            value = std::move(temp);

            return true;
        }
    };
}
