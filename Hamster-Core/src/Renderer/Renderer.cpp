#include "HamsterPCH.h"

#include "Renderer.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Utils/AssetManager.h"

namespace Hamster {
    void Renderer::Init(int viewportHeight, int viewportWidth, AssetManager *assetManager) {
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
            std::cout << "Failed to initialise glad" << std::endl;
        }

        m_ViewportHeight = viewportHeight;
        m_ViewportWidth = viewportWidth;


        // glEnable(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        Renderer::InitRendererData(assetManager);
    }

    void Renderer::Terminate() {
    }

    void Renderer::SetViewport(FramebufferResizeEvent &e) {
        m_ViewportWidth = e.GetWidth();
        m_ViewportHeight = e.GetHeight();

        glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);

        Renderer::UpdateViewMatrix();
    }

    void Renderer::SetViewport(int height, int width) {
        m_ViewportHeight = height;
        m_ViewportWidth = width;

        glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);

        Renderer::UpdateViewMatrix();
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

        Renderer::SetViewport(m_ViewportHeight, m_ViewportWidth);
        Renderer::SetClearColour(0.0f, 0.0f, 0.0f, 1.0f);

        m_SpriteShader->use();
        m_SpriteShader->setUniformi("image", 0);
        m_SpriteShader->setUniformMat4("projection", m_ViewMatrix);

        m_FlatShader->use();
        m_FlatShader->setUniformi("image", 0);
        m_FlatShader->setUniformMat4("projection", m_ViewMatrix);


        // AssetManager::AddShader("sprite", shader);

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

        // Unbind the VAO and VBO
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void Renderer::DrawSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                              float rotation, glm::vec3 colour) {
        m_SpriteShader->use();

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(position, 0.0f));
        //
        // // Code from learnopengl.com, used to translate sprite so rotation is based
        // // around centre of sprite
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

        // DrawGuizmo(position, Translate);
    }

    void Renderer::DrawFlat(glm::vec2 position, glm::vec2 size, float rotation,
                            glm::vec3 colour) {
        m_FlatShader->use();

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(position, 0.0f));
        //
        // // Code from learnopengl.com, used to translate sprite so rotation is based
        // // around centre of sprite
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


        glm::mat4 model = glm::mat4(1.0f);


        glBindVertexArray(0);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(targetTransform.position.x,
                                                targetTransform.position.y, 0.0f));
        model = glm::scale(model, glm::vec3(10.0f, 10.0f, 1.0f));
        // model = glm::scale(model, glm::vec3(1000.0f));

        m_FlatShader->setUniformMat4("model", model);

        if (selectionColour) {
            m_FlatShader->setUniformVec3("colour",
                                         Application::IdToColour(TopLeftGrabberID));
        } else {
            m_FlatShader->setUniformVec3(
                "colour", glm::vec3(60.0f / 255.0f, 219.0f / 255.0f, 211.0f / 255.0f));
        }

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(targetTransform.position.x +
                                                targetTransform.size.x - 10.0f,
                                                targetTransform.position.y, 0.0f));
        model = glm::scale(model, glm::vec3(10.0f, 10.0f, 1.0f));
        // model = glm::scale(model, glm::vec3(1000.0f));

        m_FlatShader->setUniformMat4("model", model);

        if (selectionColour) {
            m_FlatShader->setUniformVec3("colour",
                                         Application::IdToColour(TopRightGrabberID));
        } else {
            m_FlatShader->setUniformVec3(
                "colour", glm::vec3(60.0f / 255.0f, 219.0f / 255.0f, 211.0f / 255.0f));
        }

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(targetTransform.position.x,
                                                targetTransform.position.y +
                                                targetTransform.size.y - 10.0f,
                                                0.0f));
        model = glm::scale(model, glm::vec3(10.0f, 10.0f, 1.0f));
        // model = glm::scale(model, glm::vec3(1000.0f));

        m_FlatShader->setUniformMat4("model", model);

        if (selectionColour) {
            m_FlatShader->setUniformVec3("colour",
                                         Application::IdToColour(BottomLeftGrabberID));
        } else {
            m_FlatShader->setUniformVec3(
                "colour", glm::vec3(60.0f / 255.0f, 219.0f / 255.0f, 211.0f / 255.0f));
        }

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);

        model = glm::mat4(1.0f);
        model = glm::translate(
            model,
            glm::vec3(targetTransform.position.x + targetTransform.size.x - 10.0f,
                      targetTransform.position.y + targetTransform.size.y - 10.0f,
                      0.0f));
        model = glm::scale(model, glm::vec3(10.0f, 10.0f, 1.0f));
        // model = glm::scale(model, glm::vec3(1000.0f));

        m_FlatShader->setUniformMat4("model", model);

        if (selectionColour) {
            m_FlatShader->setUniformVec3("colour",
                                         Application::IdToColour(BottomRightGrabberID));
        } else {
            m_FlatShader->setUniformVec3(
                "colour", glm::vec3(60.0f / 255.0f, 219.0f / 255.0f, 211.0f / 255.0f));
        }

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);
    }

    void Renderer::UpdateViewMatrix() {
        // std::cout << m_CameraOffset.x << ", " << m_CameraOffset.y << std::endl;

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

    void Renderer::ChangeCameraOffset(const glm::vec2 &offset) {
        m_CameraOffset.x -= offset.x;
        m_CameraOffset.y -= offset.y;

        UpdateViewMatrix();
    }
} // namespace Hamster
