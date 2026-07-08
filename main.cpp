#include <meta>
#include <format>
#include <iostream>
#include <string_view>
#include <print>

#include "yaml_serialization.hpp"

struct some_class final {
    std::string string_value;
    double double_value;
    int int_value;
    char char_value;

    std::shared_ptr<some_class> shared_ptr_value;
    std::weak_ptr<some_class> weak_ptr_value;
};

int main() {
    auto head = std::make_shared<some_class>();
    auto tail = std::make_shared<some_class>();
    head->shared_ptr_value = tail;
    tail->weak_ptr_value = head;

    reflection_utils::yaml_serialization_context context;
    auto serialized = context.serialize(*head);
    std::println("{}", Dump(serialized));
}