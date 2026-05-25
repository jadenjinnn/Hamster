#ifndef UIEVENTS_H
#define UIEVENTS_H

#include "Core/Base.h"
#include "Core/UUID.h"
#include "Event.h"

namespace Hamster {
class ButtonClickedEvent : public Event {
public:
  explicit ButtonClickedEvent(UUID entityId) : m_EntityId(entityId) {}

  [[nodiscard]] UUID GetEntityId() const { return m_EntityId; }

  BIND_EVENT_TYPE(ButtonClicked);

private:
  UUID m_EntityId;
};
} // namespace Hamster

#endif // UIEVENTS_H
