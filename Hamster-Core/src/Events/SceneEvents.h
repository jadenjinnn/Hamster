//
// Created by Jaden on 31/08/2024.
//

#ifndef SCENEEVENTS_H
#define SCENEEVENTS_H

#include "Event.h"
#include "Core/UUID.h"

namespace Hamster {
  class CollisionEvent : public Event {
  public:
    CollisionEvent(UUID& uuidA, UUID& uuidB) : uuidA(uuidA), uuidB(uuidB) {};

    [[nodiscard]] UUID& GetUUIDA() const {return uuidA;}
    [[nodiscard]] UUID& GetUUIDB() const {return uuidB;}

    BIND_EVENT_TYPE(Collision);
  private:
    UUID& uuidA;
    UUID& uuidB;
  };
  class AnimationCompletedEvent : public Event {
  public:
    AnimationCompletedEvent(UUID entityUUID, std::string animationName)
        : m_EntityUUID(entityUUID), m_AnimationName(std::move(animationName)) {}

    [[nodiscard]] UUID GetEntityUUID() const { return m_EntityUUID; }
    [[nodiscard]] const std::string &GetAnimationName() const { return m_AnimationName; }

    BIND_EVENT_TYPE(AnimationCompleted);
  private:
    UUID m_EntityUUID;
    std::string m_AnimationName;
  };
} // namespace Hamster

#endif // SCENEEVENTS_H
