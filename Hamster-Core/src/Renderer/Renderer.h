#pragma once

#include <stdint.h>

#include "Core/Components.h"
#include "GLFW/glfw3.h"
#include "Shader.h"
#include "Texture.h"
#include "Utils/SpatialIndex.h"
#include <glm/glm.hpp>

namespace Hamster {
    enum TransformType { Translate, Rotate, Scale };

    class AssetManager;
    class FramebufferResizeEvent;

    class Renderer {
    public:
        Renderer(int viewportHeight, int viewportWidth, AssetManager *assetManager);
        ~Renderer();

        void SetViewport(FramebufferResizeEvent &e);

        void SetViewport(int height, int width);

        void Clear();

        void SetClearColour(float r, float g, float b, float a);

        void DrawSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                        float rotation, glm::vec3 colour);

        // Sprite batching API (v1 — same-texture batching). Accumulates sprite
        // quads in a pre-allocated VBO, flushes one draw call per texture or
        // z-boundary change. Pick path (Scene::OnRender(true)) intentionally
        // keeps using DrawSprite — see sprite-batching feature spec.
        void BeginSpriteBatch();
        void SubmitSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                          float rotation, glm::vec3 colour, float z);
        void EndSpriteBatch();
        uint32_t GetLastFrameDrawCallCount() const { return m_DrawCallsLastFrame; }

        void DrawFlat(glm::vec2 position, glm::vec2 size, float rotation,
                      glm::vec3 colour);

        void DrawGuizmo(Transform targetTransform, TransformType type,
                        bool selectionColour);

        void DrawHoverOutline(Transform targetTransform);

        void AdjustZoom(float factor, float mousePosX, float mousePosY);

        void UpdateViewMatrix();

        void ChangeCameraOffset(const glm::vec2 &offset);

        glm::vec2 ScreenToWorldPos(const glm::vec2 &mousePos);

        // World-space AABB the current camera covers. Used by Scene to cull
        // sprites outside the view before submission.
        AABB GetViewportWorldAABB() const;

        float GetZoom() const { return m_Zoom; }
        void SetZoom(float zoom);
        glm::vec2 GetCameraOffset() const { return m_CameraOffset; }
        int GetViewportWidth() const { return m_ViewportWidth; }
        int GetViewportHeight() const { return m_ViewportHeight; }

    private:
        void InitRendererData(AssetManager *assetManager);

        std::shared_ptr<Shader> m_SpriteShader;
        std::shared_ptr<Shader> m_FlatShader;
        std::shared_ptr<Shader> m_SpriteBatchShader;
        unsigned int m_VAO = 0;

        // Batching state. m_BatchVBO is pre-allocated once at construction
        // (sized for kMaxBatchSprites) so SubmitSprite never reallocates GPU
        // memory at frame time. m_BatchVerts is the CPU staging buffer, sized
        // to exactly the same maximum to avoid runtime growth.
        unsigned int m_BatchVAO = 0;
        unsigned int m_BatchVBO = 0;
        std::vector<float> m_BatchVerts;
        Texture *m_BatchTexture = nullptr;
        float m_BatchZ = 0.0f;
        uint32_t m_DrawCallsThisFrame = 0;
        uint32_t m_DrawCallsLastFrame = 0;

        int m_ViewportHeight = 1080;
        int m_ViewportWidth = 1920;

        float m_Zoom = 1.0f;

        glm::mat4 m_ViewMatrix{1.0f};
        glm::vec2 m_CameraOffset{0.0f, 0.0f};

        void FlushSpriteBatch();
    };
} // namespace Hamster
