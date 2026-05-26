#include "HamsterPCH.h"

#include "Renderer.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/Application.h"
#include "Events/WindowEvents.h"
#include "Utils/AssetManager.h"

#include <stb_truetype.h>

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
        if (m_UIRectVBO) glDeleteBuffers(1, &m_UIRectVBO);
        if (m_UIRectVAO) glDeleteVertexArrays(1, &m_UIRectVAO);
        if (m_UITextVBO) glDeleteBuffers(1, &m_UITextVBO);
        if (m_UITextVAO) glDeleteVertexArrays(1, &m_UITextVAO);
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
        // Shaders are disk files. Prefer the installed layout
        // (<exe>/../share/Resources/Hamster-Core/...); fall back to the baked
        // source dir for dev / in-place runs. HAMSTER_CORE_SRC_DIR is an
        // absolute build-machine path and does not exist on an installed box.
        std::string shaderProbe = Application::GetExecutablePath() +
            "/../share/Resources/Hamster-Core/Renderer/DefaultShaders/SpriteShader.vs";
        std::ifstream shaderPeek(shaderProbe);
        std::string hamsterCorePath = shaderPeek
            ? Application::GetExecutablePath() + "/../share/Resources/Hamster-Core"
            : std::string(HAMSTER_CORE_SRC_DIR);

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

        m_UIRectShader = assetManager->AddShader(
            "ui_rect",
            hamsterCorePath + "/Renderer/DefaultShaders/UIRectShader.vs",
            hamsterCorePath + "/Renderer/DefaultShaders/UIRectShader.fs");

        m_UITextShader = assetManager->AddShader(
            "ui_text",
            hamsterCorePath + "/Renderer/DefaultShaders/UITextShader.vs",
            hamsterCorePath + "/Renderer/DefaultShaders/UITextShader.fs");

        // Single bundled font. Searched first under the Hamster-Wheel
        // Resources layout (editor builds + smoke test relative paths), with
        // the source-tree fallback for in-place runs. Fail-soft: a missing
        // TTF leaves m_FontAtlas as a null-atlas wrapper and SubmitUIText
        // becomes a no-op (the renderer logs once at load).
        auto loadFont = [](const char *file) {
            std::string exeRel = Application::GetExecutablePath() +
                "/../share/Resources/Hamster-Wheel/Resources/Fonts/" + file;
            std::string srcRel = std::string(HAMSTER_CORE_SRC_DIR) +
                "/../../Hamster-Wheel/Resources/Fonts/" + file;
            std::ifstream peek(exeRel, std::ios::binary);
            return std::make_unique<FontAtlas>(peek ? exeRel : srcRel);
        };
        m_FontAtlas     = loadFont("Inter-Regular.ttf");
        m_FontAtlasBold = loadFont("Inter-Bold.ttf");

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

        // UI rect batch VBO. vec2 pos + vec4 colour = 6 floats/vertex.
        // 1024-rect budget × 6 verts/rect × 6 floats × 4 bytes ≈ 144 KB.
        constexpr int kMaxUIRects = 1024;
        constexpr int kUIFloatsPerVert = 6;
        constexpr int kUIVertsPerRect = 6;
        m_UIRectVerts.reserve(kMaxUIRects * kUIVertsPerRect * kUIFloatsPerVert);
        glGenVertexArrays(1, &m_UIRectVAO);
        glGenBuffers(1, &m_UIRectVBO);
        glBindVertexArray(m_UIRectVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_UIRectVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     kMaxUIRects * kUIVertsPerRect * kUIFloatsPerVert * sizeof(float),
                     nullptr, GL_DYNAMIC_DRAW);
        constexpr GLsizei uiStride = kUIFloatsPerVert * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, uiStride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, uiStride,
                              (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // UI text batch VBO. vec2 pos + vec2 uv + vec4 colour = 8 floats/vert.
        // 4096-glyph budget × 6 verts × 8 floats × 4 bytes ≈ 768 KB.
        constexpr int kMaxUIGlyphs = 4096;
        constexpr int kUITextFloatsPerVert = 8;
        constexpr int kUITextVertsPerGlyph = 6;
        m_UITextVerts.reserve(kMaxUIGlyphs * kUITextVertsPerGlyph * kUITextFloatsPerVert);
        glGenVertexArrays(1, &m_UITextVAO);
        glGenBuffers(1, &m_UITextVBO);
        glBindVertexArray(m_UITextVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_UITextVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     kMaxUIGlyphs * kUITextVertsPerGlyph * kUITextFloatsPerVert * sizeof(float),
                     nullptr, GL_DYNAMIC_DRAW);
        constexpr GLsizei textStride = kUITextFloatsPerVert * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, textStride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, textStride,
                              (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, textStride,
                              (void*)(4 * sizeof(float)));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

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
                              float rotation, glm::vec3 colour,
                              glm::vec4 uvRect) {
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
        m_SpriteShader->setUniformVec4("uvRect", uvRect);

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

    void Renderer::DrawWorldRectOutline(glm::vec2 origin, glm::vec2 size,
                                         glm::vec3 colour, float widthPx,
                                         float alpha) {
        m_FlatShader->use();

        m_FlatShader->setUniformi("borderMode", 1);
        m_FlatShader->setUniformf("alpha", alpha);
        m_FlatShader->setUniformf("borderWidthX", widthPx / size.x);
        m_FlatShader->setUniformf("borderWidthY", widthPx / size.y);
        m_FlatShader->setUniformVec3("colour", colour);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(origin, 0.0f));
        model = glm::scale(model, glm::vec3(size, 1.0f));

        m_FlatShader->setUniformMat4("model", model);
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

    UIRect Renderer::ResolveUIButton(const UIButton &b, float vw, float vh) const {
        UIButton local = b;
        const FontAtlas *measureAtlas =
            (b.bold && m_FontAtlasBold && m_FontAtlasBold->IsValid())
                ? m_FontAtlasBold.get()
                : m_FontAtlas.get();
        if (b.autoSize && measureAtlas && measureAtlas->IsValid()) {
            float w = measureAtlas->MeasureWidth(b.label, b.fontSize);
            local.size.x = w + 2.0f * b.padding;
            local.size.y = b.fontSize + 2.0f * b.padding;
        }
        return ResolveUIButtonRect(local, vw, vh);
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

    void Renderer::SetCameraOffset(const glm::vec2 &absolute) {
        m_CameraOffset = absolute;
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
                                glm::vec3 colour, float z,
                                glm::vec4 uvRect) {
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
        // winding in InitRendererData. Sub-sprite UVs are baked here: the
        // unit-quad's (u,v) corners map onto uvRect.xy + corner * uvRect.zw,
        // so the batch shader sees regular [uMin..uMax]×[vMin..vMax] coords
        // and needs no spritesheet-aware change of its own.
        const float u0 = uvRect.x;
        const float v0 = uvRect.y;
        const float u1 = uvRect.x + uvRect.z;
        const float v1 = uvRect.y + uvRect.w;
        auto push = [&](const glm::vec2 &p, float u, float v) {
            m_BatchVerts.push_back(p.x);
            m_BatchVerts.push_back(p.y);
            m_BatchVerts.push_back(u);
            m_BatchVerts.push_back(v);
            m_BatchVerts.push_back(colour.r);
            m_BatchVerts.push_back(colour.g);
            m_BatchVerts.push_back(colour.b);
        };
        push(bl, u0, v1);
        push(br, u1, v1);
        push(tr, u1, v0);
        push(bl, u0, v1);
        push(tr, u1, v0);
        push(tl, u0, v0);
    }

    void Renderer::FlushSpriteBatch() {
        if (m_BatchVerts.empty() || !m_BatchTexture) return;

        m_SpriteBatchShader->use();

        glActiveTexture(GL_TEXTURE0);
        m_BatchTexture->BindTexture();

        glBindVertexArray(m_PopoutMode ? m_BatchVAO_Popout : m_BatchVAO);
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

    void Renderer::CreatePopoutVertexArrays() {
        // Must run with the popout's GL context current. Recreates the three
        // VAOs the popout render uses, wiring the SAME shared VBOs with the
        // identical attribute layouts from InitRendererData (bug 0017).

        // Sprite batch: vec2 pos, vec2 uv, vec3 colour.
        glGenVertexArrays(1, &m_BatchVAO_Popout);
        glBindVertexArray(m_BatchVAO_Popout);
        glBindBuffer(GL_ARRAY_BUFFER, m_BatchVBO);
        {
            const GLsizei stride = kFloatsPerVertex * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                                  (void *)(2 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                                  (void *)(4 * sizeof(float)));
        }

        // UI rect: vec2 pos, vec4 colour (stride 6).
        glGenVertexArrays(1, &m_UIRectVAO_Popout);
        glBindVertexArray(m_UIRectVAO_Popout);
        glBindBuffer(GL_ARRAY_BUFFER, m_UIRectVBO);
        {
            const GLsizei stride = 6 * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
                                  (void *)(2 * sizeof(float)));
        }

        // UI text: vec2 pos, vec2 uv, vec4 colour (stride 8).
        glGenVertexArrays(1, &m_UITextVAO_Popout);
        glBindVertexArray(m_UITextVAO_Popout);
        glBindBuffer(GL_ARRAY_BUFFER, m_UITextVBO);
        {
            const GLsizei stride = 8 * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                                  (void *)(2 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                                  (void *)(4 * sizeof(float)));
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // GL render state is per-context too — match the editor's setup
        // (InitRendererData) so the popout blends sprites and ignores depth.
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void Renderer::BeginUIPass(float panelWidth, float panelHeight,
                               bool worldProjection) {
        m_UIRectVerts.clear();
        m_UITextVerts.clear();
        m_CurTextAtlas = nullptr;
        if (!m_UIRectShader) return;

        // Screen-space ortho aligned with the FBO's pixel grid. The level-
        // editor FBO is panel-sized, so (0,0) at top-left maps directly to
        // mouse-input coordinates — no Y-flip needed (the FBO is later
        // displayed UV-flipped, which is exactly what we want for this
        // origin convention).
        glm::mat4 ui = worldProjection
            ? m_ViewMatrix
            : glm::ortho(0.0f, panelWidth, panelHeight, 0.0f, -1.0f, 1.0f);
        m_UIProjection = ui; // SubmitUIImage draws quads in this same space.
        m_UIRectShader->use();
        m_UIRectShader->setUniformMat4("projection", ui);
        if (m_UITextShader) {
            m_UITextShader->use();
            m_UITextShader->setUniformi("image", 0);
            m_UITextShader->setUniformMat4("projection", ui);
        }
    }

    void Renderer::SubmitUIRect(const UIRect &rect, const glm::vec4 &colour) {
        if (!m_UIRectShader) return;
        const float x0 = rect.x;
        const float y0 = rect.y;
        const float x1 = rect.x + rect.w;
        const float y1 = rect.y + rect.h;

        auto push = [&](float x, float y) {
            m_UIRectVerts.push_back(x);
            m_UIRectVerts.push_back(y);
            m_UIRectVerts.push_back(colour.r);
            m_UIRectVerts.push_back(colour.g);
            m_UIRectVerts.push_back(colour.b);
            m_UIRectVerts.push_back(colour.a);
        };
        // 2 triangles, CCW with y-down: (TL, TR, BR) + (TL, BR, BL).
        push(x0, y0);
        push(x1, y0);
        push(x1, y1);
        push(x0, y0);
        push(x1, y1);
        push(x0, y1);
    }

    void Renderer::SubmitUIText(const std::string &text, glm::vec2 topLeft,
                                float fontSize, const glm::vec4 &colour,
                                float wrapWidth, bool bold) {
        const FontAtlas *atlas =
            (bold && m_FontAtlasBold && m_FontAtlasBold->IsValid())
                ? m_FontAtlasBold.get()
                : m_FontAtlas.get();
        if (!m_UITextShader || !atlas || !atlas->IsValid()) return;
        if (text.empty()) return;

        // The text batch holds glyphs for one atlas — flush before switching
        // (e.g. a regular run followed by a bold run).
        if (m_CurTextAtlas && atlas != m_CurTextAtlas && !m_UITextVerts.empty())
            FlushUIText();
        m_CurTextAtlas = atlas;

        const float scale = fontSize / FontAtlas::kBakeSize;
        const float atlasW = static_cast<float>(FontAtlas::kAtlasW);
        const float atlasH = static_cast<float>(FontAtlas::kAtlasH);
        const float lineHeight = fontSize;

        float lineTopY = topLeft.y;
        float baselineY = atlas->BaselineYFromTop(lineTopY, fontSize);
        float cursorX = topLeft.x;
        const float rowStartX = topLeft.x;

        auto pushQuad = [&](float x0, float y0, float x1, float y1,
                            float s0, float t0, float s1, float t1) {
            // 2 triangles, CCW with y-down: (TL, TR, BR) + (TL, BR, BL).
            auto v = [&](float x, float y, float u, float vv) {
                m_UITextVerts.push_back(x);
                m_UITextVerts.push_back(y);
                m_UITextVerts.push_back(u);
                m_UITextVerts.push_back(vv);
                m_UITextVerts.push_back(colour.r);
                m_UITextVerts.push_back(colour.g);
                m_UITextVerts.push_back(colour.b);
                m_UITextVerts.push_back(colour.a);
            };
            v(x0, y0, s0, t0);
            v(x1, y0, s1, t0);
            v(x1, y1, s1, t1);
            v(x0, y0, s0, t0);
            v(x1, y1, s1, t1);
            v(x0, y1, s0, t1);
        };

        // Greedy whitespace-aware wrap. wrapWidth == 0 disables breaking.
        // For each char: advance currentX; on space, snapshot a potential
        // break. When currentX exceeds wrapWidth, rewind to the last space
        // and start a new line. No hyphenation, no shaping.
        size_t i = 0;
        while (i < text.size()) {
            // Find the next word run (run of non-space chars) and the
            // following whitespace.
            size_t wordStart = i;
            while (wordStart < text.size() && text[wordStart] == ' ') ++wordStart;
            size_t wordEnd = wordStart;
            while (wordEnd < text.size() && text[wordEnd] != ' ' && text[wordEnd] != '\n') ++wordEnd;
            // wordEnd-exclusive is the slice of word chars.
            const float wordAdvance = atlas->MeasureWidth(
                text.substr(wordStart, wordEnd - wordStart), fontSize);
            const float leadingSpaceAdvance = atlas->MeasureWidth(
                text.substr(i, wordStart - i), fontSize);

            // Hard wrap if this word won't fit.
            const bool needWrap = wrapWidth > 0.0f &&
                cursorX > rowStartX &&
                (cursorX - rowStartX) + leadingSpaceAdvance + wordAdvance > wrapWidth;
            if (needWrap) {
                cursorX = rowStartX;
                lineTopY += lineHeight;
                baselineY = atlas->BaselineYFromTop(lineTopY, fontSize);
                // Skip the leading spaces of this run (they'd be rendered
                // at the start of the new line, which is ugly).
                i = wordStart;
            }

            // Emit leading spaces (if not wrapping; otherwise skipped).
            for (size_t k = i; k < wordStart; ++k) {
                if (text[k] == '\n') {
                    cursorX = rowStartX;
                    lineTopY += lineHeight;
                    baselineY = atlas->BaselineYFromTop(lineTopY, fontSize);
                    continue;
                }
                const stbtt_bakedchar *bc = atlas->GetGlyph(text[k]);
                if (!bc) continue;
                cursorX += bc->xadvance * scale;
            }

            // Emit the word.
            for (size_t k = wordStart; k < wordEnd; ++k) {
                const stbtt_bakedchar *bc = atlas->GetGlyph(text[k]);
                if (!bc) bc = atlas->GetGlyph('?');
                if (!bc) continue;
                const float gx0 = cursorX + bc->xoff * scale;
                const float gy0 = baselineY + bc->yoff * scale;
                const float gx1 = gx0 + (bc->x1 - bc->x0) * scale;
                const float gy1 = gy0 + (bc->y1 - bc->y0) * scale;
                const float s0 = bc->x0 / atlasW;
                const float t0 = bc->y0 / atlasH;
                const float s1 = bc->x1 / atlasW;
                const float t1 = bc->y1 / atlasH;
                pushQuad(gx0, gy0, gx1, gy1, s0, t0, s1, t1);
                cursorX += bc->xadvance * scale;
            }

            i = wordEnd;
        }
    }

    void Renderer::FlushUIRect() {
        if (!m_UIRectShader || m_UIRectVerts.empty()) return;
        m_UIRectShader->use();
        glBindVertexArray(m_PopoutMode ? m_UIRectVAO_Popout : m_UIRectVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_UIRectVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        m_UIRectVerts.size() * sizeof(float),
                        m_UIRectVerts.data());
        constexpr int kFloatsPerVert = 6;
        GLsizei vertCount =
            static_cast<GLsizei>(m_UIRectVerts.size() / kFloatsPerVert);
        glDrawArrays(GL_TRIANGLES, 0, vertCount);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_UIRectVerts.clear();
    }

    void Renderer::SubmitUIImage(Texture &texture, const UIRect &rect,
                                 glm::vec4 uvRect, glm::vec3 tint) {
        if (!m_SpriteBatchShader) return;
        // Immediate single-quad draw. Reuses the sprite-batch staging buffer
        // and VBO; the world batch has already flushed by the time the UI pass
        // runs, so borrowing them here is safe. We deliberately avoid
        // Begin/EndSpriteBatch so the per-frame draw-call HUD counter isn't
        // reset or finalised by this UI-side draw.
        m_SpriteBatchShader->use();
        m_SpriteBatchShader->setUniformi("image", 0);
        m_SpriteBatchShader->setUniformMat4("projection", m_UIProjection);

        m_BatchVerts.clear();
        m_BatchTexture = nullptr; // SubmitSprite adopts it on first push.
        SubmitSprite(texture, {rect.x, rect.y}, {rect.w, rect.h}, 0.0f, tint,
                     0.0f, uvRect);
        FlushSpriteBatch(); // popout-aware VAO selection lives here.

        // Restore the world projection so the next frame's world-pass batch
        // (which only calls use(), not set-projection) renders correctly.
        m_SpriteBatchShader->setUniformMat4("projection", m_ViewMatrix);
        m_BatchTexture = nullptr;
    }

    void Renderer::EndUIPass() {
        // Rects first (so text lands on top), then any remaining text. Callers
        // that mix bold/regular text flush rects explicitly before text.
        FlushUIRect();
        FlushUIText();
    }

    void Renderer::FlushUIText() {
        const FontAtlas *atlas = m_CurTextAtlas ? m_CurTextAtlas : m_FontAtlas.get();
        if (!m_UITextShader || !atlas || !atlas->IsValid() ||
            m_UITextVerts.empty())
            return;
        m_UITextShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas->GetTextureId());
        glBindVertexArray(m_PopoutMode ? m_UITextVAO_Popout : m_UITextVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_UITextVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        m_UITextVerts.size() * sizeof(float),
                        m_UITextVerts.data());
        constexpr int kFloatsPerVert = 8;
        GLsizei vertCount =
            static_cast<GLsizei>(m_UITextVerts.size() / kFloatsPerVert);
        glDrawArrays(GL_TRIANGLES, 0, vertCount);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_UITextVerts.clear();
    }
} // namespace Hamster
