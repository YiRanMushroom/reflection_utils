cmake_minimum_required(VERSION 4.3)
project(reflection_utils)

set(CMAKE_CXX_STANDARD 26)

add_executable(reflection_utils main.cpp
        yaml_serialization.hpp
)

target_compile_options(
        reflection_utils
        PUBLIC
        -freflection
        "-fconstexpr-ops-limit=1000000000"
        "-fconstexpr-loop-limit=1000000000"
)

target_link_libraries(
        reflection_utils
        PUBLIC
        "-lstdc++exp"
)

# statically link
target_link_options(
        reflection_utils
        PUBLIC
        "-static"
        "-static-libgcc"
        "-static-libstdc++"
)

add_subdirectory(yaml-cpp)
target_link_libraries(
        reflection_utils
        PUBLIC
        yaml-cpp
)

target_sources(
        reflection_utils
        PUBLIC
        FILE_SET
        cxx_modules
        TYPE
        CXX_MODULES
        FILES
        std.cc
        std.compat.cc
)