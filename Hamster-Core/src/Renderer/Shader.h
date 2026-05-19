#pragma once

#include <glm/glm.hpp>

namespace Hamster {
class Shader {
public:
  Shader(const char *vertexShaderPath, const char *fragmentShaderPath);
  ~Shader();

  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;

  void use();

  void setUniformi(const char *name, int value);

  void setUniformf(const char *name, float value);

  void setUniformMat4(const char *name, const glm::mat4 &value);

  void setUniformVec3(const char *name, const glm::vec3 &value);

  void setUniformVec4(const char *name, const glm::vec4 &value);

private:
  unsigned int m_shaderID = 0;
};
} // namespace Hamster
