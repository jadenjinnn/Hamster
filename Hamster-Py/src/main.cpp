#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Components.h"
#include "Core.h"
#include "HamsterBehaviour.h"
#include "Input.h"
#include "Library.h"
#include "Log.h"
#include "UUID.h"

PYBIND11_MODULE(Hamster, m) {
  Vec2Binding(m);
  Vec3Binding(m);
  UUIDBinding(m);
  TransformBinding(m);
  HamsterBehaviourBinding(m);
  ScenePtrBinding(m);
  AppInstanceBinding(m);
  KeyCodeEnumBinding(m);
  LoggingEnumBinding(m);
}
