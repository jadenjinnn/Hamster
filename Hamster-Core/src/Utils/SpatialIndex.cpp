#include "HamsterPCH.h"

#include "SpatialIndex.h"

#include <algorithm>
#include <limits>

namespace Hamster {

SpatialIndex::SpatialIndex() = default;

void SpatialIndex::Rebuild(
    const std::vector<std::pair<UUID, AABB>> &entries) {
  m_Root.reset();
  if (entries.empty()) return;

  // Root bounds = union of all entry AABBs. Empty scene already returned.
  AABB root = entries.front().second;
  for (size_t i = 1; i < entries.size(); ++i) {
    const AABB &b = entries[i].second;
    root.min.x = std::min(root.min.x, b.min.x);
    root.min.y = std::min(root.min.y, b.min.y);
    root.max.x = std::max(root.max.x, b.max.x);
    root.max.y = std::max(root.max.y, b.max.y);
  }

  m_Root = std::make_unique<Node>();
  m_Root->bounds = root;

  for (const auto &entry : entries) {
    Insert(m_Root.get(), entry, 0);
  }
}

void SpatialIndex::Insert(Node *node, const std::pair<UUID, AABB> &entry,
                          int depth) {
  if (node->IsLeaf()) {
    if (node->entries.size() < kBucketCap || depth >= kMaxDepth) {
      node->entries.push_back(entry);
      return;
    }
    Split(node, depth);
  }

  // Internal node — try to push entry down to a fully-containing child.
  // Straddlers stay at this node.
  for (int i = 0; i < 4; ++i) {
    if (node->children[i]->bounds.Contains(entry.second)) {
      Insert(node->children[i].get(), entry, depth + 1);
      return;
    }
  }
  node->entries.push_back(entry);
}

void SpatialIndex::Split(Node *node, int depth) {
  // Move existing entries aside, create children, redistribute.
  auto pending = std::move(node->entries);
  node->entries.clear();
  for (int i = 0; i < 4; ++i) {
    node->children[i] = std::make_unique<Node>();
    node->children[i]->bounds = ChildBounds(node->bounds, i);
  }
  for (const auto &entry : pending) {
    bool placed = false;
    for (int i = 0; i < 4; ++i) {
      if (node->children[i]->bounds.Contains(entry.second)) {
        Insert(node->children[i].get(), entry, depth + 1);
        placed = true;
        break;
      }
    }
    if (!placed) node->entries.push_back(entry); // straddler
  }
}

AABB SpatialIndex::ChildBounds(const AABB &parent, int childIdx) {
  const glm::vec2 center = (parent.min + parent.max) * 0.5f;
  // 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right
  // (engine y-down, but the math is symmetric — names are by index).
  switch (childIdx) {
  case 0: return {parent.min, center};
  case 1: return {{center.x, parent.min.y}, {parent.max.x, center.y}};
  case 2: return {{parent.min.x, center.y}, {center.x, parent.max.y}};
  case 3: return {center, parent.max};
  }
  return parent;
}

UUID SpatialIndex::QueryPoint(
    glm::vec2 worldPoint,
    const std::function<float(UUID)> &zOf) const {
  if (!m_Root) return UUID::GetNil();

  std::vector<std::pair<UUID, AABB>> candidates;
  QueryPointWalk(m_Root.get(), worldPoint, candidates);

  if (candidates.empty()) return UUID::GetNil();

  // Topmost wins. Stable selection: tie → first encountered (which matches
  // the existing FBO-based picking semantics when EnTT iteration order is
  // the tie-breaker).
  UUID best = candidates.front().first;
  float bestZ = zOf(best);
  for (size_t i = 1; i < candidates.size(); ++i) {
    float z = zOf(candidates[i].first);
    if (z > bestZ) {
      best = candidates[i].first;
      bestZ = z;
    }
  }
  return best;
}

void SpatialIndex::QueryPointWalk(
    const Node *node, glm::vec2 p,
    std::vector<std::pair<UUID, AABB>> &out) {
  if (!node || !node->bounds.ContainsPoint(p)) return;

  for (const auto &entry : node->entries) {
    if (entry.second.ContainsPoint(p)) out.push_back(entry);
  }
  if (!node->IsLeaf()) {
    for (int i = 0; i < 4; ++i) {
      QueryPointWalk(node->children[i].get(), p, out);
    }
  }
}

std::vector<UUID> SpatialIndex::QueryRect(const AABB &rect) const {
  std::vector<UUID> out;
  if (!m_Root) return out;
  QueryRectWalk(m_Root.get(), rect, out);
  return out;
}

void SpatialIndex::QueryRectWalk(const Node *node, const AABB &rect,
                                 std::vector<UUID> &out) {
  if (!node || !node->bounds.Intersects(rect)) return;

  for (const auto &entry : node->entries) {
    if (entry.second.Intersects(rect)) out.push_back(entry.first);
  }
  if (!node->IsLeaf()) {
    for (int i = 0; i < 4; ++i) {
      QueryRectWalk(node->children[i].get(), rect, out);
    }
  }
}

uint32_t SpatialIndex::Size() const {
  if (!m_Root) return 0;
  return SizeWalk(m_Root.get());
}

uint32_t SpatialIndex::SizeWalk(const Node *node) {
  if (!node) return 0;
  uint32_t total = static_cast<uint32_t>(node->entries.size());
  if (!node->IsLeaf()) {
    for (int i = 0; i < 4; ++i) {
      total += SizeWalk(node->children[i].get());
    }
  }
  return total;
}

} // namespace Hamster
