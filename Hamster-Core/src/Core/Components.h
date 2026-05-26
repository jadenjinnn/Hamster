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
  Animation_ID = 8,
  Hierarchy_ID = 9,
  UIButton_ID = 10,
  UIText_ID = 11
};

// Screen-space anchor for UI elements. The anchor names a single point on the
// viewport rectangle (e.g. TopLeft = (0,0), BottomRight = (vw,vh)). Combined
// with `offset`, the matching pivot of the UI rect sits at anchorPos +
// signedOffset, where signedOffset flips +x for right-column anchors and +y
// for bottom-row anchors so "+offset always moves toward the screen centre".
enum class UIAnchor : uint8_t {
  TopLeft,    TopCentre,    TopRight,
  MiddleLeft, Centre,       MiddleRight,
  BottomLeft, BottomCentre, BottomRight,
};

enum class UITextAlign : uint8_t { Left, Centre, Right };

// Transform component holding all needed transforms, ntities wanting to be
// rendered must have a transform, all entites on default have it
struct Transform {
  glm::vec3 position = glm::vec3(0.0f);
  float rotation = 0.0f;
  glm::vec2 size = glm::vec2(100.0f);
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
  // The asset UUID this sprite renders. When non-nil it is the source of
  // truth; the renderer calls AssetManager::ResolveSpriteSource(assetUUID)
  // to get (texture, uvRect) and applies the UV. Sub-sprite UUIDs resolve
  // to (parent texture, sub-rect); regular texture UUIDs resolve to
  // (texture, full UV). When nil, fall back to `texture` directly for
  // backward compatibility with scenes serialised before the spritesheet
  // feature.
  //
  // Default = nil so sprites created via the (tex, col) ctor (which
  // doesn't know about asset UUIDs) take the fallback path and behave
  // exactly as they did pre-spritesheet. The default UUID ctor generates
  // a fresh random UUID, which is why we initialise explicitly here.
  UUID assetUUID = UUID::GetNil();
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
  glm::vec2 pendingPosition = glm::vec2(0.0f); // body-centre teleport, pixels
  bool hasPendingPosition = false;
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

// Parent/child organisational relationship. parent == nil means top-level.
// siblingIndex is the entity's order among entities sharing the same parent.
// Pure organisational — no transform inheritance. See entity-hierarchy feature spec.
struct Hierarchy {
  UUID parent = UUID::GetNil();
  uint32_t siblingIndex = 0;
};

// Screen-space clickable rect with a solid background colour and a text label.
// Lives on an entity alongside Name/ID/Hierarchy; does NOT use the Transform
// component — UI elements anchor independently of world coords. Text label
// rendering arrives in Phase B (the fields persist for forward-compat).
struct UIButton {
  UIAnchor anchor = UIAnchor::TopLeft;
  glm::vec2 offset = glm::vec2(10.0f, 10.0f);
  glm::vec2 size = glm::vec2(200.0f, 50.0f);
  bool autoSize = false;
  float padding = 8.0f;
  glm::vec4 bgColour = glm::vec4(0.2f, 0.4f, 0.8f, 1.0f);
  std::string label = "Button";
  glm::vec4 textColour = glm::vec4(1.0f);
  float fontSize = 18.0f;
  UITextAlign textAlign = UITextAlign::Centre;
  bool bold = false;
  // Optional background image. When non-nil, the renderer resolves it via
  // AssetManager::ResolveSpriteSource (texture or sub-sprite) and draws it
  // filling the button rect, between the bgColour rect and the label.
  UUID imageUUID = UUID::GetNil();
  // Runtime-only show/hide toggle (NOT serialised — always true at load).
  // Scripts flip it during play via EntityHandle::set_visible; the snapshot
  // restore on stop resets it to true. When false the renderer skips it and
  // the play-mode hit-tests treat it as non-clickable.
  bool visible = true;
};

// Screen-space text label, no background, not clickable. wrapWidth == 0 means
// single-line / no wrap. Rendered in Phase B.
struct UIText {
  UIAnchor anchor = UIAnchor::TopLeft;
  glm::vec2 offset = glm::vec2(10.0f, 10.0f);
  std::string text = "Text";
  glm::vec4 textColour = glm::vec4(1.0f);
  float fontSize = 18.0f;
  float wrapWidth = 0.0f;
  bool bold = false;
  // Runtime-only show/hide toggle (NOT serialised). See UIButton::visible.
  bool visible = true;
};

// Screen-space rect: top-left + size. Returned by ResolveUIButtonRect so
// renderer and hit-test agree on geometry.
struct UIRect {
  float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
  bool ContainsPoint(float px, float py) const {
    return px >= x && px <= x + w && py >= y && py <= y + h;
  }
};

// Resolve an anchored box's top-left point. The anchor names a screen
// point; the matching corner of the box sits at anchor + signedOffset where
// signedOffset flips +x for right-column anchors and +y for bottom-row
// anchors ("+offset always moves inward toward the screen centre").
inline glm::vec2 ResolveAnchoredTopLeft(UIAnchor anchor, glm::vec2 offset,
                                         glm::vec2 size, float vw, float vh) {
  float ax = 0.0f, ay = 0.0f;
  switch (anchor) {
    case UIAnchor::TopLeft:      ax = 0.0f;       ay = 0.0f;       break;
    case UIAnchor::TopCentre:    ax = vw * 0.5f;  ay = 0.0f;       break;
    case UIAnchor::TopRight:     ax = vw;         ay = 0.0f;       break;
    case UIAnchor::MiddleLeft:   ax = 0.0f;       ay = vh * 0.5f;  break;
    case UIAnchor::Centre:       ax = vw * 0.5f;  ay = vh * 0.5f;  break;
    case UIAnchor::MiddleRight:  ax = vw;         ay = vh * 0.5f;  break;
    case UIAnchor::BottomLeft:   ax = 0.0f;       ay = vh;         break;
    case UIAnchor::BottomCentre: ax = vw * 0.5f;  ay = vh;         break;
    case UIAnchor::BottomRight:  ax = vw;         ay = vh;         break;
  }
  float px = 0.0f, py = 0.0f;
  switch (anchor) {
    case UIAnchor::TopLeft:      px = 0.0f;          py = 0.0f;          break;
    case UIAnchor::TopCentre:    px = size.x * 0.5f; py = 0.0f;          break;
    case UIAnchor::TopRight:     px = size.x;        py = 0.0f;          break;
    case UIAnchor::MiddleLeft:   px = 0.0f;          py = size.y * 0.5f; break;
    case UIAnchor::Centre:       px = size.x * 0.5f; py = size.y * 0.5f; break;
    case UIAnchor::MiddleRight:  px = size.x;        py = size.y * 0.5f; break;
    case UIAnchor::BottomLeft:   px = 0.0f;          py = size.y;        break;
    case UIAnchor::BottomCentre: px = size.x * 0.5f; py = size.y;        break;
    case UIAnchor::BottomRight:  px = size.x;        py = size.y;        break;
  }
  float xSign = (anchor == UIAnchor::TopRight ||
                 anchor == UIAnchor::MiddleRight ||
                 anchor == UIAnchor::BottomRight) ? -1.0f : 1.0f;
  float ySign = (anchor == UIAnchor::BottomLeft ||
                 anchor == UIAnchor::BottomCentre ||
                 anchor == UIAnchor::BottomRight) ? -1.0f : 1.0f;
  return {ax - px + offset.x * xSign, ay - py + offset.y * ySign};
}

// Convenience wrapper for UIButton. autoSize is handled by the renderer
// via Renderer::ResolveUIButton (it needs the FontAtlas to measure label
// width); when autoSize is false the result is identical to calling
// Renderer::ResolveUIButton.
inline UIRect ResolveUIButtonRect(const UIButton &b, float vw, float vh) {
  glm::vec2 tl = ResolveAnchoredTopLeft(b.anchor, b.offset, b.size, vw, vh);
  UIRect r;
  r.x = tl.x;
  r.y = tl.y;
  r.w = b.size.x;
  r.h = b.size.y;
  return r;
}

// Stores a map of scripts and a vector of classes that derive HamsterBehaviour
struct Behaviour {
  std::unordered_map<UUID, std::shared_ptr<HamsterScript>>
      scripts; // A map was used for ease of removal of scripts

  // Last-known display name per attached script UUID. Written on every scene
  // save (from the live HamsterScript) and persisted so the property editor
  // can show "MISSING — <cachedName>" when a UUID no longer resolves to a
  // loaded script (e.g., the .py was renamed externally and the sidecar
  // didn't follow). See the asset-sidecars feature.
  std::unordered_map<UUID, std::string> cachedNames;

  std::vector<pybind11::object> pyObjects;
};
} // namespace Hamster

#endif // COMPONENTS_H
