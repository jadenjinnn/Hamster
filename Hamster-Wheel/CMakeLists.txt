cmake_minimum_required(VERSION 3.28)
project(Hamster-Wheel)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(Python COMPONENTS Interpreter Development REQUIRED)

add_compile_definitions(GLFW_EXPOSE_NATIVE_WIN32)
add_compile_definitions(GLFW_INCLUDE_NONE)


set(PANELS
        src/Panels/PropertyEditor.cpp src/Panels/PropertyEditor.h


        src/Panels/Hierarchy.cpp src/Panels/Hierarchy.h


        src/Panels/MenuBar.cpp src/Panels/MenuBar.h
        src/Panels/ProjectCreator.cpp
        src/Panels/ProjectCreator.h
        src/Panels/ProjectSelector.cpp
        src/Panels/ProjectSelector.h
        src/ProjectHubLayer.cpp src/ProjectHubLayer.h
        src/ProjectRegistry.cpp src/ProjectRegistry.h
        src/Panels/AssetBrowser.cpp
        src/Panels/AssetBrowser.h
        src/Panels/Console.h src/Panels/Console.cpp
        src/Panels/RenameModal.cpp src/Panels/RenameModal.h
        src/Panels/ColliderEditor.cpp src/Panels/ColliderEditor.h
)

set(THEME
        src/Theme/HamsterTheme.cpp src/Theme/HamsterTheme.h
)

add_executable(Hamster-Wheel ${PANELS} ${THEME} src/EditorLayer.cpp src/EditorLayer.h src/HamsterWheelApp.cpp)

target_include_directories(Hamster-Wheel PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/Hamster-Core/src
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/GLFW/include
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/glm
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/imgui
        ${CMAKE_SOURCE_DIR}/Hamster-Core/Vendor/box2d/include
        Vendor/tinyfiledialogs
        Vendor
        ${CMAKE_SOURCE_DIR}/Hamster-Py/Vendor/pybind11/include
        ${Python_INCLUDE_DIRS}
)

target_link_libraries(Hamster-Wheel PRIVATE Hamster-Core glfw imgui box2d tinyfiledialogs pybind11::embed)

# Hamster-Wheel needs the pybind11 module built first
add_dependencies(Hamster-Wheel Hamster)

# Pass the .pyd/.so filename to C++ so ProjectCreator doesn't hardcode a Python version
target_compile_definitions(Hamster-Wheel PRIVATE HAMSTER_PY_MODULE_FILENAME="$<TARGET_FILE_NAME:Hamster>")

# Copy Resources to the build directory during development
add_custom_command(
        TARGET Hamster-Wheel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Hamster-Wheel/Resources
        $<TARGET_FILE_DIR:Hamster-Wheel>/../share/Resources/Hamster-Wheel/Resources
        COMMENT "Copying Resources directory to build output directory"
)

# Copy the pybind11 module (.pyd/.so) into the build Resources/Packages/
add_custom_command(
        TARGET Hamster-Wheel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:Hamster-Wheel>/../share/Resources/Hamster-Wheel/Resources/Packages
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Hamster>
        $<TARGET_FILE_DIR:Hamster-Wheel>/../share/Resources/Hamster-Wheel/Resources/Packages/
        COMMENT "Copying Hamster Python module to build output"
)


# Install the executable
install(TARGETS Hamster-Wheel DESTINATION usr/bin)

# Install resources
install(DIRECTORY Resources/ DESTINATION usr/share/Hamster-Wheel/Resources)


