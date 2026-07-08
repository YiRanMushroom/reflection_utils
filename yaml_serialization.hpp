#pragma once

#include <memory>
#include <meta>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <tuple>
#include <vector>

#include "yaml_serialization.hpp"
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

        // template<typename T>
        // struct yaml_serialization_impl {
        //     YAML::Node serialize(const T &value) = delete(
        //         "Type is not serializable, please specialize yaml_serialization_impl.");
        // };
        //
        // template<typename T>
        // YAML::Node serialize(const T &value) {
        //     return yaml_serialization_impl<T>::serialize(value);
        // }

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

        // template<typename T>
        //     requires (is_simple_accessible_class<T>())
        // struct yaml_serialization_impl<T> {
        //     static YAML::Node serialize(const T &value) {
        //         constexpr static auto
        //                 non_static_data_members = std::define_static_array(
        //                     all_data_members_of(^^T, std::meta::access_context::unprivileged()));
        //
        //         YAML::Node map = YAML::Node(YAML::NodeType::Map);
        //
        //         template for (constexpr std::meta::info member_info: non_static_data_members) {
        //             map[identifier_of(member_info)] = _details::serialize(value.[:member_info:]);
        //         }
        //
        //         return map;
        //     }
        // };
        //
        // template<typename T1, typename T2>
        // struct yaml_serialization_impl<std::pair<T1, T2>> {
        //     static YAML::Node serialize(const std::pair<T1, T2> &value) {
        //         YAML::Node seq = YAML::Node(YAML::NodeType::Sequence);
        //         seq.push_back(_details::serialize(value.first));
        //         seq.push_back(_details::serialize(value.second));
        //         return seq;
        //     }
        // };
        //
        // template<typename... Ts>
        // struct yaml_serialization_impl<std::tuple<Ts...>> {
        //     static YAML::Node serialize(const std::tuple<Ts...> &value) {
        //         YAML::Node seq = YAML::Node(YAML::NodeType::Sequence);
        //         template for (const auto &vs: value) {
        //             seq.push_back(_details::serialize(vs));
        //         }
        //         return seq;
        //     }
        // };
        //
        // template<std::ranges::range R> requires (!std::is_same_v<R, std::string>)
        // struct yaml_serialization_impl<R> {
        //     static YAML::Node serialize(const R &value) {
        //         YAML::Node seq = YAML::Node(YAML::NodeType::Sequence);
        //         for (const auto &item: value) {
        //             seq.push_back(_details::serialize(item));
        //         }
        //         return seq;
        //     }
        // };
        //
        // template<typename T>
        //     requires (std::is_same_v<T, std::string> || std::is_null_pointer_v<T> || std::is_integral_v<T> ||
        //               std::is_floating_point_v<T>)
        // struct yaml_serialization_impl<T> {
        //     static YAML::Node serialize(const T &value) {
        //         return YAML::Node(value);
        //     }
        // };
    }

    using _details::all_data_members_of;
    using _details::is_simple_accessible_class;
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
                if (node[std::meta::identifier_of(member)].IsDefined()) {
                    bool success = convert<typename [:std::meta::type_of(member):]>::decode(
                        node[std::meta::identifier_of(member)], new_value.[:member:]);
                    if (!success) {
                        return false; // Failed to decode member
                    }
                } else {
                    return false; // Missing member in the YAML node
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

            template for (const auto idx: std::index_sequence_for<Ts...>()) {
                if (!convert<std::tuple_element_t<idx,
                    std::tuple<Ts...>>>::decode(node[idx], std::get<idx>(new_value))) {
                    return false; // Failed to decode element
                }
            }

            value = std::move(new_value);
            return true;
        }
    };
}
