#ifndef RPG_JSON_H
#define RPG_JSON_H

#include "RpgCore.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Rpg {
// Read into a temporary value and commit only after all fields validate. Do not
// reinterpret negative/fractional JSON as huge unsigned experience/counters.
template <typename T>
inline T ReadUnsigned(const nlohmann::json& object, const char* key, T fallback) {
    static_assert(std::is_unsigned_v<T>);
    if (!object.contains(key)) return fallback;
    const auto& value = object.at(key);
    uint64_t number;
    if (value.is_number_unsigned()) number = value.get<uint64_t>();
    else if (value.is_number_integer() && value.get<int64_t>() >= 0)
        number = static_cast<uint64_t>(value.get<int64_t>());
    else throw std::runtime_error(std::string("Invalid RPG save field: ") + key);
    if (number > std::numeric_limits<T>::max())
        throw std::runtime_error(std::string("RPG save field out of range: ") + key);
    return static_cast<T>(number);
}
inline RpgSaveData DecodeProgress(const nlohmann::json& j) {
    if (!j.is_object()) throw std::runtime_error("RPG save section must be an object");
    RpgSaveData next = NewProgress();
    next.version = ReadUnsigned<uint32_t>(j, "version", SaveVersion);
    if (next.version != SaveVersion)
        throw std::runtime_error("This RPG save requires a different RPG framework version");
    next.level = ReadUnsigned<uint32_t>(j, "level", 1);
    next.experience = ReadUnsigned<uint64_t>(j, "experience", 0);
    next.totalExperience = ReadUnsigned<uint64_t>(j, "totalExperience", 0);
    next.creditedKills = ReadUnsigned<uint32_t>(j, "creditedKills", 0);
    next.berserkKills = ReadUnsigned<uint32_t>(j, "berserkKills", 0);
    next.worldBossKills = ReadUnsigned<uint32_t>(j, "worldBossKills", 0);
    next.randomState = ReadUnsigned<uint32_t>(j, "randomState", next.randomState);
    next.bossCooldownMs = ReadUnsigned<uint32_t>(j, "bossCooldownMs", 0);
    Normalize(next);
    return next;
}
} // namespace Rpg

// RpgSaveData is a C struct in the global namespace: these ADL conversions must
// also be global. Both owl saves and Song of Time saves use ShipSaveInfo's JSON.
inline void to_json(nlohmann::json& j, const RpgSaveData& value) {
    auto p = value;
    Rpg::Normalize(p);
    if (p.version != Rpg::SaveVersion)
        throw std::runtime_error("Refusing to overwrite an unknown RPG save schema");
    j = nlohmann::json{
        {"version", p.version}, {"level", p.level}, {"experience", p.experience},
        {"totalExperience", p.totalExperience}, {"creditedKills", p.creditedKills},
        {"berserkKills", p.berserkKills}, {"worldBossKills", p.worldBossKills},
        {"randomState", p.randomState}, {"bossCooldownMs", p.bossCooldownMs}
    };
}
inline void from_json(const nlohmann::json& j, RpgSaveData& value) {
    value = Rpg::DecodeProgress(j);
}
#endif
