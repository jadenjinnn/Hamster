#include "HamsterPCH.h"

#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glad/glad.h>

#include "Utils/AssetManager.h"

namespace Hamster {
Texture::Texture(const std::string &texturePath, FilterMode filter)
    : m_TexturePath(texturePath) {
  int width, height, nrChannels;

  std::cout << texturePath << std::endl;

  unsigned char *data = stbi_load(texturePath.c_str(), &width, &height,
                                  &nrChannels, STBI_rgb_alpha);

  if (!data) {
    std::cout << "Texture could not be loaded" << std::endl;
  }

  glGenTextures(1, &m_ID);

  glBindTexture(GL_TEXTURE_2D, m_ID);

  int imageFormat;

  if (nrChannels == 3) {
    imageFormat = GL_RGB;
  } else if (nrChannels == 4) {
    imageFormat = GL_RGBA;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, imageFormat, width, height, 0, imageFormat,
               GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);

  const GLint glFilter = (filter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);

  glBindTexture(GL_TEXTURE_2D, 0);

  m_Width = width;
  m_Height = height;

  stbi_image_free(data);
}

void Texture::Init(const TextureData &textData, FilterMode filter) {
  if (!textData.data) {
    std::cout << "Texture could not be loaded" << std::endl;
  }

  m_TexturePath = textData.path;

  glGenTextures(1, &m_ID);

  glBindTexture(GL_TEXTURE_2D, m_ID);

  int imageFormat;

  if (textData.nrChannels == 3) {
    imageFormat = GL_RGB;
  } else if (textData.nrChannels == 4) {
    imageFormat = GL_RGBA;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, imageFormat, textData.width, textData.height,
               0, imageFormat, GL_UNSIGNED_BYTE, textData.data);
  glGenerateMipmap(GL_TEXTURE_2D);

  const GLint glFilter = (filter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);

  glBindTexture(GL_TEXTURE_2D, 0);

  m_Width = textData.width;
  m_Height = textData.height;
}

void Texture::BindTexture() {
  // std::cout << "Hi" << std::endl;

  glBindTexture(GL_TEXTURE_2D, m_ID);
}

Texture::~Texture() {
  if (m_ID != 0) {
    glDeleteTextures(1, &m_ID);
  }
}
} // namespace Hamster
