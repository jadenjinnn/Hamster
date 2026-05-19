#pragma once

#include <functional>
#include <unordered_map>

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

#include "Core/Components.h"
#include "Core/Project.h"
#include "Scripting/HamsterScript.h"
#include "Utils/ProjectWatcher.h"

namespace Hamster {
    // Region cut from a parent texture (spritesheet support). Owns nothing
    // GL-side — the parent texture's handle is shared. pixelRect uses the
    // source texture's pixel coords (origin top-left, +y down).
    struct SubSprite {
        UUID uuid;
        UUID parentTextureUUID;
        glm::ivec4 pixelRect;  // x, y, w, h
        std::string name;
    };

    // Resolution result returned by AssetManager::ResolveSpriteSource — the
    // single hot-path lookup that hides whether a Sprite UUID points at a
    // Texture, a SubSprite, or nothing. Renderer + Scene::OnRender forward
    // (texture, uvRect) onto the existing DrawSprite path.
    struct SpriteSource {
        Texture *texture = nullptr;
        glm::vec4 uvRect = {0.0f, 0.0f, 1.0f, 1.0f};  // normalised
        bool missing = false;
    };

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

        // Test-only helper: register a texture under a specific UUID and
        // name without going through the sidecar reconciliation path. Used
        // by the smoke test to set up dummy animation frames where the path
        // is never actually loaded from disk.
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

        // Reconcile a batch of file system events from ProjectWatcher. Runs
        // on the main thread (the watcher posts via the main-thread enqueue).
        // Handles .py add/remove/rename in v1 — other extensions are ignored.
        void HandleFileEvents(const std::vector<FileEvent> &events);

        // Editor-initiated rename: moves both <asset> and <asset>.meta on
        // disk to a new filename in the same folder. Returns false if the
        // new filename collides with an existing asset of the same type in
        // that folder, or if the UUID doesn't match a registered asset.
        // The UUID is preserved, so entity attachments survive.
        bool RenameAsset(UUID uuid, const std::string &newFilename);

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

        // --- Spritesheet sub-sprites ---
        // Authoring path: mints a fresh UUID and registers a SubSprite under
        // the given parent texture. Used by the SpritesheetEditor on Save.
        UUID AddSubSprite(UUID parentTextureUUID, glm::ivec4 pixelRect,
                          const std::string &name);

        // Loading path: registers a SubSprite under a UUID already chosen
        // (read from a .png.sheet sidecar). Skips + logs on UUID collision.
        void AddSubSprite(UUID uuid, UUID parentTextureUUID,
                          glm::ivec4 pixelRect, const std::string &name);

        void RemoveSubSprite(UUID subSpriteUUID);

        // Rename — collision-checked against the combined Texture +
        // SubSprite name namespace. Returns false on collision; the editor
        // surfaces this as an inline error.
        bool RenameSubSprite(UUID subSpriteUUID, const std::string &newName);

        std::shared_ptr<SubSprite> GetSubSprite(UUID uuid);

        const std::unordered_map<UUID, std::shared_ptr<SubSprite>> &
        GetSubSpriteMap() {
            return m_SubSprites;
        }

        // The single hot-path resolver. Sprite UUID could be a Texture, a
        // SubSprite, or unresolvable (parent deleted). Returns a pink-black
        // MISSING fallback texture in the unresolvable case.
        SpriteSource ResolveSpriteSource(UUID uuid) const;

        // Flat-name lookup across the unified Texture + SubSprite namespace.
        // Returns UUID::GetNil() if no match. EntityHandle::set_texture and
        // the Spritesheet Editor's rename-collision check both consume this.
        UUID FindAssetByName(const std::string &name) const;

        void Serialise(std::ostream &out);

        void Deserialise(std::istream &in, const ProjectConfig &config);

        std::shared_ptr<Texture> AddTexture(const std::string &texturePath);

        // Resets project-scoped state. Call between project loads so a new
        // project does not inherit textures / scripts / animations from the
        // previously-loaded one.
        void Clear();

    private:
        // Lazy-built pink-black 8x8 checker texture returned by
        // ResolveSpriteSource when a UUID can't be resolved. Generated
        // programmatically — no PNG dep — so the binary stays self-contained.
        const Texture &GetMissingTexture() const;
        mutable std::unique_ptr<Texture> m_MissingTexture;

        std::unordered_map<std::string, std::shared_ptr<Shader> > m_Shaders;
        std::unordered_map<UUID, std::shared_ptr<Texture> > m_Textures;
        std::unordered_map<UUID, std::shared_ptr<HamsterScript> > m_Scripts;
        std::unordered_map<UUID, std::shared_ptr<AnimationData>> m_Animations;
        std::unordered_map<UUID, std::shared_ptr<SubSprite>> m_SubSprites;
        MainThreadEnqueue m_Enqueue;
    };
} // namespace Hamster
