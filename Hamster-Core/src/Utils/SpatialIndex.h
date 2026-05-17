#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Core/UUID.h"

namespace Hamster {

struct AABB {
  glm::vec2 min{0.0f, 0.0f};
  glm::vec2 max{0.0f, 0.0f};

  bool ContainsPoint(glm::vec2 p) const {
    return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
  }

  bool Intersects(const AABB &other) const {
    return !(other.max.x < min.x || other.min.x > max.x ||
             other.max.y < min.y || other.min.y > max.y);
  }

  bool Contains(const AABB &other) const {
    return other.min.x >= min.x && other.max.x <= max.x &&
           other.min.y >= min.y && other.max.y <= max.y;
  }
};

// Quadtree index over (UUID, AABB) pairs. Rebuilt from scratch each frame
// from the current Transform state — no incremental updates in v1.
// Consumers: Scene::OnRender (viewport-rect culling), EditorLayer (cursor
// point picking).
class SpatialIndex {
public:
  SpatialIndex();

  // Wipe the existing tree and rebuild over the given entries. Root bounds
  // are computed from the union of all entry AABBs.
  void Rebuild(const std::vector<std::pair<UUID, AABB>> &entries);

  // Topmost entity (highest z per `zOf`; tie → first encountered) whose
  // AABB contains `worldPoint`. Returns UUID::GetNil() on no hit.
  UUID QueryPoint(glm::vec2 worldPoint,
                  const std::function<float(UUID)> &zOf) const;

  // All UUIDs whose AABB intersects `rect`. Order unspecified.
  std::vector<UUID> QueryRect(const AABB &rect) const;

  // Diagnostic: total number of stored entries across the tree. Useful for
  // tests asserting build correctness.
  uint32_t Size() const;

private:
  struct Node {
    AABB bounds;
    // Entries that straddle a child boundary (don't fit fully into any one
    // child) live here, on the internal node. Pure leaves also use this.
    std::vector<std::pair<UUID, AABB>> entries;
    std::unique_ptr<Node> children[4];

    bool IsLeaf() const { return !children[0]; }
  };

  static constexpr int kBucketCap = 8;
  static constexpr int kMaxDepth = 8;

  std::unique_ptr<Node> m_Root;

  static void Insert(Node *node, const std::pair<UUID, AABB> &entry, int depth);
  static void Split(Node *node, int depth);
  static AABB ChildBounds(const AABB &parent, int childIdx);
  static void QueryPointWalk(const Node *node, glm::vec2 p,
                             std::vector<std::pair<UUID, AABB>> &out);
  static void QueryRectWalk(const Node *node, const AABB &rect,
                            std::vector<UUID> &out);
  static uint32_t SizeWalk(const Node *node);
};

} // namespace Hamster
