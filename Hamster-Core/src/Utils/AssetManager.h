#pragma once

#include <mutex>

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include "Core/Project.h"
#include "Scripting/HamsterScript.h"

namespace Hamster {
    class AssetManager {
    public:
        using MainThreadEnqueue = std::function<void(std::function<void()>)>;

        static void Init(MainThreadEnqueue enqueue) { m_Enqueue = std::move(enqueue); }
        static std::shared_ptr<Shader>
        AddShader(std::string name, const std::string &vertexShaderPath,
                  const std::string &fragmentShaderPath);

        static std::shared_ptr<Shader> GetShader(std::string name);

        static std::shared_ptr<Texture> AddTextureAsync(const std::string &texturePath);

        static void AddTexture(UUID uuid, const std::string &texturePath,
                               const std::string &textureName);

        static std::shared_ptr<Texture> GetTexture(UUID uuid);

        static const std::unordered_map<UUID, std::shared_ptr<Texture> > &
        GetTextureMap() {
            return m_Textures;
        }

        static uint32_t GetTextureCount() {
            return static_cast<uint32_t>(m_Textures.size());
        }

        static UUID AddScript(const std::filesystem::path &scriptPath,
                              const std::string &fileName);

        static void AddScript(UUID uuid, const std::filesystem::path &scriptPath,
                              const std::string &fileName,
                              const std::string &scriptName);

        static void AddScript(UUID uuid, const std::filesystem::path &scriptPath,
                              const std::string &fileName);

        static UUID AddDefaultScript();

        static std::shared_ptr<HamsterScript> GetScript(UUID uuid);

        //
        static const std::unordered_map<UUID, std::shared_ptr<HamsterScript> > &
        GetScriptMap() {
            return m_Scripts;
        }

        //
        static uint32_t GetScriptCount() {
            return static_cast<uint32_t>(m_Scripts.size());
        }

        static void Serialise(std::ostream &out);

        static void Deserialise(std::istream &in, const ProjectConfig &config);

        static void Terminate();

        static std::shared_ptr<Texture> AddTexture(const std::string &texturePath);

    private:
        inline static std::unordered_map<std::string, std::shared_ptr<Shader> >
        m_Shaders;
        inline static std::unordered_map<UUID, std::shared_ptr<Texture> > m_Textures;
        inline static std::unordered_map<UUID, std::shared_ptr<HamsterScript> >
        m_Scripts;
        inline static std::mutex m_TextureLoadMutex;
        inline static MainThreadEnqueue m_Enqueue;
    };
} // namespace Hamster
