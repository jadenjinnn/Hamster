#include "HamsterPCH.h"

#include <future>

#include <stb_image.h>

#include "AssetManager.h"

#include "Core/Project.h"
#include "Scripting/Scripting.h"

namespace Hamster {
    std::shared_ptr<Shader>
    AssetManager::AddShader(std::string name, const std::string &vertexShaderPath,
                            const std::string &fragmentShaderPath) {
        std::shared_ptr<Shader> shader = std::make_shared<Shader>(
            vertexShaderPath.c_str(), fragmentShaderPath.c_str());

        // m_Shaders.emplace(name, shader);

        m_Shaders[name] = shader;

        return shader;
    }

    std::shared_ptr<Shader> AssetManager::GetShader(std::string name) {
        if (name != "") {
            return m_Shaders.at(name);
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<Texture> AssetManager::AddTextureAsync(const std::string &texturePath) {
        auto futurePtr = std::make_shared<std::shared_future<TextureData> >(
            std::async(std::launch::async, [texturePath]() {
                int width, height, nrChannels;

                unsigned char *data = stbi_load(texturePath.c_str(), &width, &height,
                                                &nrChannels, STBI_rgb_alpha);

                if (!data) {
                    std::cerr << "Texture could not be loaded" << std::endl;
                }

                TextureData textData;

                textData.data = data;
                textData.width = width;
                textData.height = height;
                textData.nrChannels = nrChannels;
                textData.path = texturePath;

                return textData;
            }));

        std::shared_ptr<Texture> texture = std::make_shared<Texture>();

        m_Enqueue(
            [futurePtr, texture]() mutable {
                TextureData textData = futurePtr->get();

                texture->Init(textData); {
                    std::lock_guard<std::mutex> lock(m_TextureLoadMutex);

                    m_Textures.emplace(texture->GetUUID(), texture);

                    std::cout << "adding texture" << std::endl;
                }

                stbi_image_free(textData.data);
            });

        return texture;
    }

    std::shared_ptr<Texture> AssetManager::AddTexture(const std::string &texturePath) {
        std::cout << "Add texture with path " << texturePath << std::endl;

        std::shared_ptr<Texture> texture =
                std::make_shared<Texture>(texturePath.c_str());

        std::lock_guard<std::mutex> lock(m_TextureLoadMutex);

        m_Textures.emplace(texture->GetUUID(), texture);

        return texture;
    }

    void AssetManager::AddTexture(UUID uuid, const std::string &texturePath,
                                  const std::string &textureName) {
        std::shared_ptr<Texture> texture =
                std::make_shared<Texture>(texturePath.c_str());

        texture->SetUUID(uuid);
        texture->SetName(textureName);

        m_Textures.emplace(uuid, texture);

        for (const auto &[uuid, texture]: m_Textures) {
            std::cout << uuid.GetUUID() << " here" << std::endl;
        }
    }

    std::shared_ptr<Texture> AssetManager::GetTexture(UUID uuid) {
        return m_Textures.at(uuid);
    }

    UUID AssetManager::AddScript(const std::filesystem::path &scriptPath,
                                 const std::string &fileName) {
        std::shared_ptr<HamsterScript> script =
                std::make_shared<HamsterScript>(scriptPath, fileName);

        m_Scripts.emplace(script->GetUUID(), script);

        return script->GetUUID();
    }

    void AssetManager::AddScript(UUID uuid, const std::filesystem::path &scriptPath,
                                 const std::string &fileName,
                                 const std::string &scriptName) {
        std::shared_ptr<HamsterScript> script =
                std::make_shared<HamsterScript>(scriptPath, fileName);

        script->SetUUID(uuid);
        script->SetName(scriptName);

        m_Scripts.emplace(uuid, script);
    }

    void AssetManager::AddScript(UUID uuid, const std::filesystem::path &scriptPath,
                                 const std::string &fileName) {
        std::shared_ptr<HamsterScript> script =
                std::make_shared<HamsterScript>(scriptPath, fileName);

        script->SetUUID(uuid);

        m_Scripts.emplace(uuid, script);
    }

    UUID AssetManager::AddDefaultScript() {
        UUID scriptUUID;

        std::filesystem::path scriptPath =
                Scripting::GenerateDefaultScript(&scriptUUID);

        std::shared_ptr<HamsterScript> script =
                std::make_shared<HamsterScript>(scriptPath, scriptPath.stem().string());

        script->SetUUID(scriptUUID);

        m_Scripts.emplace(scriptUUID, script);

        return scriptUUID;
    }

    std::shared_ptr<HamsterScript> AssetManager::GetScript(UUID uuid) {
        return m_Scripts.at(uuid);
    }

    void AssetManager::Serialise(std::ostream &out) {
        uint32_t textureCount = m_Textures.size();

        out.write(reinterpret_cast<const char *>(&textureCount),
                  sizeof(textureCount));

        // serialise texture asset manager
        for (auto const &[uuid, texture]: m_Textures) {
            std::cout << "Serialising texture with uuid: " << uuid.GetUUID()
                    << std::endl;

            UUID::Serialise(out, uuid);

            std::string texturePathStr = texture->GetTexturePath();

            std::size_t texturePathLength = texturePathStr.size();
            out.write(reinterpret_cast<const char *>(&texturePathLength),
                      sizeof(texturePathLength));
            out.write(texturePathStr.data(), texturePathLength);

            std::string textureNameStr = texture->GetName();
            std::size_t textureNameLength = textureNameStr.size();
            out.write(reinterpret_cast<const char *>(&textureNameLength),
                      sizeof(textureNameLength));
            out.write(textureNameStr.data(), textureNameLength);
        }

        uint32_t scriptCount = m_Scripts.size();

        out.write(reinterpret_cast<const char *>(&scriptCount), sizeof(scriptCount));

        for (auto const &[uuid, script]: m_Scripts) {
            std::cout << "Serialising script with uuid: " << uuid.GetUUID()
                    << std::endl;

            UUID::Serialise(out, uuid);

            std::string scriptPathStr = script->GetScriptPath().string();
            std::size_t scriptPathLength = scriptPathStr.size();

            out.write(reinterpret_cast<const char *>(&scriptPathLength),
                      sizeof(scriptPathLength));
            out.write(scriptPathStr.data(), scriptPathLength);

            std::string scriptNameStr = script->GetName();
            std::size_t scriptNameLength = scriptNameStr.size();
            out.write(reinterpret_cast<const char *>(&scriptNameLength),
                      sizeof(scriptNameLength));

            out.write(scriptNameStr.data(), scriptNameLength);

            std::string fileNameStr = script->GetFileName();
            std::size_t fileNameStrLength = fileNameStr.size();

            out.write(reinterpret_cast<const char *>(&fileNameStrLength),
                      sizeof(fileNameStrLength));
            out.write(fileNameStr.data(), fileNameStrLength);
        }
    }

    void AssetManager::Deserialise(std::istream &in, const ProjectConfig &config) {
        uint32_t textureCount;
        in.read(reinterpret_cast<char *>(&textureCount), sizeof(textureCount));

        for (uint32_t i = 0; i < textureCount; i++) {
            UUID uuid = UUID::Deserialise(in);

            std::size_t texturePathLength;
            in.read(reinterpret_cast<char *>(&texturePathLength),
                    sizeof(texturePathLength));

            std::string texturePathStr(texturePathLength, '\0');
            in.read(texturePathStr.data(), texturePathLength);

            std::size_t textureNameLength;
            in.read(reinterpret_cast<char *>(&textureNameLength),
                    sizeof(textureNameLength));

            std::string textureNameStr(textureNameLength, '\0');
            in.read(textureNameStr.data(), textureNameLength);

            AddTexture(uuid, texturePathStr, textureNameStr);
        }

        uint32_t scriptCount;
        in.read(reinterpret_cast<char *>(&scriptCount), sizeof(scriptCount));

        for (uint32_t i = 0; i < scriptCount; i++) {
            UUID uuid = UUID::Deserialise(in);

            std::cout << "Deserialising script with uuid " << uuid.GetUUIDString()
                    << std::endl;

            std::size_t scriptPathLength;
            in.read(reinterpret_cast<char *>(&scriptPathLength),
                    sizeof(scriptPathLength));

            std::string scriptPathStr(scriptPathLength, '\0');
            in.read(scriptPathStr.data(), scriptPathLength);

            std::size_t scriptNameLength; // here
            in.read(reinterpret_cast<char *>(&scriptNameLength),
                    sizeof(scriptNameLength));

            std::string scriptNameStr(scriptNameLength, '\0');
            in.read(scriptNameStr.data(), scriptNameLength);

            std::filesystem::path path(scriptPathStr);

            std::size_t fileNameLength;
            in.read(reinterpret_cast<char *>(&fileNameLength), sizeof(fileNameLength));

            std::string fileNameStr(fileNameLength, '\0');
            in.read(fileNameStr.data(), fileNameLength);

            AddScript(uuid, path, fileNameStr, scriptNameStr);
        }
    }

    void AssetManager::Terminate() {
        m_Textures.clear();
        m_Shaders.clear();
    }
} // namespace Hamster
