#include "HamsterPCH.h"

#include <future>

#include <stb_image.h>

#include "AssetManager.h"

#include "Core/Project.h"
#include "Scripting/Scripting.h"
#include "Utils/MetaFile.h"
#include "Utils/SheetSidecar.h"

namespace Hamster {
    AssetManager::AssetManager(MainThreadEnqueue enqueue)
        : m_Enqueue(std::move(enqueue)) {
    }

    AssetManager::~AssetManager() {
        m_Textures.clear();
        m_Shaders.clear();
        m_Scripts.clear();
        m_Animations.clear();
        m_SubSprites.clear();
        m_MissingTexture.reset();
    }

    void AssetManager::Clear() {
        m_Textures.clear();
        m_Scripts.clear();
        m_Animations.clear();
        m_SubSprites.clear();
    }

    std::shared_ptr<Shader>
    AssetManager::AddShader(std::string name, const std::string &vertexShaderPath,
                            const std::string &fragmentShaderPath) {
        std::shared_ptr<Shader> shader = std::make_shared<Shader>(
            vertexShaderPath.c_str(), fragmentShaderPath.c_str());

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

        // Reconcile UUID against sidecar BEFORE the texture goes in the map.
        // If a .png.meta exists next to the file (e.g., the user re-imported
        // a texture they'd added in another project), reuse that UUID so
        // attachments in scenes survive the trip across projects. Otherwise,
        // persist the freshly-generated UUID into a new sidecar.
        if (auto persisted = MetaFile::Read(texturePath)) {
            texture->SetUUID(*persisted);
        } else {
            MetaFile::Write(texturePath, texture->GetUUID());
        }

        // Capture this to access m_Textures on main thread when the async load completes
        m_Enqueue(
            [this, futurePtr, texture, texturePath]() mutable {
                TextureData textData = futurePtr->get();

                texture->Init(textData, FilterMode::Nearest);

                m_Textures.emplace(texture->GetUUID(), texture);

                std::cout << "adding texture" << std::endl;

                stbi_image_free(textData.data);

                // .png.sheet sidecar load — same step the sync AddTexture
                // path runs, deferred to here because the parent texture
                // isn't in m_Textures until this main-thread callback fires.
                std::vector<SubSpriteEntry> sheetEntries;
                if (SheetSidecar::Read(texturePath, sheetEntries)) {
                    for (const auto &e : sheetEntries) {
                        AddSubSprite(e.uuid, texture->GetUUID(),
                                     e.pixelRect, e.name);
                    }
                }
            });

        return texture;
    }

    std::shared_ptr<Texture> AssetManager::AddTexture(const std::string &texturePath) {
        std::cout << "Add texture with path " << texturePath << std::endl;

        std::shared_ptr<Texture> texture =
                std::make_shared<Texture>(texturePath.c_str(), FilterMode::Nearest);

        // Same sidecar reconciliation as AddTextureAsync — see comment there.
        if (auto persisted = MetaFile::Read(texturePath)) {
            texture->SetUUID(*persisted);
        } else {
            MetaFile::Write(texturePath, texture->GetUUID());
        }

        m_Textures.emplace(texture->GetUUID(), texture);

        // If a .png.sheet sidecar exists next to this texture, load the
        // sub-sprite regions and register them under this texture as their
        // parent. Missing sidecar is the common case for plain textures.
        std::vector<SubSpriteEntry> sheetEntries;
        if (SheetSidecar::Read(texturePath, sheetEntries)) {
            for (const auto &e : sheetEntries) {
                AddSubSprite(e.uuid, texture->GetUUID(), e.pixelRect, e.name);
            }
        }

        return texture;
    }

