#include <meta>
#include <format>
#include <iostream>
#include <string_view>
#include <print>
#include <tuple>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include "yaml_serialization.hpp"

struct base_A {
    int a;
};

struct base_B : base_A {
    int b;
};


struct some_class : base_B {
    std::string string_value;
    double double_value;
    int int_value;
    char char_value;
    std::vector<std::string> vec;
    std::unordered_map<std::string, int> map;
    std::tuple<int, double, std::pair<std::string, std::vector<size_t>>> tup;
};

enum class test_enum : uint32_t {
    one,
    two,
    three,
    four
};

int main() {
    some_class cls = {
        {base_A{.a = 1}, 2},

        "Hello", 3.14, 42, 'A',
        {"one", "two", "three"},
        {{"key1", 1}, {"key2", 2}},
        {1, 2.5, {"tuple_string", {10, 20, 30}}}
    };

    constexpr static auto non_static_data_members = std::define_static_array(
        reflection_utils::_details::all_data_members_of(^^std::vector<int>, std::meta::access_context::unchecked()));


    template for (constexpr std::meta::info member_info: non_static_data_members) {
        std::println("Member: {}", std::meta::display_string_of(member_info));
    }

    test_enum en{test_enum::one};

    constexpr static auto enums = std::define_static_array(std::meta::enumerators_of(^^test_enum));

    YAML::Node node(cls);

    std::println("{}", Dump(node));
}
