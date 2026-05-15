#include "HamsterPCH.h"

#include "Renderer.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/Application.h"
#include "Events/WindowEvents.h"
#include "Utils/AssetManager.h"

namespace Hamster {
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
    }

    void Renderer::AdjustZoom(float factor, float mousePosX, float mousePosY) {
        glm::vec2 worldMousePos = ScreenToWorldPos({mousePosX, mousePosY});

        m_Zoom += factor;

        glm::vec2 newWorldMousePos = ScreenToWorldPos({mousePosX, mousePosY});

        UpdateViewMatrix();

        ChangeCameraOffset(newWorldMousePos - worldMousePos);
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
} // namespace Hamster
