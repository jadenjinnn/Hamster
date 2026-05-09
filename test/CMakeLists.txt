cmake_minimum_required(VERSION 3.28)
project(HamsterTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Python COMPONENTS Interpreter Development REQUIRED)

add_executable(SmokeTest smoke_test.cpp)

target_include_directories(SmokeTest PRIVATE
        ${CMAKE_SOURCE_DIR}/Hamster-Core/src
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/GLFW/include
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/glm
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/imgui
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/box2d/include
        ${CMAKE_SOURCE_DIR}/Hamster-Py/Vendor/pybind11/include
        ${Python_INCLUDE_DIRS}
)

# Bake the fixture path so the test can find its Python scripts
target_compile_definitions(SmokeTest PRIVATE
        SMOKE_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(SmokeTest PRIVATE Hamster-Core glfw pybind11::embed)

# Copy the pybind11 module (.pyd) into the fixtures directory
add_dependencies(SmokeTest Hamster)
add_custom_command(
        TARGET SmokeTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Hamster>
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/
        COMMENT "Copying Hamster Python module to test fixtures"
)

enable_testing()
add_test(NAME SmokeTest COMMAND SmokeTest)
