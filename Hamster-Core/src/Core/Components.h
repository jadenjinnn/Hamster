//
// Created by Jaden on 26/08/2024.
//

#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <box2d/box2d.h>
#include <glm/glm.hpp>
#include <pybind11/pybind11.h>

#include "Renderer/Texture.h"
#include "Scripting/HamsterScript.h"

namespace Hamster {
// Component IDs used during serialisation and deserialisation to identify
// component
enum ComponentID {
  Transform_ID = 1,
  Sprite_ID = 2,
  Name_ID = 3,
  Rigidbody_ID = 4,
  ID_ID = 5,
  Behaviour_ID,
  Collider_ID = 7,
  Animation_ID = 8
};

// Transform component holding all needed transforms, ntities wanting to be
// rendered must have a transform, all entites on default have it
struct Transform {
  glm::vec3 position = glm::vec3(0.0f);
  float rotation = 0.0f;
  glm::vec2 size = glm::vec2(10.0f);
};

// Contains a shared_ptr to a texture stored in the asset manager, also contains
// a colour that is rendered over a sprite, all entities wanting to be rendered
// must have a Sprite
struct Sprite {
  Sprite(std::shared_ptr<Texture> tex, glm::vec3 col)
      : texture(tex), colour(col) {};

  Sprite(glm::vec3 col) : colour(col) {};

  Sprite() {};

  std::shared_ptr<Texture> texture = nullptr;
  glm::vec3 colour = glm::vec3(1.0f);
};

// All entities have a name component, used pureply for debugging and for user
// use in Hamster-Wheel
struct Name {
  std::string name = "Entity";
};

enum class BodyType { Static = 0, Dynamic = 1, Kinematic = 2 };
enum class ColliderShape { Box = 0, Circle = 1 };

struct Rigidbody {
  BodyType bodyType = BodyType::Static;
  ColliderShape colliderShape = ColliderShape::Box;
  float density = 1.0f;
  float friction = 0.3f;
  float restitution = 0.0f;
  float gravityScale = 1.0f;

  glm::vec2 colliderOffset = glm::vec2(0.0f);
  glm::vec2 colliderSize = glm::vec2(0.0f); // (0,0) means "use transform size"

  // Runtime only — not serialized
  b2BodyId bodyId = b2_nullBodyId;
  glm::vec2 pendingForce = glm::vec2(0.0f);
  glm::vec2 pendingImpulse = glm::vec2(0.0f);
  glm::vec2 pendingVelocity = glm::vec2(0.0f);
  bool hasPendingVelocity = false;
  glm::vec2 cachedVelocity = glm::vec2(0.0f);
};

// All entities must also have an ID which contains the UUID used to identify
// the entity
struct ID {
  UUID uuid;
};

struct AnimationKeyframe {
  float time;
  UUID textureUUID;
};

struct AnimationData {
  std::string name;
  std::vector<AnimationKeyframe> keyframes;
  float duration = 0.0f;
};

struct Animation {
  std::unordered_map<std::string, UUID> animations;
  std::string defaultAnimation;
  bool loop = true;

  // Runtime only — not serialized
  std::string currentAnimation;
  float currentTime = 0.0f;
  bool playing = false;
  bool runtimeLoop = true;
  std::shared_ptr<Texture> originalTexture = nullptr;
  std::vector<std::string> completedAnimations;
};

// Stores a map of scripts and a vector of classes that derive HamsterBehaviour
struct Behaviour {
  std::unordered_map<UUID, std::shared_ptr<HamsterScript>>
      scripts; // A map was used for ease of removal of scripts

  std::vector<pybind11::object> pyObjects;
};
} // namespace Hamster

#endif // COMPONENTS_H
