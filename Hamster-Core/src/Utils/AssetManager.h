#pragma once

#include <functional>
#include <unordered_map>

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include "Core/Project.h"
#include "Scripting/HamsterScript.h"

namespace Hamster {
    class AssetManager {
    public:
        using MainThreadEnqueue = std::function<void(std::function<void()>)>;

        explicit AssetManager(MainThreadEnqueue enqueue);
        ~AssetManager();

        std::shared_ptr<Shader>
        AddShader(std::string name, const std::string &vertexShaderPath,
                  const std::string &fragmentShaderPath);

        std::shared_ptr<Shader> GetShader(std::string name);

        std::shared_ptr<Texture> AddTextureAsync(const std::string &texturePath);

        void AddTexture(UUID uuid, const std::string &texturePath,
                               const std::string &textureName);

        std::shared_ptr<Texture> GetTexture(UUID uuid);

        const std::unordered_map<UUID, std::shared_ptr<Texture> > &
        GetTextureMap() {
            return m_Textures;
        }

        uint32_t GetTextureCount() {
            return static_cast<uint32_t>(m_Textures.size());
        }

        UUID AddScript(const std::filesystem::path &scriptPath,
                              const std::string &fileName);

        void AddScript(UUID uuid, const std::filesystem::path &scriptPath,
                              const std::string &fileName,
                              const std::string &scriptName);

        void AddScript(UUID uuid, const std::filesystem::path &scriptPath,
                              const std::string &fileName);

        UUID AddDefaultScript();

        std::shared_ptr<HamsterScript> GetScript(UUID uuid);

        const std::unordered_map<UUID, std::shared_ptr<HamsterScript> > &
        GetScriptMap() {
            return m_Scripts;
        }

        uint32_t GetScriptCount() {
            return static_cast<uint32_t>(m_Scripts.size());
        }

        void Serialise(std::ostream &out);

        void Deserialise(std::istream &in, const ProjectConfig &config);

        std::shared_ptr<Texture> AddTexture(const std::string &texturePath);

    private:
        std::unordered_map<std::string, std::shared_ptr<Shader> > m_Shaders;
        std::unordered_map<UUID, std::shared_ptr<Texture> > m_Textures;
        std::unordered_map<UUID, std::shared_ptr<HamsterScript> > m_Scripts;
        MainThreadEnqueue m_Enqueue;
    };
} // namespace Hamster