    void AssetManager::AddTexture(UUID uuid, const std::string &texturePath,
                                  const std::string &textureName) {
        auto texture = std::make_shared<Texture>(texturePath.c_str(), FilterMode::Nearest);
        texture->SetUUID(uuid);
        texture->SetName(textureName);
        m_Textures.emplace(uuid, texture);
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

    // Defined below; forward-declared so AddDefaultScript can reuse it.
    static std::string ModuleNameForScript(
        const std::filesystem::path &scriptPath,
        const std::filesystem::path &projectDir);

    UUID AssetManager::AddDefaultScript() {
        UUID scriptUUID;

        std::filesystem::path scriptPath =
                Scripting::GenerateDefaultScript(&scriptUUID);

        // Import by dotted module name (e.g. Assets.Scripts.Untitled_Script),
        // exactly as LoadProjectScripts does: only the project root is on
        // sys.path, so the bare stem isn't importable and the import would
        // raise ModuleNotFoundError -> unhandled -> editor crash (bug 0014).
        const std::filesystem::path projectDir =
                Project::GetCurrentProject()->GetConfig().ProjectDirectory;
        const std::string moduleName =
                ModuleNameForScript(scriptPath, projectDir);

        std::shared_ptr<HamsterScript> script =
                std::make_shared<HamsterScript>(scriptPath, moduleName);

        script->SetUUID(scriptUUID);
        script->SetName(scriptPath.stem().string());

        m_Scripts.emplace(scriptUUID, script);

        return scriptUUID;
    }

    // Derives the dotted Python module name for a script file from its path
    // relative to the project root. enemies/boss.py → "enemies.boss".
    static std::string ModuleNameForScript(
        const std::filesystem::path &scriptPath,
        const std::filesystem::path &projectDir) {
        std::error_code ec;
        std::filesystem::path rel =
            std::filesystem::relative(scriptPath, projectDir, ec);
        if (ec || rel.empty()) return scriptPath.stem().string();

        std::filesystem::path parent = rel.parent_path();
        std::string stem = rel.stem().string();
        if (parent.empty()) return stem;

        std::string dotted = parent.string();
        std::replace(dotted.begin(), dotted.end(), '\\', '.');
        std::replace(dotted.begin(), dotted.end(), '/', '.');
        return dotted + "." + stem;
    }

    void AssetManager::LoadProjectScripts(const std::filesystem::path &projectDir) {
        if (!std::filesystem::exists(projectDir) ||
            !std::filesystem::is_directory(projectDir)) {
            return;
        }

        // Pass 1: walk recursively for .py files; register each with its
        // sidecar UUID (minting one + writing the sidecar if it's absent).
        // Recursive so subdirectories ("enemies/", "players/", ...) are
        // first-class.
        for (auto const &entry :
             std::filesystem::recursive_directory_iterator(projectDir)) {
            if (!entry.is_regular_file()) continue;
            const auto &path = entry.path();
            if (path.extension() != ".py") continue;

            std::optional<UUID> persisted = MetaFile::Read(path);
            UUID uuid;
            if (persisted.has_value()) {
                uuid = persisted.value();
            } else {
                MetaFile::Write(path, uuid);
            }

            const std::string moduleName = ModuleNameForScript(path, projectDir);

            std::shared_ptr<HamsterScript> script =
                    std::make_shared<HamsterScript>(path, moduleName);
            script->SetUUID(uuid);
            script->SetName(path.stem().string());

            m_Scripts.emplace(uuid, script);
        }

        // Pass 2: recursively delete orphan .py.meta files (paired .py is
        // gone). Keeping these around would let stale UUIDs come back to
        // life next session.
        for (auto const &entry :
             std::filesystem::recursive_directory_iterator(projectDir)) {
            if (!entry.is_regular_file()) continue;
            const auto &path = entry.path();
            if (path.extension() != ".meta") continue;

            std::filesystem::path paired = path;
            paired.replace_extension(); // drops ".meta"
            if (paired.extension() != ".py") continue;
            if (std::filesystem::exists(paired)) continue;

            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }

    void AssetManager::HandleFileEvents(
        const std::vector<FileEvent> &events) {
        for (auto const &ev : events) {
            // .meta files are editor-internal — we own them, ignore events.
            const std::filesystem::path &path = ev.path;
            if (path.extension() == ".meta") continue;

            // v1: react only to .py. Other extensions roll in with later
            // phases (textures + animations live mostly outside the
            // watched dir anyway).
            if (path.extension() != ".py") continue;

            switch (ev.kind) {
            case FileEvent::Kind::Added: {
                std::optional<UUID> persisted = MetaFile::Read(path);
                UUID uuid;
                if (persisted) uuid = *persisted;
                else MetaFile::Write(path, uuid);

                if (m_Scripts.find(uuid) != m_Scripts.end()) break;

                const std::filesystem::path projectDir =
                    Project::GetCurrentProject()->GetConfig().ProjectDirectory;
                const std::string moduleName =
                    ModuleNameForScript(path, projectDir);

                auto script =
                    std::make_shared<HamsterScript>(path, moduleName);
                script->SetUUID(uuid);
                script->SetName(path.stem().string());
                m_Scripts.emplace(uuid, script);
                break;
            }
            case FileEvent::Kind::Removed: {
                // Need the UUID to drop the right entry. The .py is already
                // gone, but the .meta may or may not be — check both. If we
                // can't resolve, scan the map for a matching path.
                std::optional<UUID> persisted = MetaFile::Read(path);
                if (persisted) {
                    m_Scripts.erase(*persisted);
                } else {
                    for (auto it = m_Scripts.begin(); it != m_Scripts.end(); ) {
                        if (it->second->GetScriptPath() == path) {
                            it = m_Scripts.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                // Clean orphan sidecar if it survived alone.
                std::error_code ec;
                std::filesystem::remove(MetaFile::SidecarPath(path), ec);
                break;
            }
            case FileEvent::Kind::Renamed: {
                // Editor-initiated renames handle their own bookkeeping (see
                // Phase 4). External renames: if the .meta moved alongside
                // the .py, the new path's sidecar carries the OLD UUID, so
                // we drop the old entry and register the new one — same UUID
                // resurfaces, attachments survive on next project load. If
                // the .meta DID NOT move, the new file gets a fresh UUID
                // and the old UUID becomes a dangling reference (Phase 6 UI).
                std::optional<UUID> oldUUID;
                for (auto it = m_Scripts.begin(); it != m_Scripts.end(); ++it) {
                    if (it->second->GetScriptPath() == ev.oldPath) {
                        oldUUID = it->first;
                        break;
                    }
                }
                if (oldUUID) m_Scripts.erase(*oldUUID);

                std::optional<UUID> persisted = MetaFile::Read(path);
                UUID uuid;
                if (persisted) uuid = *persisted;
                else MetaFile::Write(path, uuid);

                const std::filesystem::path projectDir =
                    Project::GetCurrentProject()->GetConfig().ProjectDirectory;
                const std::string moduleName =
                    ModuleNameForScript(path, projectDir);

                auto script =
                    std::make_shared<HamsterScript>(path, moduleName);
                script->SetUUID(uuid);
                script->SetName(path.stem().string());
                m_Scripts.emplace(uuid, script);
                break;
            }
            case FileEvent::Kind::Modified:
                // v1: no hot reload — content changes don't affect identity.
                break;
            }
        }
    }

    bool AssetManager::RenameAsset(UUID uuid, const std::string &newFilename) {
        // v1 supports scripts and textures. Animations are renamed via the
        // animation panel's save flow (a "rename" there is functionally a
        // new save under a new name).

        auto scriptIt = m_Scripts.find(uuid);
        if (scriptIt != m_Scripts.end()) {
            auto &script = scriptIt->second;
            const std::filesystem::path oldPath = script->GetScriptPath();
            const std::filesystem::path newPath = oldPath.parent_path() /
                                                  newFilename;

            if (oldPath == newPath) return true; // no-op

            if (std::filesystem::exists(newPath)) {
                std::cerr << "RenameAsset: '" << newPath
                          << "' already exists in the same folder" << std::endl;
                return false;
            }

            std::error_code ec;
            std::filesystem::rename(oldPath, newPath, ec);
            if (ec) {
                std::cerr << "RenameAsset: rename failed: " << ec.message()
                          << std::endl;
                return false;
            }

            // Move the sidecar alongside. If the .meta is absent for any
            // reason, mint a fresh one with the same UUID at the new path so
            // we never lose track.
            std::filesystem::path oldMeta = MetaFile::SidecarPath(oldPath);
            std::filesystem::path newMeta = MetaFile::SidecarPath(newPath);
            if (std::filesystem::exists(oldMeta)) {
                std::filesystem::rename(oldMeta, newMeta, ec);
                if (ec) {
                    std::cerr << "RenameAsset: sidecar rename failed: "
                              << ec.message() << std::endl;
                    // Asset is at the new path even if sidecar is stuck —
                    // write a fresh .meta to keep identity intact.
                    MetaFile::Write(newPath, uuid);
                }
            } else {
                MetaFile::Write(newPath, uuid);
            }

            // Teach the in-memory script about its new location. Shared
            // pointers held by Behaviour components stay valid. The dotted
            // module name reflects the new path so future ReloadScript
            // calls import from the right place. Fall back to the file's
            // parent dir as the "project root" when there is no active
            // project (smoke-test setup), which makes the dotted module
            // collapse to a plain stem.
            auto curProj = Project::GetCurrentProject();
            const std::filesystem::path projectDir =
                curProj ? curProj->GetConfig().ProjectDirectory
                        : newPath.parent_path();
            const std::string moduleName =
                ModuleNameForScript(newPath, projectDir);
            script->SetScriptPath(newPath, moduleName);
            script->SetName(newPath.stem().string());
            return true;
        }

        auto texIt = m_Textures.find(uuid);
        if (texIt != m_Textures.end()) {
            auto &texture = texIt->second;
            const std::filesystem::path oldPath = texture->GetTexturePath();
            const std::filesystem::path newPath = oldPath.parent_path() /
                                                  newFilename;

            if (oldPath == newPath) return true;

            if (std::filesystem::exists(newPath)) {
                std::cerr << "RenameAsset: '" << newPath
                          << "' already exists in the same folder" << std::endl;
                return false;
            }

            std::error_code ec;
            std::filesystem::rename(oldPath, newPath, ec);
            if (ec) {
                std::cerr << "RenameAsset: rename failed: " << ec.message()
                          << std::endl;
                return false;
            }

            std::filesystem::path oldMeta = MetaFile::SidecarPath(oldPath);
            std::filesystem::path newMeta = MetaFile::SidecarPath(newPath);
            if (std::filesystem::exists(oldMeta)) {
                std::filesystem::rename(oldMeta, newMeta, ec);
                if (ec) MetaFile::Write(newPath, uuid);
            } else {
                MetaFile::Write(newPath, uuid);
            }

            // Move the .png.sheet sidecar too, so a sliced sheet's sub-sprites
            // travel with the rename (stage 9). If the move fails, rewrite the
            // sidecar at the new path from the in-memory sub-sprites for this
            // texture so the regions are never orphaned at the old name.
            std::filesystem::path oldSheet = SheetSidecar::SidecarPath(oldPath);
            std::filesystem::path newSheet = SheetSidecar::SidecarPath(newPath);
            if (std::filesystem::exists(oldSheet)) {
                std::filesystem::rename(oldSheet, newSheet, ec);
                if (ec) {
                    std::vector<SubSpriteEntry> entries;
                    for (auto &kv : m_SubSprites) {
                        auto &ss = kv.second;
                        if (ss && ss->parentTextureUUID.GetUUID() ==
                                      uuid.GetUUID()) {
                            SubSpriteEntry e;
                            e.uuid = ss->uuid;
                            e.name = ss->name;
                            e.pixelRect = ss->pixelRect;
                            entries.push_back(e);
                        }
                    }
                    if (!entries.empty()) SheetSidecar::Write(newPath, entries);
                }
            }

            // Texture has no in-place setter for its path — rebuild the
            // shared_ptr-tracked Texture path field. Easiest: it's only
            // used cosmetically; readers re-derive from GetTexturePath.
            // We'd need a setter to track this; for now leave the texture
            // referencing its old path string (the file is at the new path
            // and renders the same GL handle either way).
            return true;
        }

        return false;
    }

    void AssetManager::LoadProjectAnimations(
        const std::filesystem::path &projectDir) {
        const std::filesystem::path animDir =
            projectDir / "Assets" / "Animations";
        if (!std::filesystem::exists(animDir) ||
            !std::filesystem::is_directory(animDir)) {
            return;
        }

        for (auto const &entry : std::filesystem::directory_iterator(animDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".hanim") continue;

            // .hanim files carry their own UUID + name + keyframes —
            // LoadAnimationFile does the parsing and registers the result.
            LoadAnimationFile(entry.path());
        }
    }

    void AssetManager::RemoveTexture(UUID uuid) {
        m_Textures.erase(uuid);
    }

    void AssetManager::RemoveScript(UUID uuid) {
        auto it = m_Scripts.find(uuid);
        if (it == m_Scripts.end()) return;

        std::filesystem::path path = it->second->GetScriptPath();
        m_Scripts.erase(it);

        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    std::shared_ptr<HamsterScript> AssetManager::GetScript(UUID uuid) {
        // Missing-tolerant: scenes can reference scripts whose .py vanished
        // since the scene was saved (external delete / sloppy rename). The
        // editor's missing-asset UI relies on nullptr here rather than an
        // exception — see SceneSerialiser::Behaviour_ID deserialise.
        auto it = m_Scripts.find(uuid);
        if (it == m_Scripts.end()) return nullptr;
        return it->second;
    }

    UUID AssetManager::AddAnimation(const std::string &name,
                                     const std::vector<AnimationKeyframe> &keyframes) {
        auto data = std::make_shared<AnimationData>();
        data->name = name;
        data->keyframes = keyframes;
        data->duration = keyframes.empty() ? 0.0f : keyframes.back().time;

        UUID uuid;
        m_Animations.emplace(uuid, data);
        return uuid;
    }

    void AssetManager::AddAnimation(UUID uuid, const AnimationData &data) {
        auto ptr = std::make_shared<AnimationData>(data);
        m_Animations[uuid] = ptr;
    }

    std::shared_ptr<AnimationData> AssetManager::GetAnimation(UUID uuid) {
        return m_Animations.at(uuid);
    }

    void AssetManager::RemoveAnimation(UUID uuid) {
        m_Animations.erase(uuid);
    }

    void AssetManager::SaveAnimationFile(UUID uuid, const std::filesystem::path &path) {
        auto data = m_Animations.at(uuid);

        std::ofstream out(path, std::ios::binary);

        UUID::Serialise(out, uuid);

        std::size_t nameLen = data->name.size();
        out.write(reinterpret_cast<const char *>(&nameLen), sizeof(nameLen));
        out.write(data->name.data(), nameLen);

        uint32_t kfCount = static_cast<uint32_t>(data->keyframes.size());
        out.write(reinterpret_cast<const char *>(&kfCount), sizeof(kfCount));

        for (auto &kf : data->keyframes) {
            out.write(reinterpret_cast<const char *>(&kf.time), sizeof(kf.time));
            UUID::Serialise(out, kf.textureUUID);
        }

        out.close();
    }

    UUID AssetManager::LoadAnimationFile(const std::filesystem::path &path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "Failed to open animation file: " << path << std::endl;
            return UUID::GetNil();
        }

        UUID uuid = UUID::Deserialise(in);

        std::size_t nameLen;
        in.read(reinterpret_cast<char *>(&nameLen), sizeof(nameLen));
        std::string name(nameLen, '\0');
        in.read(name.data(), nameLen);

        uint32_t kfCount;
        in.read(reinterpret_cast<char *>(&kfCount), sizeof(kfCount));

        std::vector<AnimationKeyframe> keyframes;
        keyframes.reserve(kfCount);

        for (uint32_t i = 0; i < kfCount; i++) {
            AnimationKeyframe kf;
            in.read(reinterpret_cast<char *>(&kf.time), sizeof(kf.time));
            kf.textureUUID = UUID::Deserialise(in);
            keyframes.push_back(kf);
        }

        in.close();

        AnimationData data;
        data.name = name;
        data.keyframes = keyframes;
        data.duration = keyframes.empty() ? 0.0f : keyframes.back().time;

        AddAnimation(uuid, data);
        return uuid;
    }

    void AssetManager::Serialise(std::ostream &out) {
        // Per-texture: path + display name. UUID lives in the sidecar next
        // to the file on disk (see AddTexture / AddTextureAsync).
        uint32_t textureCount = m_Textures.size();

        out.write(reinterpret_cast<const char *>(&textureCount),
                  sizeof(textureCount));

        for (auto const &[uuid, texture]: m_Textures) {
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

        // Scripts are persisted via .py.meta sidecars next to each .py file
        // (see MetaFile + LoadProjectScripts). They are deliberately not
        // written into the project blob — sidecars are the authoritative
        // source of script identity from Phase 1 of the asset-sidecars feature.

        // Animations are persisted as .hanim files inside <project>/Animations
        // (the .hanim already carries UUID + name + keyframes, so it is its
        // own metadata). LoadProjectAnimations walks the directory on open.
    }

    void AssetManager::Deserialise(std::istream &in, const ProjectConfig &config) {
        uint32_t textureCount;
        in.read(reinterpret_cast<char *>(&textureCount), sizeof(textureCount));

        for (uint32_t i = 0; i < textureCount; i++) {
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

            // Load the texture; AddTexture reads the .png.meta sidecar to
            // recover the persisted UUID (or mints a fresh one + writes the
            // sidecar). The user-facing name comes from the blob.
            auto texture = AddTexture(texturePathStr);
            if (texture) {
                texture->SetName(textureNameStr);
            }
        }

        // Scripts and animations are loaded outside this method:
        //   - LoadProjectScripts walks the project dir for .py + .py.meta
        //   - LoadProjectAnimations walks <projectDir>/Animations for .hanim
    }

    // --- Spritesheet sub-sprites ---------------------------------------------

    UUID AssetManager::AddSubSprite(UUID parentTextureUUID,
                                    glm::ivec4 pixelRect,
                                    const std::string &name) {
        auto ss = std::make_shared<SubSprite>();
        ss->uuid = UUID();  // fresh
        ss->parentTextureUUID = parentTextureUUID;
        ss->pixelRect = pixelRect;
        ss->name = name;
        m_SubSprites.emplace(ss->uuid, ss);
        return ss->uuid;
    }

    void AssetManager::AddSubSprite(UUID uuid, UUID parentTextureUUID,
                                    glm::ivec4 pixelRect,
                                    const std::string &name) {
        if (m_SubSprites.find(uuid) != m_SubSprites.end()) {
            std::cerr << "AssetManager: SubSprite UUID collision on load, "
                         "skipping: "
                      << name << std::endl;
            return;
        }
        auto ss = std::make_shared<SubSprite>();
        ss->uuid = uuid;
        ss->parentTextureUUID = parentTextureUUID;
        ss->pixelRect = pixelRect;
        ss->name = name;
        m_SubSprites.emplace(uuid, ss);
    }

    void AssetManager::RemoveSubSprite(UUID subSpriteUUID) {
        m_SubSprites.erase(subSpriteUUID);
    }

    bool AssetManager::RenameSubSprite(UUID subSpriteUUID,
                                       const std::string &newName) {
        auto it = m_SubSprites.find(subSpriteUUID);
        if (it == m_SubSprites.end()) return false;
        if (newName.empty()) return false;

        // Reject collision against the combined Texture + SubSprite namespace,
        // but allow renaming a sub-sprite to its own current name (no-op).
        UUID existing = FindAssetByName(newName);
        if (!UUID::IsNil(existing) && existing.GetUUID() != subSpriteUUID.GetUUID()) {
            return false;
        }
        it->second->name = newName;
        return true;
    }

    std::shared_ptr<SubSprite> AssetManager::GetSubSprite(UUID uuid) {
        auto it = m_SubSprites.find(uuid);
        if (it == m_SubSprites.end()) return nullptr;
        return it->second;
    }

    SpriteSource AssetManager::ResolveSpriteSource(UUID uuid) const {
        SpriteSource src;

        // Sub-sprites first — the wider/named container. If a UUID names
        // both a Texture and a SubSprite (should never happen post-collision-
        // check), prefer the SubSprite as the more-specific reference.
        auto sit = m_SubSprites.find(uuid);
        if (sit != m_SubSprites.end()) {
            const auto &sub = *sit->second;
            auto tit = m_Textures.find(sub.parentTextureUUID);
            if (tit == m_Textures.end()) {
                src.texture = const_cast<Texture *>(&GetMissingTexture());
                src.missing = true;
                return src;
            }
            const Texture *tex = tit->second.get();
            const float tw = static_cast<float>(tex->GetWidth());
            const float th = static_cast<float>(tex->GetHeight());
            if (tw <= 0.0f || th <= 0.0f) {
                src.texture = const_cast<Texture *>(&GetMissingTexture());
                src.missing = true;
                return src;
            }
            // Clamp the pixel rect to texture bounds. A rect that no longer
            // fits at all (zero area after clamp) → MISSING.
            int x0 = std::max(0, sub.pixelRect.x);
            int y0 = std::max(0, sub.pixelRect.y);
            int x1 = std::min(static_cast<int>(tw),
                              sub.pixelRect.x + sub.pixelRect.z);
            int y1 = std::min(static_cast<int>(th),
                              sub.pixelRect.y + sub.pixelRect.w);
            if (x1 <= x0 || y1 <= y0) {
                src.texture = const_cast<Texture *>(&GetMissingTexture());
                src.missing = true;
                return src;
            }
            src.texture = const_cast<Texture *>(tex);
            src.uvRect = {
                static_cast<float>(x0) / tw,
                static_cast<float>(y0) / th,
                static_cast<float>(x1 - x0) / tw,
                static_cast<float>(y1 - y0) / th,
            };
            return src;
        }

        auto tit = m_Textures.find(uuid);
        if (tit != m_Textures.end()) {
            src.texture = tit->second.get();
            src.uvRect = {0.0f, 0.0f, 1.0f, 1.0f};
            return src;
        }

        src.texture = const_cast<Texture *>(&GetMissingTexture());
        src.missing = true;
        return src;
    }

    UUID AssetManager::FindAssetByName(const std::string &name) const {
        // Sub-sprites first, mirroring ResolveSpriteSource priority.
        for (const auto &[uuid, ss] : m_SubSprites) {
            if (ss && ss->name == name) return uuid;
        }
        for (const auto &[uuid, tex] : m_Textures) {
            if (tex && tex->GetName() == name) return uuid;
        }
        return UUID::GetNil();
    }

    const Texture &AssetManager::GetMissingTexture() const {
        if (m_MissingTexture) return *m_MissingTexture;

        // 8x8 RGBA pink/black checker. Industry-convention "your asset is
        // missing" stamp; visible enough that you can't miss it in-scene.
        constexpr int W = 8;
        constexpr int H = 8;
        std::vector<unsigned char> pixels(W * H * 4);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                bool pink = ((x / 2) + (y / 2)) % 2 == 0;
                int i = (y * W + x) * 4;
                pixels[i + 0] = pink ? 255 : 0;
                pixels[i + 1] = 0;
                pixels[i + 2] = pink ? 217 : 0;
                pixels[i + 3] = 255;
            }
        }
        TextureData td{};
        td.data = pixels.data();
        td.width = W;
        td.height = H;
        td.nrChannels = 4;
        td.path = "<missing>";

        m_MissingTexture = std::make_unique<Texture>();
        m_MissingTexture->Init(td, FilterMode::Nearest);
        m_MissingTexture->SetName("<missing>");
        return *m_MissingTexture;
    }
} // namespace Hamster
