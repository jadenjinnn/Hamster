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

    if (sprite.texture != nullptr) {
      UUID::Serialise(out, sprite.texture->GetUUID());
    } else {
      UUID::Serialise(out, UUID::GetNil());
    }

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

  if (m_Scene->EntityHasComponent<Behaviour>(entity_uuid)) {
    int id = static_cast<int>(Behaviour_ID);

    out.write(reinterpret_cast<const char *>(&id), sizeof(id));

    Behaviour &behaviour = m_Scene->GetEntityComponent<Behaviour>(entity_uuid);

    std::uint32_t scriptCount = static_cast<uint32_t>(behaviour.scripts.size());
    out.write(reinterpret_cast<const char *>(&scriptCount),
              sizeof(scriptCount));

    for (auto const &[uuid, script] : behaviour.scripts) {
      UUID::Serialise(out, uuid);
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
      UUID textureUUID = UUID::Deserialise(in);

      std::cout << textureUUID.GetUUIDString() << std::endl;

      glm::vec3 colour = DeserialiseVec3(in);

      if (!UUID::IsNil(textureUUID)) {
        m_Scene->AddEntityComponent<Sprite>(
            uuid, m_AssetManager->GetTexture(textureUUID), colour);
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

        std::cout << "trying to add script with uuid of: "
                  << scriptUUID.GetUUIDString() << std::endl;

        behaviour.scripts.emplace(scriptUUID,
                                  m_AssetManager->GetScript(scriptUUID));
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
