#pragma once

#include <stdint.h>

#include "Core/Application.h"
#include "Core/Components.h"
#include "GLFW/glfw3.h"
#include "Shader.h"
#include "Texture.h"
#include <glm/glm.hpp>

namespace Hamster {
    enum TransformType { Translate, Rotate, Scale };

    class AssetManager;

    class Renderer {
    public:
        static void Init(int viewportHeight, int viewportWidt, AssetManager *assetManager);

        static void Terminate();

        static void SetViewport(FramebufferResizeEvent &e);

        static void SetViewport(int height, int width);

        static void Clear();

        static void SetClearColour(float r, float g, float b, float a);

        static void DrawSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                               float rotation, glm::vec3 colour);

        static void DrawFlat(glm::vec2 position, glm::vec2 size, float rotation,
                             glm::vec3 colour);

        static void DrawGuizmo(Transform targetTransform, TransformType type,
                               bool selectionColour);

        static void AdjustZoom(float factor, float mousePosX, float mousePosY);

        static void UpdateViewMatrix();

        static void ChangeCameraOffset(const glm::vec2 &offset);

        static glm::vec2 ScreenToWorldPos(const glm::vec2 &mousePos);

    private:
        static void InitRendererData(AssetManager *assetManager);

        inline static std::shared_ptr<Shader> m_SpriteShader;
        inline static std::shared_ptr<Shader> m_FlatShader;
        inline static unsigned int m_VAO;

        inline static int m_ViewportHeight = 1080;
        inline static int m_ViewportWidth = 1920;

        inline static float m_Zoom = 1.0f;

        inline static glm::mat4 m_ViewMatrix;
        inline static glm::vec2 m_CameraOffset;
    };
} // namespace Hamster
