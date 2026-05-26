#pragma once

#include <stdint.h>
#include <memory>

#include "Core/Components.h"
#include "FontAtlas.h"
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

        // uvRect = (x, y, w, h) in normalised [0,1] over the texture.
        // Default (0,0,1,1) = whole texture (backward-compat with the
        // pre-spritesheet API). Sub-sprites pass a sub-region.
        void DrawSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                        float rotation, glm::vec3 colour,
                        glm::vec4 uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

        // Sprite batching API (v1 — same-texture batching). Accumulates sprite
        // quads in a pre-allocated VBO, flushes one draw call per texture or
        // z-boundary change. Pick path (Scene::OnRender(true)) intentionally
        // keeps using DrawSprite — see sprite-batching feature spec.
        // Sub-sprite UV is baked into the per-vertex UVs at submission time —
        // no shader change needed for the batch path.
        void BeginSpriteBatch();
        void SubmitSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                          float rotation, glm::vec3 colour, float z,
                          glm::vec4 uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        void EndSpriteBatch();
        uint32_t GetLastFrameDrawCallCount() const { return m_DrawCallsLastFrame; }

        // Popout play-window support. VAOs (and GL render state) are NOT shared
        // across GL contexts, even shared ones — only buffers/textures/shaders
        // are. The popout has its own context, so it needs its own VAOs (wiring
        // the shared VBOs) and its own blend/depth state. Call
        // CreatePopoutVertexArrays() once with the popout context current, then
        // wrap the popout's scene render in SetPopoutMode(true)/(false) so the
        // draw paths bind the popout VAOs (bug 0017).
        void CreatePopoutVertexArrays();
        void SetPopoutMode(bool on) { m_PopoutMode = on; }

        void DrawFlat(glm::vec2 position, glm::vec2 size, float rotation,
                      glm::vec3 colour);

        // UI pass — screen-space ortho, depth off. Begin sets up state and
        // overrides the projection to (0, panelW, panelH, 0) so coordinates
        // are pixels with origin at the panel's top-left (matches mouse
        // input). End restores world-space state and flushes the batched
        // rects + text. SubmitUIRect / SubmitUIText append one rect or one
        // text run; each batches into a single draw call per pass.
        // worldProjection=true renders UI through the world view matrix instead
        // of a screen ortho — the editor uses it so UI anchored within
        // (0,0)->(target) lands on the play-area box and pans/zooms with it.
        // Play (popout) uses the screen ortho (the window IS the play area).
        void BeginUIPass(float panelWidth, float panelHeight,
                         bool worldProjection = false);
        void SubmitUIRect(const UIRect &rect, const glm::vec4 &colour);
        // text is rendered with top-left origin at `topLeft`. fontSize scales
        // the baked atlas; wrapWidth == 0 means single-line.
        void SubmitUIText(const std::string &text, glm::vec2 topLeft,
                          float fontSize, const glm::vec4 &colour,
                          float wrapWidth, bool bold = false);
        // Draws one textured quad filling `rect`, in the UI pass's projection
        // (so it works under both the editor's world projection and the
        // popout's screen ortho). Image buttons are few, so this is an
        // immediate single-draw (no batching). Reuses the sprite-batch shader
        // + VAO (popout-aware) and restores the world projection afterward.
        // Call after FlushUIRect and before any text so layering is
        // bg-rect → image → label.
        void SubmitUIImage(Texture &texture, const UIRect &rect,
                           glm::vec4 uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
                           glm::vec3 tint = glm::vec3(1.0f));
        void EndUIPass();
        // Draws the accumulated UI rects immediately. Call after submitting all
        // backgrounds and before any text, so text always lands on top (bold
        // text triggers a mid-pass text flush, so we can't rely on EndUIPass's
        // ordering alone).
        void FlushUIRect();

        // Font atlases — owned by the renderer. Nullable when the TTF failed
        // to load (text becomes a no-op). Exposed read-only so Scene's auto-
        // size path can call MeasureWidth without going through the renderer.
        const FontAtlas *GetFontAtlas() const { return m_FontAtlas.get(); }
        const FontAtlas *GetFontAtlasBold() const { return m_FontAtlasBold.get(); }

        // Resolve a UIButton's screen-space rect. When autoSize is true and
        // the atlas is loaded, size is replaced with label-width + 2·padding
        // and font-height + 2·padding. Same call from EditorLayer hit-test
        // and Scene render so geometry stays consistent.
        UIRect ResolveUIButton(const UIButton &b, float vw, float vh) const;

        void DrawGuizmo(Transform targetTransform, TransformType type,
                        bool selectionColour);

        void DrawHoverOutline(Transform targetTransform);

        // Axis-aligned rectangle outline in world space, no fill. Used by
        // EditorLayer to draw the project's play-area bounds. Reuses the
        // existing FlatShader border-mode path (same as DrawHoverOutline).
        void DrawWorldRectOutline(glm::vec2 origin, glm::vec2 size,
                                  glm::vec3 colour, float widthPx,
                                  float alpha);

        void AdjustZoom(float factor, float mousePosX, float mousePosY);

        void UpdateViewMatrix();

        void ChangeCameraOffset(const glm::vec2 &offset);
        // Absolute setter — bypasses the delta semantics of ChangeCameraOffset.
        // Used by the popout-window render path to swap camera state in and
        // out around the second render pass.
        void SetCameraOffset(const glm::vec2 &absolute);

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
        std::shared_ptr<Shader> m_UIRectShader;
        std::shared_ptr<Shader> m_UITextShader;
        unsigned int m_VAO = 0;

        // UI rect batch — vertex layout: vec2 pos, vec4 colour (6 floats).
        // Pre-allocated VBO sized for the v1 element-count budget (< 100 UI
        // rects per scene at 6 verts/rect × 6 floats × 4 bytes ≈ 14 KB).
        unsigned int m_UIRectVAO = 0;
        unsigned int m_UIRectVBO = 0;
        std::vector<float> m_UIRectVerts;

        // UI text batch — vertex layout: vec2 pos, vec2 uv, vec4 colour
        // (8 floats). One quad per glyph; flushed once per UI pass on the
        // atlas texture.
        unsigned int m_UITextVAO = 0;
        unsigned int m_UITextVBO = 0;
        std::vector<float> m_UITextVerts;

        std::unique_ptr<FontAtlas> m_FontAtlas;
        std::unique_ptr<FontAtlas> m_FontAtlasBold;
        // Atlas the pending m_UITextVerts belong to; the text batch flushes
        // when it changes so regular/bold runs use the right atlas texture.
        const FontAtlas *m_CurTextAtlas = nullptr;

        // Batching state. m_BatchVBO is pre-allocated once at construction
        // (sized for kMaxBatchSprites) so SubmitSprite never reallocates GPU
        // memory at frame time. m_BatchVerts is the CPU staging buffer, sized
        // to exactly the same maximum to avoid runtime growth.
        unsigned int m_BatchVAO = 0;
        unsigned int m_BatchVBO = 0;

        // Popout-context duplicates of the VAOs the scene + UI render use. The
        // shared VBOs are reused; only the VAO objects differ per context.
        // m_PopoutMode selects them at the draw-bind sites (bug 0017).
        unsigned int m_BatchVAO_Popout = 0;
        unsigned int m_UIRectVAO_Popout = 0;
        unsigned int m_UITextVAO_Popout = 0;
        bool m_PopoutMode = false;
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

        // Projection set by the last BeginUIPass — world view matrix (editor)
        // or screen ortho (popout). SubmitUIImage uses it so image quads land
        // in the same space as the UI rects/text of the current pass.
        glm::mat4 m_UIProjection{1.0f};

        void FlushSpriteBatch();
        void FlushUIText(); // draws m_UITextVerts with m_CurTextAtlas bound
    };
} // namespace Hamster
