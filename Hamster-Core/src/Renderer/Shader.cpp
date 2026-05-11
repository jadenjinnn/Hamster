#include "HamsterPCH.h"

#include "Shader.h"

#include <sstream>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "Utils/AssetManager.h"

namespace Hamster {
Shader::Shader(const char *vertexShaderPath, const char *fragmentShaderPath) {
  std::string vertexCode;
  std::string fragmentCode;
  std::ifstream vertexShaderFile;
  std::ifstream fragmentShaderFile;

  vertexShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  fragmentShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  try {
    vertexShaderFile.open(vertexShaderPath);
    fragmentShaderFile.open(fragmentShaderPath);

    std::stringstream vertexShaderStream, fragmentShaderStream;

    vertexShaderStream << vertexShaderFile.rdbuf();
    fragmentShaderStream << fragmentShaderFile.rdbuf();

    vertexShaderFile.close();
    fragmentShaderFile.close();

    vertexCode = vertexShaderStream.str();
    fragmentCode = fragmentShaderStream.str();
  } catch (std::ifstream::failure e) {
    std::cout << "Shader file could not be successfully read " << e.what()
              << std::endl;
  }

  const char *vertexShaderCode = vertexCode.c_str();
  const char *fragmentShaderCode = fragmentCode.c_str();

  unsigned int vertexShader, fragmentShader;

  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderCode, NULL);
  glCompileShader(vertexShader);

  fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderCode, NULL);
  glCompileShader(fragmentShader);

  int success;
  char infoLog[512];

  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);

    std::cout << "Vertex Shader could not be compiled " << infoLog << std::endl;
  }

  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);

    std::cout << "Fragment Shader could not be compiled " << infoLog
              << std::endl;
  }

  m_shaderID = glCreateProgram();
  glAttachShader(m_shaderID, vertexShader);
  glAttachShader(m_shaderID, fragmentShader);
  glLinkProgram(m_shaderID);

  glGetProgramiv(m_shaderID, GL_LINK_STATUS, &success);

  if (!success) {
    glGetProgramInfoLog(m_shaderID, 512, NULL, infoLog);

    std::cout << "Could not link program " << infoLog << std::endl;
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}

void Shader::use() { glUseProgram(m_shaderID); }

void Shader::setUniformi(const char *name, int value) {
  glUniform1i(glGetUniformLocation(m_shaderID, name), value);
}

void Shader::setUniformf(const char *name, float value) {
  glUniform1f(glGetUniformLocation(m_shaderID, name), value);
}

void Shader::setUniformMat4(const char *name, const glm::mat4 &value) {
  glUniformMatrix4fv(glGetUniformLocation(m_shaderID, name), 1, GL_FALSE,
                     glm::value_ptr(value));
}

void Shader::setUniformVec3(const char *name, const glm::vec3 &value) {
  glUniform3fv(glGetUniformLocation(m_shaderID, name), 1, &value[0]);
}
} // namespace Hamster
