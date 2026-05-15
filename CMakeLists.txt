cmake_minimum_required(VERSION 3.28)

project(Hamster VERSION 1.0
        DESCRIPTION "An intuitive game engine made for python"
        LANGUAGES CXX
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
enable_testing()

add_subdirectory(Hamster-Core)
add_subdirectory(Hamster-Py)
add_subdirectory(Hamster-Wheel)
add_subdirectory(test)

# Hamster-Wheel-old preserved as historical reference; not built.
# add_subdirectory(Hamster-Wheel-old)

add_subdirectory(Hamster-Core/Vendor/imgui)
add_subdirectory(Hamster-Core/Vendor/glfw)
add_subdirectory(Hamster-Core/Vendor/entt)
add_subdirectory(Hamster-Core/Vendor/box2d)
add_subdirectory(Hamster-Wheel/Vendor/tinyfiledialogs)
