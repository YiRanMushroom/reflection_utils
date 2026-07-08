cmake_minimum_required(VERSION 4.3)
project(reflection_utils)

set(CMAKE_CXX_STANDARD 26)

add_executable(reflection_utils main.cpp
        yaml_serialization.hpp)

target_compile_options(
        reflection_utils
        PUBLIC
        -freflection
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