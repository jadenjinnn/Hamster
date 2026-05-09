//
// Created by Jaden on 29/08/2024.
//

#ifndef SCENESERIALISER_H
#define SCENESERIALISER_H

#include "Scene.h"

namespace Hamster {

class AssetManager;

class SceneSerialiser {
public:
  explicit SceneSerialiser(std::shared_ptr<Scene> scene,
                           AssetManager *assetManager = nullptr);

  void Serialise(std::ostream &out);
  void Deserialise(std::istream &in);

  void SerialiseEntity(std::ostream &out, entt::entity const &entity,
                       UUID const &entity_uuid);
  UUID DeserialiseEntity(std::istream &in);

private:
  std::shared_ptr<Scene> m_Scene;
  AssetManager *m_AssetManager;
};

} // namespace Hamster

#endif // SCENESERIALISER_H
