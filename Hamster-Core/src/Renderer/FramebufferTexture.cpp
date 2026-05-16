//
// Created by jaden on 30/07/24.
//

#include "HamsterPCH.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "FramebufferTexture.h"

namespace Hamster {
FramebufferTexture::FramebufferTexture(unsigned int width,
                                       unsigned int height)
    : m_Width(width), m_Height(height) {
  glGenFramebuffers(1, &m_ID);
  glBindFramebuffer(GL_FRAMEBUFFER, m_ID);

  glGenTextures(1, &m_TextureID);
  glBindTexture(GL_TEXTURE_2D, m_TextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         m_TextureID, 0);

  glGenRenderbuffers(1, &m_RenderBufferID);
  glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBufferID);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, m_RenderBufferID);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

FramebufferTexture::~FramebufferTexture() {
  if (m_RenderBufferID != 0) glDeleteRenderbuffers(1, &m_RenderBufferID);
  if (m_TextureID != 0)      glDeleteTextures(1, &m_TextureID);
  if (m_ID != 0)             glDeleteFramebuffers(1, &m_ID);
}

void FramebufferTexture::Bind() { glBindFramebuffer(GL_FRAMEBUFFER, m_ID); }

void FramebufferTexture::Unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void FramebufferTexture::ResizeFrameBuffer(unsigned int width,
                                           unsigned int height) {
  // No-op when size unchanged. EditorLayer calls this every frame; without
  // this guard the driver reallocates GPU storage 60×/second and leaves the
  // previous allocation pending release, inflating the kernel handle count.
  if (width == m_Width && height == m_Height) return;

  m_Width = width;
  m_Height = height;

  glBindTexture(GL_TEXTURE_2D, m_TextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         m_TextureID, 0);

  glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBufferID);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, m_RenderBufferID);
}

unsigned int FramebufferTexture::GetTextureID() { return m_TextureID; }

} // namespace Hamster
