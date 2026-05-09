#pragma once

#include <stdint.h>

#include "Core/Components.h"
#include "GLFW/glfw3.h"
#include "Shader.h"
#include "Texture.h"
#include <glm/glm.hpp>

namespace Hamster {
    enum TransformType { Translate, Rotate, Scale };

    class AssetManager;
    class FramebufferResizeEvent;

    class Renderer {
    public:
        Renderer(int viewportHeight, int viewportWidth, AssetManager *assetManager);

        void SetViewport(FramebufferResizeEvent &e);

        void SetViewport(int height, int width);

        void Clear();

        void SetClearColour(float r, float g, float b, float a);

        void DrawSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                        float rotation, glm::vec3 colour);

        void DrawFlat(glm::vec2 position, glm::vec2 size, float rotation,
                      glm::vec3 colour);

        void DrawGuizmo(Transform targetTransform, TransformType type,
                        bool selectionColour);

        void AdjustZoom(float factor, float mousePosX, float mousePosY);

        void UpdateViewMatrix();

        void ChangeCameraOffset(const glm::vec2 &offset);

        glm::vec2 ScreenToWorldPos(const glm::vec2 &mousePos);

    private:
        void InitRendererData(AssetManager *assetManager);

        std::shared_ptr<Shader> m_SpriteShader;
        std::shared_ptr<Shader> m_FlatShader;
        unsigned int m_VAO = 0;

        int m_ViewportHeight = 1080;
        int m_ViewportWidth = 1920;

        float m_Zoom = 1.0f;

        glm::mat4 m_ViewMatrix;
        glm::vec2 m_CameraOffset;
    };
} // namespace Hamster
