#pragma once

#include <ostream>

#include <glad/glad.h>

#include "Core/UUID.h"

namespace Hamster {
struct TextureData {
  unsigned char *data;
  int width, height, nrChannels;
  std::string path;
};

enum class FilterMode { Linear, Nearest };

class Texture {
public:
  Texture(const std::string &texturePath,
          FilterMode filter = FilterMode::Linear);
  Texture() {};
  ~Texture();

  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;

  void Init(const TextureData &textData,
            FilterMode filter = FilterMode::Linear);

  void BindTexture();

  [[nodiscard]] const std::string &GetTexturePath() const {
    return m_TexturePath;
  }

  void SetUUID(const UUID &uuid) { m_UUID = uuid; };
  UUID GetUUID() { return m_UUID; };

  void SetName(const std::string &name) { m_TextureName = name; }
  const std::string &GetName() { return m_TextureName; }

  GLuint GetTextureId() { return m_ID; }

  int GetWidth() const { return m_Width; }
  int GetHeight() const { return m_Height; }

private:
  GLuint m_ID = 0;
  int m_Width = 0;
  int m_Height = 0;
  std::string m_TexturePath;
  std::string m_TextureName = "Untitled Texture";
  UUID m_UUID;
};
} // namespace Hamster
