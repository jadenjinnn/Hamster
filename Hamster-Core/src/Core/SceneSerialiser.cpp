//
// Created by Jaden on 29/08/2024.
//

#include "HamsterPCH.h"

#include "SceneSerialiser.h"

#include <boost/uuid/uuid_io.hpp>
#include <entt/entt.hpp>

#include "Utils/AssetManager.h"
#include <chrono>

namespace Hamster {
SceneSerialiser::SceneSerialiser(std::shared_ptr<Scene> scene,
                                 AssetManager *assetManager)
    : m_Scene(std::move(scene)), m_AssetManager(assetManager) {}

void SceneSerialiser::Serialise(std::ostream &out) {
  boost::uuids::uuid uuidValue = m_Scene->GetUUID().GetUUID();

  out.write(reinterpret_cast<const char *>(&uuidValue), uuidValue.size());

  uint32_t entityCount = m_Scene->GetEntityCount();

  std::cout << "Entity count: " << entityCount << std::endl;

  out.write(reinterpret_cast<const char *>(&entityCount), sizeof(entityCount));

  for (auto const &[uuid, entity] : m_Scene->GetEntityMap()) {
    SerialiseEntity(out, entity, uuid);
  }
}

void SceneSerialiser::Deserialise(std::istream &in) {
  boost::uuids::uuid boostUUID;
  in.read(reinterpret_cast<char *>(&boostUUID), boostUUID.size());

  std::cout << "Boost UUID: " << boostUUID << std::endl;

  UUID uuid(boostUUID);

  m_Scene->SetUUID(uuid);

  uint32_t count;
  in.read(reinterpret_cast<char *>(&count), sizeof(count));

  std::cout << "Entity count: " << count << std::endl;

  for (uint32_t i = 0; i < count; i++) {

    DeserialiseEntity(in);
  }

  // Now that every entity (and its Hierarchy component, if present) exists,
  // rebuild m_ChildrenIndex so parent UUIDs referencing entities that appeared
  // later in the file resolve correctly.
  m_Scene->RebuildHierarchyIndex();
}

void SerialiseVec2(std::ostream &out, const glm::vec2 &v) {
  out.write(reinterpret_cast<const char *>(&v.x), sizeof(v.x));
  out.write(reinterpret_cast<const char *>(&v.y), sizeof(v.y));
}

glm::vec2 DeserialiseVec2(std::istream &in) {
  glm::vec2 v;

  in.read(reinterpret_cast<char *>(&v.x), sizeof(v.x));
  in.read(reinterpret_cast<char *>(&v.y), sizeof(v.y));

  return v;
}

void SerialiseVec3(std::ostream &out, const glm::vec3 &v) {
  out.write(reinterpret_cast<const char *>(&v.x), sizeof(v.x));
  out.write(reinterpret_cast<const char *>(&v.y), sizeof(v.y));
  out.write(reinterpret_cast<const char *>(&v.z), sizeof(v.z));
}

glm::vec3 DeserialiseVec3(std::istream &in) {
  glm::vec3 v;

  in.read(reinterpret_cast<char *>(&v.x), sizeof(v.x));
  in.read(reinterpret_cast<char *>(&v.y), sizeof(v.y));
  in.read(reinterpret_cast<char *>(&v.z), sizeof(v.z));

  return v;
}

void SerialiseVec4(std::ostream &out, const glm::vec4 &v) {
  out.write(reinterpret_cast<const char *>(&v.x), sizeof(v.x));
  out.write(reinterpret_cast<const char *>(&v.y), sizeof(v.y));
  out.write(reinterpret_cast<const char *>(&v.z), sizeof(v.z));
  out.write(reinterpret_cast<const char *>(&v.w), sizeof(v.w));
}

glm::vec4 DeserialiseVec4(std::istream &in) {
  glm::vec4 v;
  in.read(reinterpret_cast<char *>(&v.x), sizeof(v.x));
  in.read(reinterpret_cast<char *>(&v.y), sizeof(v.y));
  in.read(reinterpret_cast<char *>(&v.z), sizeof(v.z));
  in.read(reinterpret_cast<char *>(&v.w), sizeof(v.w));
  return v;
}

void SerialiseString(std::ostream &out, const std::string &s) {
  std::size_t len = s.size();
  out.write(reinterpret_cast<const char *>(&len), sizeof(len));
  out.write(s.data(), len);
}

std::string DeserialiseString(std::istream &in) {
  std::size_t len;
  in.read(reinterpret_cast<char *>(&len), sizeof(len));
  std::string s(len, '\0');
  in.read(s.data(), len);
  return s;
}

void SceneSerialiser::SerialiseEntity(std::ostream &out,
                                      entt::entity const &entity,
                                      UUID const &entity_uuid) {
  std::cout << "Serialising entity with UUID: " << entity_uuid.GetUUID()
            << std::endl;

  UUID::Serialise(out, entity_uuid);

  if (m_Scene->EntityHasComponent<Transform>(entity_uuid)) {
    int id = static_cast<int>(Transform_ID);

    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    Transform &transform = m_Scene->GetEntityComponent<Transform>(entity_uuid);

    SerialiseVec3(out, transform.position);

    out.write(reinterpret_cast<const char *>(&transform.rotation),
              sizeof(transform.rotation));

    SerialiseVec2(out, transform.size);
  }

  if (m_Scene->EntityHasComponent<Sprite>(entity_uuid)) {
    int id = static_cast<int>(Sprite_ID);

    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    Sprite &sprite = m_Scene->GetEntityComponent<Sprite>(entity_uuid);

    // Prefer the explicit assetUUID (set by the editor + sub-sprite drag
    // path), falling back to the parent texture's own UUID for sprites
    // assigned the pre-spritesheet way. The deserialiser routes either
    // kind of UUID through AssetManager::ResolveSpriteSource so the
    // resulting (texture, uvRect) is correct for both.
    UUID toWrite = UUID::GetNil();
    if (!UUID::IsNil(sprite.assetUUID)) {
      toWrite = sprite.assetUUID;
    } else if (sprite.texture != nullptr) {
      toWrite = sprite.texture->GetUUID();
    }
    UUID::Serialise(out, toWrite);

    SerialiseVec3(out, sprite.colour);
  }

  if (m_Scene->EntityHasComponent<Name>(entity_uuid)) {
    int id = static_cast<int>(Name_ID);
    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    Name &name = m_Scene->GetEntityComponent<Name>(entity_uuid);

    std::size_t nameLength = name.name.size();
    out.write(reinterpret_cast<const char *>(&nameLength), sizeof(nameLength));
    out.write(reinterpret_cast<const char *>(name.name.data()), nameLength);
  }

  if (m_Scene->EntityHasComponent<Rigidbody>(entity_uuid)) {
    int id = static_cast<int>(Rigidbody_ID);
    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    Rigidbody &rb = m_Scene->GetEntityComponent<Rigidbody>(entity_uuid);

    int bodyType = static_cast<int>(rb.bodyType);
    int colliderShape = static_cast<int>(rb.colliderShape);
    out.write(reinterpret_cast<const char *>(&bodyType), sizeof(bodyType));
    out.write(reinterpret_cast<const char *>(&colliderShape), sizeof(colliderShape));
    out.write(reinterpret_cast<const char *>(&rb.density), sizeof(rb.density));
    out.write(reinterpret_cast<const char *>(&rb.friction), sizeof(rb.friction));
    out.write(reinterpret_cast<const char *>(&rb.restitution), sizeof(rb.restitution));
    out.write(reinterpret_cast<const char *>(&rb.gravityScale), sizeof(rb.gravityScale));

    int colliderId = static_cast<int>(Collider_ID);
    out.write(reinterpret_cast<const char *>(&colliderId), sizeof(colliderId));
    SerialiseVec2(out, rb.colliderOffset);
    SerialiseVec2(out, rb.colliderSize);
  }

  if (m_Scene->EntityHasComponent<Animation>(entity_uuid)) {
    int id = static_cast<int>(Animation_ID);
    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    Animation &anim = m_Scene->GetEntityComponent<Animation>(entity_uuid);

    uint32_t animCount = static_cast<uint32_t>(anim.animations.size());
    out.write(reinterpret_cast<const char *>(&animCount), sizeof(animCount));

    for (auto const &[name, uuid] : anim.animations) {
      std::size_t nameLen = name.size();
      out.write(reinterpret_cast<const char *>(&nameLen), sizeof(nameLen));
      out.write(name.data(), nameLen);
      UUID::Serialise(out, uuid);
    }

    std::size_t defaultLen = anim.defaultAnimation.size();
    out.write(reinterpret_cast<const char *>(&defaultLen), sizeof(defaultLen));
    out.write(anim.defaultAnimation.data(), defaultLen);

    uint8_t loopByte = anim.loop ? 1 : 0;
    out.write(reinterpret_cast<const char *>(&loopByte), sizeof(loopByte));
  }

  if (m_Scene->EntityHasComponent<Hierarchy>(entity_uuid)) {
    int id = static_cast<int>(Hierarchy_ID);
    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    Hierarchy &h = m_Scene->GetEntityComponent<Hierarchy>(entity_uuid);
    UUID::Serialise(out, h.parent);
    out.write(reinterpret_cast<const char *>(&h.siblingIndex),
              sizeof(h.siblingIndex));
  }

  if (m_Scene->EntityHasComponent<UIButton>(entity_uuid)) {
    int id = static_cast<int>(UIButton_ID);
    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    UIButton &btn = m_Scene->GetEntityComponent<UIButton>(entity_uuid);

    uint8_t anchor = static_cast<uint8_t>(btn.anchor);
    uint8_t align = static_cast<uint8_t>(btn.textAlign);
    uint8_t autoSize = btn.autoSize ? 1 : 0;
    out.write(reinterpret_cast<const char *>(&anchor), sizeof(anchor));
    SerialiseVec2(out, btn.offset);
    SerialiseVec2(out, btn.size);
    out.write(reinterpret_cast<const char *>(&autoSize), sizeof(autoSize));
    out.write(reinterpret_cast<const char *>(&btn.padding), sizeof(btn.padding));
    SerialiseVec4(out, btn.bgColour);
    SerialiseString(out, btn.label);
    SerialiseVec4(out, btn.textColour);
    out.write(reinterpret_cast<const char *>(&btn.fontSize), sizeof(btn.fontSize));
    out.write(reinterpret_cast<const char *>(&align), sizeof(align));
    uint8_t bold = btn.bold ? 1 : 0;
    out.write(reinterpret_cast<const char *>(&bold), sizeof(bold));
    UUID::Serialise(out, btn.imageUUID);
  }

  if (m_Scene->EntityHasComponent<UIText>(entity_uuid)) {
    int id = static_cast<int>(UIText_ID);
    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    UIText &txt = m_Scene->GetEntityComponent<UIText>(entity_uuid);

    uint8_t anchor = static_cast<uint8_t>(txt.anchor);
    out.write(reinterpret_cast<const char *>(&anchor), sizeof(anchor));
    SerialiseVec2(out, txt.offset);
    SerialiseString(out, txt.text);
    SerialiseVec4(out, txt.textColour);
    out.write(reinterpret_cast<const char *>(&txt.fontSize), sizeof(txt.fontSize));
    out.write(reinterpret_cast<const char *>(&txt.wrapWidth), sizeof(txt.wrapWidth));
    uint8_t bold = txt.bold ? 1 : 0;
    out.write(reinterpret_cast<const char *>(&bold), sizeof(bold));
  }

  if (m_Scene->EntityHasComponent<Behaviour>(entity_uuid)) {
    int id = static_cast<int>(Behaviour_ID);

    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    Behaviour &behaviour = m_Scene->GetEntityComponent<Behaviour>(entity_uuid);

    std::uint32_t scriptCount = static_cast<uint32_t>(behaviour.scripts.size());
    out.write(reinterpret_cast<const char *>(&scriptCount),
              sizeof(scriptCount));

    for (auto const &[uuid, script] : behaviour.scripts) {
      UUID::Serialise(out, uuid);

      // Refresh the cached name from the live script if possible, else
      // fall back to the cachedNames entry (preserves last-known name
      // even when the script went missing between load and save).
      std::string name;
      if (script) {
        name = script->GetName();
      } else {
        auto it = behaviour.cachedNames.find(uuid);
        if (it != behaviour.cachedNames.end()) name = it->second;
      }
      std::size_t nameLen = name.size();
      out.write(reinterpret_cast<const char *>(&nameLen), sizeof(nameLen));
      out.write(name.data(), nameLen);
    }
  }

  int endId = -1;

  out.write(reinterpret_cast<const char *>(&endId), sizeof(endId));
}

UUID SceneSerialiser::DeserialiseEntity(std::istream &in) {
  UUID uuid = UUID::Deserialise(in);

  std::cout << "Deserialising entity with uuid: " << uuid.GetUUIDString()
            << std::endl;

  m_Scene->CreateEntityWithUUID(uuid);

  int componentId;

  while (in.read(reinterpret_cast<char *>(&componentId), sizeof(componentId))) {
    std::cout << "Deserialising component id: " << componentId << std::endl;

    switch (componentId) {
    case Transform_ID: {
      glm::vec3 pos = DeserialiseVec3(in);

      float rotation;

      in.read(reinterpret_cast<char *>(&rotation), sizeof(rotation));

      glm::vec2 size = DeserialiseVec2(in);

      m_Scene->AddEntityComponent<Transform>(uuid, pos, rotation, size);

      break;
    }
    case Sprite_ID: {
      UUID assetUUID = UUID::Deserialise(in);

      std::cout << assetUUID.GetUUIDString() << std::endl;

      glm::vec3 colour = DeserialiseVec3(in);

      if (!UUID::IsNil(assetUUID)) {
        // Route through ResolveSpriteSource so the UUID can be either a
        // Texture OR a SubSprite — sub-sprite resolution returns the
        // parent texture pointer, which is what Sprite caches. The UV
        // rect is recomputed at render time from assetUUID.
        SpriteSource src = m_AssetManager->ResolveSpriteSource(assetUUID);
        std::shared_ptr<Texture> texPtr;
        if (src.texture != nullptr && !src.missing) {
          // Find the shared_ptr that owns src.texture so Sprite's
          // texture member shares ownership with AssetManager.
          for (auto const &[texUUID, tex] : m_AssetManager->GetTextureMap()) {
            if (tex.get() == src.texture) {
              texPtr = tex;
              break;
            }
          }
        }
        m_Scene->AddEntityComponent<Sprite>(uuid, texPtr, colour);
        m_Scene->GetEntityComponent<Sprite>(uuid).assetUUID = assetUUID;
      } else {
        m_Scene->AddEntityComponent<Sprite>(uuid, colour);
      }

      break;
    }
    case Name_ID: {
      std::size_t nameLength;
      in.read(reinterpret_cast<char *>(&nameLength), sizeof(nameLength));

      std::string name(nameLength, '\0');
      in.read(reinterpret_cast<char *>(name.data()), nameLength);

      m_Scene->AddEntityComponent<Name>(uuid, name);

      break;
    }
    case Rigidbody_ID: {
      int bodyType, colliderShape;
      float density, friction, restitution, gravityScale;
      in.read(reinterpret_cast<char *>(&bodyType), sizeof(bodyType));
      in.read(reinterpret_cast<char *>(&colliderShape), sizeof(colliderShape));
      in.read(reinterpret_cast<char *>(&density), sizeof(density));
      in.read(reinterpret_cast<char *>(&friction), sizeof(friction));
      in.read(reinterpret_cast<char *>(&restitution), sizeof(restitution));
      in.read(reinterpret_cast<char *>(&gravityScale), sizeof(gravityScale));

      Rigidbody rb;
      rb.bodyType = static_cast<BodyType>(bodyType);
      rb.colliderShape = static_cast<ColliderShape>(colliderShape);
      rb.density = density;
      rb.friction = friction;
      rb.restitution = restitution;
      rb.gravityScale = gravityScale;

      m_Scene->AddEntityComponent<Rigidbody>(uuid, rb);

      break;
    }
    case Behaviour_ID: {
      uint32_t scriptCount;
      in.read(reinterpret_cast<char *>(&scriptCount), sizeof(scriptCount));

      m_Scene->AddEntityComponent<Behaviour>(uuid);

      Behaviour &behaviour = m_Scene->GetEntityComponent<Behaviour>(uuid);

      for (uint32_t i = 0; i < scriptCount; i++) {
        UUID scriptUUID = UUID::Deserialise(in);

        std::size_t nameLen;
        in.read(reinterpret_cast<char *>(&nameLen), sizeof(nameLen));
        std::string cachedName(nameLen, '\0');
        in.read(cachedName.data(), nameLen);

        // Missing-tolerant: GetScript no longer throws — it returns a
        // shared_ptr that's nullptr when the UUID has no live script. The
        // cached name is kept so the property editor can show MISSING with
        // the script's last-known display name.
        auto script = m_AssetManager->GetScript(scriptUUID);
        behaviour.scripts.emplace(scriptUUID, script);
        behaviour.cachedNames.emplace(scriptUUID, cachedName);
      }

      break;
    }
    case Animation_ID: {
      uint32_t animCount;
      in.read(reinterpret_cast<char *>(&animCount), sizeof(animCount));

      Animation anim;
      for (uint32_t i = 0; i < animCount; i++) {
        std::size_t nameLen;
        in.read(reinterpret_cast<char *>(&nameLen), sizeof(nameLen));
        std::string name(nameLen, '\0');
        in.read(name.data(), nameLen);

        UUID animUUID = UUID::Deserialise(in);
        anim.animations[name] = animUUID;
      }

      std::size_t defaultLen;
      in.read(reinterpret_cast<char *>(&defaultLen), sizeof(defaultLen));
      std::string defaultAnim(defaultLen, '\0');
      in.read(defaultAnim.data(), defaultLen);
      anim.defaultAnimation = defaultAnim;

      uint8_t loopByte;
      in.read(reinterpret_cast<char *>(&loopByte), sizeof(loopByte));
      anim.loop = (loopByte != 0);

      m_Scene->AddEntityComponent<Animation>(uuid, anim);

      break;
    }
    case Collider_ID: {
      glm::vec2 offset = DeserialiseVec2(in);
      glm::vec2 size = DeserialiseVec2(in);

      if (m_Scene->EntityHasComponent<Rigidbody>(uuid)) {
        Rigidbody &rb = m_Scene->GetEntityComponent<Rigidbody>(uuid);
        rb.colliderOffset = offset;
        rb.colliderSize = size;
      }

      break;
    }
    case Hierarchy_ID: {
      UUID parent = UUID::Deserialise(in);
      uint32_t siblingIndex;
      in.read(reinterpret_cast<char *>(&siblingIndex), sizeof(siblingIndex));

      // CreateEntityWithUUID already inserted a default Hierarchy at the root
      // — overwrite with the persisted parent/index here. Index rebuild happens
      // post-load when SceneSerialiser::Deserialise calls FinaliseHierarchy().
      if (m_Scene->EntityHasComponent<Hierarchy>(uuid)) {
        Hierarchy &h = m_Scene->GetEntityComponent<Hierarchy>(uuid);
        h.parent = parent;
        h.siblingIndex = siblingIndex;
      }

      break;
    }
    case UIButton_ID: {
      UIButton btn;
      uint8_t anchor;
      in.read(reinterpret_cast<char *>(&anchor), sizeof(anchor));
      btn.anchor = static_cast<UIAnchor>(anchor);
      btn.offset = DeserialiseVec2(in);
      btn.size = DeserialiseVec2(in);
      uint8_t autoSize;
      in.read(reinterpret_cast<char *>(&autoSize), sizeof(autoSize));
      btn.autoSize = (autoSize != 0);
      in.read(reinterpret_cast<char *>(&btn.padding), sizeof(btn.padding));
      btn.bgColour = DeserialiseVec4(in);
      btn.label = DeserialiseString(in);
      btn.textColour = DeserialiseVec4(in);
      in.read(reinterpret_cast<char *>(&btn.fontSize), sizeof(btn.fontSize));
      uint8_t align;
      in.read(reinterpret_cast<char *>(&align), sizeof(align));
      btn.textAlign = static_cast<UITextAlign>(align);
      uint8_t bold;
      in.read(reinterpret_cast<char *>(&bold), sizeof(bold));
      btn.bold = (bold != 0);
      btn.imageUUID = UUID::Deserialise(in);

      m_Scene->AddEntityComponent<UIButton>(uuid, btn);
      break;
    }
    case UIText_ID: {
      UIText txt;
      uint8_t anchor;
      in.read(reinterpret_cast<char *>(&anchor), sizeof(anchor));
      txt.anchor = static_cast<UIAnchor>(anchor);
      txt.offset = DeserialiseVec2(in);
      txt.text = DeserialiseString(in);
      txt.textColour = DeserialiseVec4(in);
      in.read(reinterpret_cast<char *>(&txt.fontSize), sizeof(txt.fontSize));
      in.read(reinterpret_cast<char *>(&txt.wrapWidth), sizeof(txt.wrapWidth));
      uint8_t bold;
      in.read(reinterpret_cast<char *>(&bold), sizeof(bold));
      txt.bold = (bold != 0);

      m_Scene->AddEntityComponent<UIText>(uuid, txt);
      break;
    }
    case -1: {
      return uuid;
    }
    default: {
      std::cout << componentId << std::endl;

      throw std::runtime_error("Unrecognized scene serialisation type");
    }
    }
  }

  return uuid;
}
} // namespace Hamster
