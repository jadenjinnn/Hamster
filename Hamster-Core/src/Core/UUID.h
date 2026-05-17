//
// Created by Jaden on 28/08/2024.
//

#ifndef UUID_H
#define UUID_H

#include <boost/container_hash/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace Hamster {
class UUID {
public:
  UUID();

  UUID(boost::uuids::uuid uuid);

  UUID(std::string& uuid);

  const std::string GetUUIDString() { return boost::uuids::to_string(m_UUID); }
  [[nodiscard]] const boost::uuids::uuid GetUUID() const { return m_UUID; }

  [[nodiscard]] static bool IsNil(UUID uuid) { return uuid.GetUUID().is_nil(); }
  [[nodiscard]] static UUID GetNil() { return {boost::uuids::nil_uuid()}; }

  bool operator==(const UUID &other) const { return m_UUID == other.m_UUID; }
  bool operator!=(const UUID &other) const { return !(*this == other); }

  static void Serialise(std::ostream &out, const UUID &uuid);

  static UUID Deserialise(std::istream &in);

private:
  boost::uuids::uuid m_UUID;

};
} // namespace Hamster

namespace std {
template <> struct hash<Hamster::UUID> {
  std::size_t operator()(const Hamster::UUID &uuid) const {
    return boost::hash<boost::uuids::uuid>()(uuid.GetUUID());
  }
};

template <> struct equal_to<Hamster::UUID> {
  bool operator()(const Hamster::UUID &lhs, const Hamster::UUID &rhs) const {
    return lhs.GetUUID() == rhs.GetUUID();
  }
};
} // namespace std

#endif // UUID_H
