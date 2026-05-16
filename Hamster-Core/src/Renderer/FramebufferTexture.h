//
// Created by jaden on 30/07/24.
//

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

namespace Hamster {
    class FramebufferTexture {
    public:
        FramebufferTexture(unsigned int width, unsigned int height);
        ~FramebufferTexture();

        FramebufferTexture(const FramebufferTexture &) = delete;
        FramebufferTexture &operator=(const FramebufferTexture &) = delete;

        void Bind();

        void Unbind();

        void ResizeFrameBuffer(unsigned int width, unsigned int height);

        unsigned int GetTextureID();

    private:
        unsigned int m_ID = 0;
        unsigned int m_TextureID = 0;
        unsigned int m_RenderBufferID = 0;
        unsigned int m_Width = 0;
        unsigned int m_Height = 0;
    };
} // Hamster

#endif //FRAMEBUFFER_H
