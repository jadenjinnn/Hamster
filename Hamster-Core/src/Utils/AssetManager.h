#pragma once

#include <functional>
#include <unordered_map>

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include "Core/Components.h"
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

        // Walks the project directory for .py files. For each, reads its
        // sibling .py.meta to recover the persisted UUID; mints a fresh UUID
        // and writes a new sidecar when the .meta is missing. Orphan .py.meta
        // files (whose paired .py is gone) are removed. Idempotent — safe to
        // call on a fresh or partially-migrated project.
        void LoadProjectScripts(const std::filesystem::path &projectDir);

        // Walks <projectDir>/Animations for .hanim files and loads each via
        // LoadAnimationFile. .hanim files are self-identifying (UUID is
        // stored inside the file), so no sidecar is needed for animations.
        void LoadProjectAnimations(const std::filesystem::path &projectDir);

        void RemoveTexture(UUID uuid);
        void RemoveScript(UUID uuid);

        std::shared_ptr<HamsterScript> GetScript(UUID uuid);

        const std::unordered_map<UUID, std::shared_ptr<HamsterScript> > &
        GetScriptMap() {
            return m_Scripts;
        }

        uint32_t GetScriptCount() {
            return static_cast<uint32_t>(m_Scripts.size());
        }

        UUID AddAnimation(const std::string &name,
                          const std::vector<AnimationKeyframe> &keyframes);

        void AddAnimation(UUID uuid, const AnimationData &data);

        std::shared_ptr<AnimationData> GetAnimation(UUID uuid);

        void RemoveAnimation(UUID uuid);

        const std::unordered_map<UUID, std::shared_ptr<AnimationData>> &
        GetAnimationMap() {
            return m_Animations;
        }

        void SaveAnimationFile(UUID uuid, const std::filesystem::path &path);
        UUID LoadAnimationFile(const std::filesystem::path &path);

        void Serialise(std::ostream &out);

        void Deserialise(std::istream &in, const ProjectConfig &config);

        std::shared_ptr<Texture> AddTexture(const std::string &texturePath);

        // Resets project-scoped state. Call between project loads so a new
        // project does not inherit textures / scripts / animations from the
        // previously-loaded one.
        void Clear();

    private:
        std::unordered_map<std::string, std::shared_ptr<Shader> > m_Shaders;
        std::unordered_map<UUID, std::shared_ptr<Texture> > m_Textures;
        std::unordered_map<UUID, std::shared_ptr<HamsterScript> > m_Scripts;
        std::unordered_map<UUID, std::shared_ptr<AnimationData>> m_Animations;
        MainThreadEnqueue m_Enqueue;
    };
} // namespace Hamster
