#include <meta>
#include <format>
#include <iostream>
#include <string_view>
#include <print>

struct some_class {
    std::string string_value;
    double double_value;
    int int_value;
    char char_value;
};

int main() {
    constexpr std::meta::info ref = ^^void;

    template for (constexpr auto e : std::define_static_array(std::meta::members_of(^^std::meta, std::meta::access_context::unchecked()))) {
        if constexpr (std::meta::has_identifier(e)) {
            std::println("{}: {}", std::meta::identifier_of(e), std::meta::display_string_of(e));
        }
    }
}