#include "HamsterPCH.h"

#include "Renderer.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/Application.h"
#include "Events/WindowEvents.h"
#include "Utils/AssetManager.h"

namespace Hamster {
    namespace {
        // Per-sprite cap on a single batch. 10k sprites × 6 verts × 7 floats
        // × 4 bytes ≈ 1.68 MB pre-allocated VBO. SubmitSprite flushes early
        // if appending would overflow, so this is a hard cap on per-flush
        // sprites, not on per-frame sprites.
        constexpr int kMaxBatchSprites = 10000;
        constexpr int kFloatsPerVertex = 7;
        constexpr int kVertsPerSprite  = 6;
        constexpr int kFloatsPerSprite = kVertsPerSprite * kFloatsPerVertex;
    }

    Renderer::Renderer(int viewportHeight, int viewportWidth, AssetManager *assetManager) {
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
            std::cout << "Failed to initialise glad" << std::endl;
        }

        m_ViewportHeight = viewportHeight;
        m_ViewportWidth = viewportWidth;

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        InitRendererData(assetManager);
    }

    Renderer::~Renderer() {
        if (m_BatchVBO) glDeleteBuffers(1, &m_BatchVBO);
        if (m_BatchVAO) glDeleteVertexArrays(1, &m_BatchVAO);
    }

    void Renderer::SetViewport(FramebufferResizeEvent &e) {
        m_ViewportWidth = e.GetWidth();
        m_ViewportHeight = e.GetHeight();

        glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);

        UpdateViewMatrix();
    }

    void Renderer::SetViewport(int height, int width) {
        m_ViewportHeight = height;
        m_ViewportWidth = width;

        glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);

        UpdateViewMatrix();
    }

    void Renderer::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void Renderer::SetClearColour(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
    }

    void Renderer::InitRendererData(AssetManager *assetManager) {
        std::string hamsterCorePath = HAMSTER_CORE_SRC_DIR;

        m_SpriteShader = assetManager->AddShader(
            "sprite", hamsterCorePath + "/Renderer/DefaultShaders/SpriteShader.vs",
            hamsterCorePath + "/Renderer/DefaultShaders/SpriteShader.fs");

        m_FlatShader = assetManager->AddShader(
            "flat", hamsterCorePath + "/Renderer/DefaultShaders/FlatShader.vs",
            hamsterCorePath + "/Renderer/DefaultShaders/FlatShader.fs");

        m_SpriteBatchShader = assetManager->AddShader(
            "sprite_batch",
            hamsterCorePath + "/Renderer/DefaultShaders/SpriteBatchShader.vs",
            hamsterCorePath + "/Renderer/DefaultShaders/SpriteBatchShader.fs");

        SetViewport(m_ViewportHeight, m_ViewportWidth);
        SetClearColour(0.0f, 0.0f, 0.0f, 1.0f);

        m_SpriteShader->use();
        m_SpriteShader->setUniformi("image", 0);
        m_SpriteShader->setUniformMat4("projection", m_ViewMatrix);

        m_FlatShader->use();
        m_FlatShader->setUniformi("image", 0);
        m_FlatShader->setUniformMat4("projection", m_ViewMatrix);
        m_FlatShader->setUniformf("alpha", 1.0f);
        m_FlatShader->setUniformi("borderMode", 0);
        m_FlatShader->setUniformf("borderWidthX", 0.0f);
        m_FlatShader->setUniformf("borderWidthY", 0.0f);

        m_SpriteBatchShader->use();
        m_SpriteBatchShader->setUniformi("image", 0);
        m_SpriteBatchShader->setUniformMat4("projection", m_ViewMatrix);

        // Pre-allocated batch VBO. Vertex layout: vec2 pos, vec2 uv, vec3
        // colour (7 floats, 28 bytes). Glued together for cache locality;
        // any v2 sampler-array additions append a new attribute at location 3.
        m_BatchVerts.reserve(kMaxBatchSprites * kFloatsPerSprite);
        glGenVertexArrays(1, &m_BatchVAO);
        glGenBuffers(1, &m_BatchVBO);
        glBindVertexArray(m_BatchVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     kMaxBatchSprites * kFloatsPerSprite * sizeof(float),
                     nullptr, GL_DYNAMIC_DRAW);
        constexpr GLsizei stride = kFloatsPerVertex * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                              (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                              (void*)(4 * sizeof(float)));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        float vertices[] = {
            0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 0.0f, 1.0f, 0.0f,

            0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f

        };

        unsigned int VBO;

        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &VBO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindVertexArray(m_VAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void Renderer::DrawSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                              float rotation, glm::vec3 colour) {
        m_SpriteShader->use();

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(position, 0.0f));
        model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.0f));
        model =
                glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        model =
                glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));

        model = glm::scale(model, glm::vec3(size, 1.0f));

        m_SpriteShader->setUniformMat4("model", model);
        m_SpriteShader->setUniformVec3("spriteColour", colour);

        glActiveTexture(GL_TEXTURE0);
        texture.BindTexture();

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);
    }

    void Renderer::DrawFlat(glm::vec2 position, glm::vec2 size, float rotation,
                            glm::vec3 colour) {
        m_FlatShader->use();
        m_FlatShader->setUniformf("alpha", 1.0f);
        m_FlatShader->setUniformi("borderMode", 0);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(position, 0.0f));
        model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.0f));
        model =
                glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        model =
                glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));

        model = glm::scale(model, glm::vec3(size, 1.0f));

        m_FlatShader->setUniformMat4("model", model);
        m_FlatShader->setUniformVec3("colour", colour);

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);
    }

    void Renderer::DrawGuizmo(Transform targetTransform, TransformType type,
                              bool selectionColour) {
        m_FlatShader->use();

        float cx = targetTransform.position.x + targetTransform.size.x * 0.5f;
        float cy = targetTransform.position.y + targetTransform.size.y * 0.5f;
        float hw = targetTransform.size.x * 0.5f;
        float hh = targetTransform.size.y * 0.5f;

        float rad = glm::radians(targetTransform.rotation);
        float cosR = std::cos(rad);
        float sinR = std::sin(rad);

        glm::vec2 offsets[8] = {
            {-hw, -hh}, {hw, -hh}, {-hw, hh}, {hw, hh},
            {0, -hh}, {hw, 0}, {0, hh}, {-hw, 0}
        };
        uint32_t ids[8] = {
            TopLeftGrabberID, TopRightGrabberID,
            BottomLeftGrabberID, BottomRightGrabberID,
            TopGrabberID, RightGrabberID,
            BottomGrabberID, LeftGrabberID
        };

        if (!selectionColour) {
            glm::vec3 blue(0.45f, 0.72f, 1.0f);
            glm::vec3 white(1.0f, 1.0f, 1.0f);

            // Entity outline
            m_FlatShader->setUniformi("borderMode", 1);
            m_FlatShader->setUniformf("alpha", 0.8f);
            float outlinePx = 1.5f;
            m_FlatShader->setUniformf("borderWidthX", outlinePx / targetTransform.size.x);
            m_FlatShader->setUniformf("borderWidthY", outlinePx / targetTransform.size.y);
            m_FlatShader->setUniformVec3("colour", blue);

            glm::mat4 outlineModel = glm::mat4(1.0f);
            outlineModel = glm::translate(outlineModel, targetTransform.position);
            outlineModel = glm::translate(outlineModel, glm::vec3(hw, hh, 0.0f));
            outlineModel = glm::rotate(outlineModel, rad, glm::vec3(0.0f, 0.0f, 1.0f));
            outlineModel = glm::translate(outlineModel, glm::vec3(-hw, -hh, 0.0f));
            outlineModel = glm::scale(outlineModel, glm::vec3(targetTransform.size, 1.0f));

            m_FlatShader->setUniformMat4("model", outlineModel);
            glBindVertexArray(m_VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            // 8 grabber squares: white fill + blue border
            const float grabSize = 8.0f;
            const float grabHalf = grabSize * 0.5f;
            const float borderPx = 1.0f;
            float bwUV = borderPx / grabSize;

            for (int i = 0; i < 8; i++) {
                float rx = offsets[i].x * cosR - offsets[i].y * sinR;
                float ry = offsets[i].x * sinR + offsets[i].y * cosR;

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(cx + rx - grabHalf, cy + ry - grabHalf, 0.0f));
                model = glm::scale(model, glm::vec3(grabSize, grabSize, 1.0f));
                m_FlatShader->setUniformMat4("model", model);

                // White fill
                m_FlatShader->setUniformi("borderMode", 0);
                m_FlatShader->setUniformf("alpha", 1.0f);
                m_FlatShader->setUniformVec3("colour", white);
                glBindVertexArray(m_VAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                // Blue border
                m_FlatShader->setUniformi("borderMode", 1);
                m_FlatShader->setUniformf("borderWidthX", bwUV);
                m_FlatShader->setUniformf("borderWidthY", bwUV);
                m_FlatShader->setUniformVec3("colour", blue);
                glBindVertexArray(m_VAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }
        } else {
            // Pick pass: filled squares with selection colors
            m_FlatShader->setUniformi("borderMode", 0);
            m_FlatShader->setUniformf("alpha", 1.0f);

            const float pickSize = 24.0f;
            const float pickHalf = pickSize * 0.5f;

            for (int i = 0; i < 8; i++) {
                float rx = offsets[i].x * cosR - offsets[i].y * sinR;
                float ry = offsets[i].x * sinR + offsets[i].y * cosR;

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(cx + rx - pickHalf, cy + ry - pickHalf, 0.0f));
                model = glm::scale(model, glm::vec3(pickSize, pickSize, 1.0f));

                m_FlatShader->setUniformMat4("model", model);
                m_FlatShader->setUniformVec3("colour", Application::IdToColour(ids[i]));

                glBindVertexArray(m_VAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }
        }
    }

    void Renderer::DrawHoverOutline(Transform t) {
        m_FlatShader->use();

        float hw = t.size.x * 0.5f;
        float hh = t.size.y * 0.5f;
        float rad = glm::radians(t.rotation);

        glm::vec3 blue(0.45f, 0.72f, 1.0f);

        m_FlatShader->setUniformi("borderMode", 1);
        m_FlatShader->setUniformf("alpha", 0.45f);
        float outlinePx = 1.5f;
        m_FlatShader->setUniformf("borderWidthX", outlinePx / t.size.x);
        m_FlatShader->setUniformf("borderWidthY", outlinePx / t.size.y);
        m_FlatShader->setUniformVec3("colour", blue);

        glm::mat4 outlineModel = glm::mat4(1.0f);
        outlineModel = glm::translate(outlineModel, t.position);
        outlineModel = glm::translate(outlineModel, glm::vec3(hw, hh, 0.0f));
        outlineModel = glm::rotate(outlineModel, rad, glm::vec3(0.0f, 0.0f, 1.0f));
        outlineModel = glm::translate(outlineModel, glm::vec3(-hw, -hh, 0.0f));
        outlineModel = glm::scale(outlineModel, glm::vec3(t.size, 1.0f));

        m_FlatShader->setUniformMat4("model", outlineModel);
        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void Renderer::UpdateViewMatrix() {
        m_ViewMatrix = glm::ortho(0.0f + m_CameraOffset.x,
                                  static_cast<float>(m_ViewportWidth) / m_Zoom + m_CameraOffset.x,
                                  static_cast<float>(m_ViewportHeight) / m_Zoom + m_CameraOffset.y,
                                  0.0f + m_CameraOffset.y,
                                  -1.0f, 1.0f);

        m_SpriteShader->use();
        m_SpriteShader->setUniformi("image", 0);
        m_SpriteShader->setUniformMat4("projection", m_ViewMatrix);

        m_FlatShader->use();
        m_FlatShader->setUniformi("image", 0);
        m_FlatShader->setUniformMat4("projection", m_ViewMatrix);

        if (m_SpriteBatchShader) {
            m_SpriteBatchShader->use();
            m_SpriteBatchShader->setUniformi("image", 0);
            m_SpriteBatchShader->setUniformMat4("projection", m_ViewMatrix);
        }
    }

    void Renderer::AdjustZoom(float factor, float mousePosX, float mousePosY) {
        glm::vec2 worldMousePos = ScreenToWorldPos({mousePosX, mousePosY});

        m_Zoom += factor;

        glm::vec2 newWorldMousePos = ScreenToWorldPos({mousePosX, mousePosY});

        UpdateViewMatrix();

        ChangeCameraOffset(newWorldMousePos - worldMousePos);
    }

    AABB Renderer::GetViewportWorldAABB() const {
        // Mirror of the ortho projection in UpdateViewMatrix — the world rect
        // mapped onto the framebuffer.
        AABB rect;
        rect.min = m_CameraOffset;
        rect.max = m_CameraOffset +
                   glm::vec2(static_cast<float>(m_ViewportWidth),
                             static_cast<float>(m_ViewportHeight)) / m_Zoom;
        return rect;
    }

    glm::vec2 Renderer::ScreenToWorldPos(const glm::vec2 &mousePos) {
        float normalizedX = mousePos.x / static_cast<float>(m_ViewportWidth);
        float normalizedY = mousePos.y / static_cast<float>(m_ViewportHeight);

        float worldX = m_CameraOffset.x + (normalizedX * m_ViewportWidth) / m_Zoom;
        float worldY = m_CameraOffset.y + (normalizedY * m_ViewportHeight) / m_Zoom;

        return {worldX, worldY};
    }

    void Renderer::SetZoom(float zoom) {
        m_Zoom = zoom;
        if (m_Zoom < 0.1f) m_Zoom = 0.1f;
        if (m_Zoom > 5.0f) m_Zoom = 5.0f;
        UpdateViewMatrix();
    }

    void Renderer::ChangeCameraOffset(const glm::vec2 &offset) {
        m_CameraOffset.x -= offset.x;
        m_CameraOffset.y -= offset.y;

        UpdateViewMatrix();
    }

    void Renderer::BeginSpriteBatch() {
        m_DrawCallsThisFrame = 0;
        m_BatchVerts.clear();
        m_BatchTexture = nullptr;
        m_BatchZ = 0.0f;
    }

    void Renderer::SubmitSprite(Texture &texture, glm::vec2 position,
                                glm::vec2 size, float rotation,
                                glm::vec3 colour, float z) {
        // Flush on key change. Identity-by-pointer is safe — Texture is owned
        // by AssetManager and stable for the frame.
        const bool textureChanged = m_BatchTexture != &texture;
        const bool zChanged = z != m_BatchZ;
        if (!m_BatchVerts.empty() && (textureChanged || zChanged)) {
            FlushSpriteBatch();
        }

        // Hard cap on per-batch sprites — pre-allocated VBO can't grow.
        if (m_BatchVerts.size() + kFloatsPerSprite >
            static_cast<size_t>(kMaxBatchSprites * kFloatsPerSprite)) {
            FlushSpriteBatch();
        }

        if (m_BatchVerts.empty()) {
            m_BatchTexture = &texture;
            m_BatchZ = z;
        }

        // Compute 4 rotated/translated corners on CPU, matching DrawSprite's
        // model = T(pos) · T(half) · R(rad) · T(-half) · S(size) semantics
        // (rotate around sprite centre, then translate).
        const float hw = size.x * 0.5f;
        const float hh = size.y * 0.5f;
        const float cx = position.x + hw;
        const float cy = position.y + hh;
        const float rad = glm::radians(rotation);
        const float c = std::cos(rad);
        const float s = std::sin(rad);

        auto worldXY = [&](float lx, float ly) -> glm::vec2 {
            return {lx * c - ly * s + cx, lx * s + ly * c + cy};
        };

        const glm::vec2 tl = worldXY(-hw, -hh);
        const glm::vec2 tr = worldXY( hw, -hh);
        const glm::vec2 bl = worldXY(-hw,  hh);
        const glm::vec2 br = worldXY( hw,  hh);

        // 6 verts: (BL, BR, TR), (BL, TR, TL) — matches original unit-quad
        // winding in InitRendererData.
        auto push = [&](const glm::vec2 &p, float u, float v) {
            m_BatchVerts.push_back(p.x);
            m_BatchVerts.push_back(p.y);
            m_BatchVerts.push_back(u);
            m_BatchVerts.push_back(v);
            m_BatchVerts.push_back(colour.r);
            m_BatchVerts.push_back(colour.g);
            m_BatchVerts.push_back(colour.b);
        };
        push(bl, 0.0f, 1.0f);
        push(br, 1.0f, 1.0f);
        push(tr, 1.0f, 0.0f);
        push(bl, 0.0f, 1.0f);
        push(tr, 1.0f, 0.0f);
        push(tl, 0.0f, 0.0f);
    }

    void Renderer::FlushSpriteBatch() {
        if (m_BatchVerts.empty() || !m_BatchTexture) return;

        m_SpriteBatchShader->use();

        glActiveTexture(GL_TEXTURE0);
        m_BatchTexture->BindTexture();

        glBindVertexArray(m_BatchVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        m_BatchVerts.size() * sizeof(float),
                        m_BatchVerts.data());

        const GLsizei vertexCount =
            static_cast<GLsizei>(m_BatchVerts.size() / kFloatsPerVertex);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_BatchVerts.clear();
        m_DrawCallsThisFrame++;
    }

    void Renderer::EndSpriteBatch() {
        if (!m_BatchVerts.empty()) {
            FlushSpriteBatch();
        }
        m_BatchTexture = nullptr;
        m_DrawCallsLastFrame = m_DrawCallsThisFrame;
    }
} // namespace Hamster
