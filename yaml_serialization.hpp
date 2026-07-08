#pragma once

#include <memory>
#include <meta>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "yaml_serialization.hpp"
#include "yaml-cpp/yaml.h"

namespace reflection_utils {
    namespace _details {
        class yaml_serialization_context;

        template<typename T>
        concept can_be_serded =
                (!std::is_reference_v<T>) &&
                (!std::is_function_v<T> && !std::is_union_v<T> && !std::is_pointer_v<T>)
                && (std::is_default_constructible_v<T> && !std::is_const_v<T>);

        template<typename T>
        concept can_be_referenced_serded =
                can_be_serded<T> && (
                    !std::is_class_v<T>
                    ||
                    (std::is_class_v<T> && std::is_final_v<T>)
                );

        template<can_be_serded T>
        struct serialize_impl {
            static YAML::Node serialize(yaml_serialization_context &context, const T &value);
        };

        class yaml_serialization_context {
        public:
            template<can_be_serded T>
            friend struct serialize_impl;

            template<can_be_serded T>
            YAML::Node serialize(const T &value) {
                return serialize_impl<T>::serialize(*this, value);
            }

            YAML::Node output_shared_objects() const {
                YAML::Node map = YAML::Node(YAML::NodeType::Map);

                for (const auto &[ptr, node]: m_shared_objects) {
                    map[reinterpret_cast<std::uintptr_t>(ptr)] = node;
                }

                return map;
            }

            template<can_be_referenced_serded T>
            YAML::Node get_or_register_node(const T &value) {
                void *address = static_cast<void *>(const_cast<T *>(&value));
                auto it = m_shared_objects.find(address);

                if (it != m_shared_objects.end()) {
                    return it->second;
                }

                YAML::Node node = m_shared_objects[address] = YAML::Node();

                YAML::Node temp = this->serialize(value);

                if (temp.IsMap()) {
                    for (const auto &pair: temp) {
                        node[pair.first] = pair.second;
                    }
                } else if (temp.IsSequence()) {
                    for (const auto &item: temp) {
                        node.push_back(item);
                    }
                } else {
                    node = temp;
                }

                return node;
            }

        private:
            std::unordered_map<void *, YAML::Node> m_shared_objects;
        };

        template<can_be_serded T>
        YAML::Node serialize_impl<T>::serialize(yaml_serialization_context &context,
                                                const T &value) {
            if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_same_v<T, std::string>) {
                return YAML::Node(value);
            } else if constexpr (std::is_class_v<T>) {
                YAML::Node node(YAML::NodeType::Map);

                constexpr static auto non_static_data_members = std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));

                template for (constexpr auto member: non_static_data_members) {
                    constexpr std::string_view name = std::meta::identifier_of(member);
                    node[name] = context.serialize(value.[:member:]);
                }

                return node;
            } else {
                static_assert(can_be_serded<T>, "Type cannot be serialized");
            }
        }

        template<can_be_serded T>
        struct serialize_impl<std::shared_ptr<T>> {
            static YAML::Node serialize(yaml_serialization_context &context, const std::shared_ptr<T> &value) {
                if (!value) {
                    return YAML::Node(YAML::NodeType::Null);
                }
                return context.get_or_register_node(*value);
            }
        };

        template<can_be_serded T>
        struct serialize_impl<std::weak_ptr<T>> {
            static YAML::Node serialize(yaml_serialization_context &context, const std::weak_ptr<T> &value) {
                if (auto shared = value.lock()) {
                    return context.get_or_register_node(*shared);
                }
                return YAML::Node(YAML::NodeType::Null);
            }
        };

        template<can_be_serded T>
        struct serialize_impl<std::unique_ptr<T>> {
            static YAML::Node serialize(yaml_serialization_context &context, const std::unique_ptr<T> &value) {
                if (!value) {
                    return YAML::Node(YAML::NodeType::Null);
                }
                return context.get_or_register_node(*value);
            }
        };

        template<can_be_serded T>
        struct serialize_impl<std::optional<T>> {
            static YAML::Node serialize(yaml_serialization_context &context, const std::optional<T> &value) {
                if (!value.has_value()) {
                    return YAML::Node(YAML::NodeType::Null);
                }
                return context.serialize(value.value());
            }
        };

        template<can_be_serded T1, can_be_serded T2>
        struct serialize_impl<std::pair<T1, T2>> {
            static YAML::Node serialize(yaml_serialization_context &context, const std::pair<T1, T2> &value) {
                YAML::Node node(YAML::NodeType::Sequence);
                node.push_back(context.serialize(value.first));
                node.push_back(context.serialize(value.second));
                return node;
            }
        };

        template<can_be_serded... T>
        struct serialize_impl<std::tuple<T...>> {
            static YAML::Node serialize(yaml_serialization_context &context, const std::tuple<T...> &value) {
                YAML::Node node(YAML::NodeType::Sequence);
                template for (auto member : value) {
                    node.push_back(context.serialize(member));
                }

                return node;
            }
        };

        template<can_be_serded T>
        struct serialize_impl<std::vector<T>> {
            static YAML::Node serialize(yaml_serialization_context &context, const std::vector<T> &value) {
                YAML::Node node(YAML::NodeType::Sequence);
                for (const auto &item: value) {
                    node.push_back(context.serialize(item));
                }
                return node;
            }
        };
    }

    using _details::yaml_serialization_context;
}
