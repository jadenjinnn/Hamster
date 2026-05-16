#include "HamsterPCH.h"

#include <future>

#include <stb_image.h>

#include "AssetManager.h"

#include "Core/Project.h"
#include "Scripting/Scripting.h"

namespace Hamster {
    AssetManager::AssetManager(MainThreadEnqueue enqueue)
        : m_Enqueue(std::move(enqueue)) {
    }

    AssetManager::~AssetManager() {
        m_Textures.clear();
        m_Shaders.clear();
        m_Scripts.clear();
        m_Animations.clear();
    }

    void AssetManager::Clear() {
        m_Textures.clear();
        m_Scripts.clear();
        m_Animations.clear();
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

        // Capture this to access m_Textures on main thread when the async load completes
        m_Enqueue(
            [this, futurePtr, texture]() mutable {
                TextureData textData = futurePtr->get();

                texture->Init(textData);

                m_Textures.emplace(texture->GetUUID(), texture);

                std::cout << "adding texture" << std::endl;

                stbi_image_free(textData.data);
            });

        return texture;
    }

    std::shared_ptr<Texture> AssetManager::AddTexture(const std::string &texturePath) {
        std::cout << "Add texture with path " << texturePath << std::endl;

        std::shared_ptr<Texture> texture =
                std::make_shared<Texture>(texturePath.c_str());

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
        return m_Scripts.at(uuid);
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
        uint32_t textureCount = m_Textures.size();

        out.write(reinterpret_cast<const char *>(&textureCount),
                  sizeof(textureCount));

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

        uint32_t animCount = static_cast<uint32_t>(m_Animations.size());
        out.write(reinterpret_cast<const char *>(&animCount), sizeof(animCount));

        for (auto const &[uuid, anim] : m_Animations) {
            UUID::Serialise(out, uuid);

            std::size_t nameLen = anim->name.size();
            out.write(reinterpret_cast<const char *>(&nameLen), sizeof(nameLen));
            out.write(anim->name.data(), nameLen);

            uint32_t kfCount = static_cast<uint32_t>(anim->keyframes.size());
            out.write(reinterpret_cast<const char *>(&kfCount), sizeof(kfCount));

            for (auto &kf : anim->keyframes) {
                out.write(reinterpret_cast<const char *>(&kf.time), sizeof(kf.time));
                UUID::Serialise(out, kf.textureUUID);
            }
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

            std::size_t scriptNameLength;
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

        uint32_t animCount;
        if (in.read(reinterpret_cast<char *>(&animCount), sizeof(animCount))) {
            for (uint32_t i = 0; i < animCount; i++) {
                UUID uuid = UUID::Deserialise(in);

                std::size_t nameLen;
                in.read(reinterpret_cast<char *>(&nameLen), sizeof(nameLen));
                std::string name(nameLen, '\0');
                in.read(name.data(), nameLen);

                uint32_t kfCount;
                in.read(reinterpret_cast<char *>(&kfCount), sizeof(kfCount));

                std::vector<AnimationKeyframe> keyframes;
                keyframes.reserve(kfCount);

                for (uint32_t j = 0; j < kfCount; j++) {
                    AnimationKeyframe kf;
                    in.read(reinterpret_cast<char *>(&kf.time), sizeof(kf.time));
                    kf.textureUUID = UUID::Deserialise(in);
                    keyframes.push_back(kf);
                }

                AnimationData data;
                data.name = name;
                data.keyframes = keyframes;
                data.duration = keyframes.empty() ? 0.0f : keyframes.back().time;

                AddAnimation(uuid, data);
            }
        }
    }
} // namespace Hamster
