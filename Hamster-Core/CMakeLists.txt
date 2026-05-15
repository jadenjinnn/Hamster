cmake_minimum_required(VERSION 3.28)
project(Hamster-Core)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


find_package(Python COMPONENTS Interpreter Development REQUIRED)

add_compile_definitions(GLFW_EXPOSE_NATIVE_WIN32)
add_compile_definitions(GLFW_INCLUDE_NONE)

set(Core
        src/Core/Application.cpp src/Core/Application.h

        src/Core/Base.h

        src/Core/Layer.h

        src/Core/LayerStack.cpp src/Core/LayerStack.h

        src/Core/stb_image.cpp

        src/Core/Window.cpp src/Core/Window.h

        src/Core/Scene.cpp src/Core/Scene.h

        src/Core/UUID.cpp src/Core/UUID.h

        src/Core/SceneSerialiser.cpp src/Core/SceneSerialiser.h
        src/Core/ProjectSerialiser.cpp
        src/Core/ProjectSerialiser.h

        src/Core/Project.cpp
        src/Core/Project.h

        src/Core/Log.h src/Core/Log.cpp
)

set(Events
        src/Events/ApplicationEvents.h

        src/Events/Event.cpp src/Events/Event.h

        src/Events/GuiEvents.h

        src/Events/InputEvents.h

        src/Events/WindowEvents.h
        src/Events/SceneEvents.h
)


set(Renderer
        src/Renderer/FramebufferTexture.cpp src/Renderer/FramebufferTexture.h

        src/Renderer/Renderer.cpp src/Renderer/Renderer.h

        src/Renderer/Shader.cpp src/Renderer/Shader.h

        src/Renderer/Texture.cpp src/Renderer/Texture.h
)


set(Utils
        src/Utils/AssetManager.cpp src/Utils/AssetManager.h

        src/Utils/InputManager.cpp src/Utils/InputManager.h
)


set(Stb_Image
        src/Core/stb_image.cpp Vendor/stb/stb_image.h
)

set(Gui
        src/Gui/ImGuiLayer.cpp src/Gui/ImGuiLayer.h

        src/Gui/Panel.h

        src/Gui/Modal.h
)

set(Physics
        src/Physics/Physics.cpp src/Physics/Physics.h
)

set(Scripting
        src/Scripting/Scripting.cpp src/Scripting/Scripting.h
        src/Scripting/HamsterBehaviour.cpp src/Scripting/HamsterBehaviour.h
        src/Scripting/HamsterScript.cpp src/Scripting/HamsterScript.h
)

set(sources
        ${Core}
        ${Events}
        ${Renderer}
        ${Utils}
        ${Stb_Image}
        ${Gui}
        ${Physics}
        ${Scripting}

)

set(c_sources
        Vendor/glad/src/glad.c


)

add_library(Hamster-C STATIC ${c_sources})

target_include_directories(Hamster-C PUBLIC
        Vendor/glad/include
)


set(HAMSTER_CORE_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

add_library(Hamster-Core STATIC ${sources} src/Hamster.h)

# find_package(Python COMPONENTS Interpreter Development REQUIRED)

target_precompile_headers(Hamster-Core PRIVATE src/HamsterPCH.h)

target_include_directories(Hamster-Core PUBLIC
        src
        Vendor/glad/include
        Vendor/glfw/include
        Vendor/glm
        Vendor/stb
        #        Vendor/pybind11/include
        ${Python_INCLUDE_DIRS}
        Vendor/imgui
        Vendor/imgui/backends
        Vendor/entt/src
        Vendor/box2d/include
        Vendor/boost_1_86_0
        ${CMAKE_SOURCE_DIR}/Hamster-Py/Vendor/pybind11/include
)

target_compile_definitions(Hamster-Core PRIVATE HAMSTER_CORE_SRC_DIR="${CMAKE_SOURCE_DIR}/Hamster-Core/src")

target_link_libraries(Hamster-Core PUBLIC glfw imgui box2d Hamster-C)

# Comctl32 provides SetWindowSubclass (used for borderless-with-snap on Win32).
if(WIN32)
    target_link_libraries(Hamster-Core PUBLIC Comctl32)
endif()

install(TARGETS Hamster-Core
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
