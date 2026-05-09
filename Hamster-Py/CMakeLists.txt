cmake_minimum_required(VERSION 3.28)
project(Hamster-Py)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Python COMPONENTS Interpreter Development REQUIRED)

add_subdirectory(Vendor/pybind11)

pybind11_add_module(Hamster src/main.cpp)

target_include_directories(Hamster PUBLIC
        src
        ${CMAKE_SOURCE_DIR}/Hamster-Core/src
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/GLFW/include
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/glm
        Vendor/pybind11/include
        ${Python_INCLUDE_DIRS}
)

target_link_libraries(Hamster PUBLIC Hamster-Core)
target_link_libraries(Hamster PUBLIC glfw)

add_custom_command(
        TARGET Hamster
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Hamster>
        ${CMAKE_SOURCE_DIR}/Hamster-Wheel/Resources/Packages/
        COMMENT "Copying Hamster Python module to Hamster-Wheel"
)
