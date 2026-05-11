#pragma once

// Shorthand for binding all virtual methods of the Event class, GetEventName
// should only be used for debugging and not in production
#define BIND_EVENT_TYPE(type)                                                  \
  static EventType GetStaticEventType() { return EventType::type; }            \
  virtual EventType GetEventType() const override {                            \
    return GetStaticEventType();                                               \
  }                                                                            \
  virtual std::string GetEventName() const override { return #type; }

// Shorthand for forhanding a callback to an Event class
#define FORWARD_CALLBACK_FUNCTION(func, eventClass)                            \
  [this](Hamster::Event &e) { this->func(dynamic_cast<eventClass &>(e)); }

#define FORWARD_STATIC_CALLBACK_FUNCTION(func, eventClass)                     \
  [](Hamster::Event &e) { func(dynamic_cast<eventClass &>(e)); }

// Definitions for the colour of the x and y guizmo used in the editor, the
// largest and second largest possible entity handle to avoid collisions
#define XGuizmoID 16777214
#define YGuizmoID 16777213
#define TopLeftGrabberID 16777212
#define TopRightGrabberID 16777211
#define BottomLeftGrabberID 16777210
#define BottomRightGrabberID 16777209
#define TopGrabberID 16777208
#define RightGrabberID 16777207
#define BottomGrabberID 16777206
#define LeftGrabberID 16777205
#define GrabberGuizmoID 16777200
