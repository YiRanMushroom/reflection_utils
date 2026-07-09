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
    std::string string_value [[=reflection_utils::annotations::default_construct]] = "string";
    double double_value [[=reflection_utils::annotations::default_construct]];
    int int_value;
    char char_value[[=reflection_utils::annotations::default_construct]];
    std::vector<std::string> vec [[=reflection_utils::annotations::default_construct]];
    std::unordered_map<std::string, int> map [[=reflection_utils::annotations::default_construct]];
    std::tuple<int, double, std::pair<std::string, std::vector<size_t>>> tup;
};

enum class test_enum : uint32_t {
    one,
    two,
    three,
    four
};

class string_literal {
public:
    consteval string_literal(std::string_view sv) : str(std::define_static_string(sv)), sz(sv.size()) {}

    template<size_t N>
    consteval string_literal(const char (&str)[N]) : string_literal(std::string_view(str, N - 1)) {}

    [[nodiscard]] constexpr std::string_view view() const {
        return {str, sz};
    }

public:
    const char *str;
    size_t sz;
};

constexpr static string_literal sl = "123";
constexpr static auto view = std::string_view(sl.view());

template<string_literal some_literal>
void print_literal() {
    std::println("Literal: {}", some_literal.view());
}

int main() {
    print_literal<"literal">();
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

    // std::println("{}", Dump(node));

    try {
        constexpr static auto yaml_literal = R"(a: 1
b: 2
double_value: 3.14
int_value: 42
vec:
  - one
  - two
  - three
map:
  key2: 2
  key1: 1
tup:
  - 1
  - 2.5
  -
    - tuple_string
    -
      - 10
      - 20
      - 30
)";
        auto yaml_obj = YAML::Load(yaml_literal);
        auto cls = yaml_obj.as<some_class>();
        auto get_yaml = YAML::Node(cls);
        std::println("{}", Dump(get_yaml));
    } catch (std::exception &e) {
        std::println("Exception: {}", e.what());
    }
}
