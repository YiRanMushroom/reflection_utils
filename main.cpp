// #include <bits/stdc++.h>

import std.compat;

namespace __details {
    enum class line_kind { use, ns_open, ns_close, ns_alias };

    constexpr auto included_inline_namespaces = std::array{std::string_view("_Cpo")};

    struct line_record {
        line_kind kind;
        const char *name;
        const char *target;
        unsigned indent;
    };

    consteval line_record make_alias_line(std::string_view name, std::string_view target, unsigned indent) {
        return line_record{
            line_kind::ns_alias, std::define_static_string(name),
            std::define_static_string(target), indent
        };
    }


    consteval line_record make_line(line_kind kind, std::string_view name, unsigned indent) {
        return line_record{.kind = kind, .name = std::define_static_string(name), .indent = indent};
    }

    consteval bool name_should_be_exported(std::meta::info info) {
        if (is_operator_function(info) || is_operator_function_template(info) || is_literal_operator(info) ||
            is_literal_operator_template(info)) {
            return true;
        }

        if (!has_identifier(info)) {
            return false;
        }

        std::string_view identifier = identifier_of(info);

        if (std::ranges::any_of(included_inline_namespaces, [&](std::string_view ns) {
            return identifier == ns;
        })) {
            return true;
        }

        if (identifier.empty()) {
            return false;
        }

        if (identifier[0] >= 'A' && identifier[0] <= 'Z') {
            return false;
        }

        if (identifier.contains("__")) {
            return false;
        }

        if (identifier.starts_with('_')) {
            if (identifier.size() == 1) {
                return false;
            }

            char second = identifier[1];

            if (second >= '0' && second <= '9') {
                return true;
            } else {
                return false;
            }
        }

        return true;
    }

    consteval void collect_use_statement(std::vector<line_record> &lines, std::meta::info info, unsigned indent);

    consteval std::string_view get_operator_string(std::string_view display_string) {
        if (display_string.contains("operator()")) {
            return "operator()";
        }

        if (display_string.contains("operator")) {
            auto end_of_operator = display_string.find("operator") + std::string_view("operator").size();
            // ends before any (
            auto rest = display_string.substr(end_of_operator);
            auto end = rest.find('(');
            return std::define_static_string(std::string("operator") + std::string(rest.substr(0, end)));
        }
    }

    consteval std::string qualified_name_of(std::meta::info entity) {
        std::vector<std::string_view> parts;
        std::meta::info cur = entity;

        while (has_identifier(cur)) {
            parts.push_back(identifier_of(cur));
            if (!has_parent(cur)) {
                break;
            }
            cur = parent_of(cur);
        }

        std::string result = "::";
        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
            result += *it;
            if (std::next(it) != parts.rend()) {
                result += "::";
            }
        }
        return result;
    }

    consteval void collect_use_single(std::vector<line_record> &lines, std::meta::info info, unsigned indent) {
        if (has_internal_linkage(info)) {
            return;
        }

        if (!name_should_be_exported(info)) {
            return;
        }

        if (is_namespace_alias(info)) {
            std::string target = qualified_name_of(dealias(info));
            lines.push_back(make_alias_line(identifier_of(info), target, indent));
            return;
        }

        if (is_namespace(info)) {
            collect_use_statement(lines, info, indent);
            return;
        }

        if (std::meta::is_operator_function(info) || std::meta::is_operator_function_template(info)
            || is_literal_operator(info) || is_literal_operator_template(info)) {
            lines.push_back(make_line(line_kind::use, get_operator_string(std::meta::display_string_of(info)), indent));
            return;
        }

        if (is_type(info) || is_type_alias(info)
            || is_function(info) || std::meta::is_function_template(info)
            || is_variable(info) || std::meta::is_variable_template(info)
            || is_template(info)) {
            lines.push_back(make_line(line_kind::use, identifier_of(info), indent));
        }
    }

    consteval void collect_use_statement(std::vector<line_record> &lines, std::meta::info info, unsigned indent) {
        if (has_internal_linkage(info)) {
            return;
        }

        if (std::meta::is_namespace_alias(info)) {
            lines.push_back(make_line(line_kind::use, identifier_of(info), indent));
            return;
        }

        if (!std::meta::is_namespace(info)) {
            throw std::meta::exception("collect_use_statement can only be called on namespace info.", info);
        }

        bool is_inline = has_identifier(info) && std::ranges::any_of(included_inline_namespaces,
                                                                     [&](std::string_view ns) {
                                                                         return identifier_of(info) == ns;
                                                                     });

        if (!is_inline)
            lines.push_back(make_line(
                line_kind::ns_open,
                has_identifier(info) ? identifier_of(info) : std::string_view{},
                indent
            ));

        std::vector<std::meta::info> members = members_of(info, std::meta::access_context::unprivileged());

        for (auto member: members) {
            collect_use_single(lines, member, indent + 4 * (is_inline ? 0 : 1));
        }

        if (!is_inline)
            lines.push_back(make_line(line_kind::ns_close, std::string_view{}, indent));
    }

    consteval auto collect_all() {
        std::vector<line_record> lines;
        lines.reserve(16384);
        collect_use_statement(lines, ^^::, 0);
        return lines;
    }

    constexpr auto lines_storage = std::define_static_array(collect_all());

    inline std::string render(std::span<const line_record> lines) {
        std::string ss;
        ss.reserve(1'000'000);

        std::vector<std::string_view> ns_path;
        std::vector<std::unordered_set<std::string_view>> seen_stack;
        seen_stack.emplace_back();

        for (const auto &line: lines) {
            switch (line.kind) {
                case line_kind::use: {
                    std::string_view name(line.name);

                    auto &seen = seen_stack.back();
                    if (!seen.insert(name).second) {
                        break;
                    }

                    ss.append(line.indent, ' ');
                    ss.append("using ::");
                    for (auto part: ns_path) {
                        if (part.empty()) continue;
                        ss.append(part);
                        ss.append("::");
                    }
                    ss.append(name);
                    ss.append(";\n");
                    break;
                }
                case line_kind::ns_open: {
                    std::string_view name(line.name);

                    ss.append(line.indent, ' ');
                    if (!name.empty()) {
                        ss.append("namespace ");
                        ss.append(name);
                        ss.append(" ");
                    } else {
                        ss.append("export ");
                    }
                    ss.append("{\n");

                    ns_path.push_back(name);
                    seen_stack.emplace_back();
                    break;
                }
                case line_kind::ns_close:
                    ss.append(line.indent, ' ');
                    ss.append("}\n");

                    ns_path.pop_back();
                    seen_stack.pop_back();
                    break;
                case line_kind::ns_alias: {
                    std::string_view name(line.name);

                    auto &seen = seen_stack.back();
                    if (!seen.insert(name).second) {
                        break;
                    }

                    ss.append(line.indent, ' ');
                    ss.append("namespace ");
                    ss.append(name);
                    ss.append(" = ");
                    ss.append(line.target);
                    ss.append(";\n");
                    break;
                }
            }
        }

        return ss;
    }


    inline std::string export_result() {
        return render(lines_storage);
    }
}

namespace test_namespace {
}

int main() {
    std::string res = __details::export_result();
    std::ofstream out("std.module.ixx");
    out << R"(module;

#include <bits/stdc++.h>

export module std.module;

    )";
    out << res;
}
