cmake_minimum_required(VERSION 3.28)
project(Hamster-Wheel LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python COMPONENTS Interpreter Development REQUIRED)

add_compile_definitions(GLFW_EXPOSE_NATIVE_WIN32)
add_compile_definitions(GLFW_INCLUDE_NONE)

add_executable(Hamster-Wheel
    src/main.cpp
    src/EditorLayer.cpp
    src/ProjectHubLayer.cpp
    src/ProjectRegistry.cpp
    src/Theme.cpp
    src/Panel.cpp
    src/Components/Components.cpp
    src/Panels/PropertyEditor.cpp
    src/Panels/LevelEditor.cpp
    src/Panels/Hierarchy.cpp
    src/Panels/AnimationPanel.cpp
    src/Panels/BottomPanel.cpp
    src/Panels/AssetBrowser.cpp
    src/Panels/Console.cpp
    src/Panels/CreateProjectModal.cpp
    src/Panels/OpenProjectModal.cpp
    src/Panels/ColliderEditor.cpp
    src/Panels/RenameModal.cpp
    src/Panels/SpritesheetEditor.cpp
)

target_include_directories(Hamster-Wheel PRIVATE
    src
    src/Theme
    ${CMAKE_SOURCE_DIR}/Hamster-Core/src
    ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/imgui
    ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/imgui/backends
    ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/imgui/misc/cpp
    ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/glad/include
    ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/glfw/include
    ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/glm
    ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/stb
    ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/box2d/include
    Vendor
    Vendor/tinyfiledialogs
    ${CMAKE_SOURCE_DIR}/Hamster-Py/Vendor/pybind11/include
    ${Python_INCLUDE_DIRS}
)

target_link_libraries(Hamster-Wheel PRIVATE
    Hamster-Core
    glfw
    imgui
    box2d
    tinyfiledialogs
    pybind11::embed
)

# Hamster-Wheel needs the pybind11 module built first
add_dependencies(Hamster-Wheel Hamster)

# Pass the .pyd/.so filename to C++ for ProjectCreator
target_compile_definitions(Hamster-Wheel PRIVATE
    HAMSTER_PY_MODULE_FILENAME="$<TARGET_FILE_NAME:Hamster>"
)

# Copy Resources to the build directory during development
add_custom_command(
    TARGET Hamster-Wheel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources
    $<TARGET_FILE_DIR:Hamster-Wheel>/../share/Resources/Hamster-Wheel/Resources
    COMMENT "Copying Resources directory to build output directory"
)

# Copy the pybind11 module (.pyd/.so) into Resources/Packages/
add_custom_command(
    TARGET Hamster-Wheel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:Hamster-Wheel>/../share/Resources/Hamster-Wheel/Resources/Packages
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Hamster>
    $<TARGET_FILE_DIR:Hamster-Wheel>/../share/Resources/Hamster-Wheel/Resources/Packages/
    COMMENT "Copying Hamster Python module to build output"
)
