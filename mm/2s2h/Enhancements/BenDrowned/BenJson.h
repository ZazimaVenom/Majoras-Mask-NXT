#ifndef BEN_JSON_H
#define BEN_JSON_H

#include "BenCore.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Ben {
template <typename T>
inline T ReadUnsigned(const nlohmann::json& object, const char* key, T fallback) {
    static_assert(std::is_unsigned_v<T>);
    if (!object.contains(key)) return fallback;
    const auto& value = object.at(key);
    uint64_t number;
    if (value.is_number_unsigned()) number = value.get<uint64_t>();
    else if (value.is_number_integer() && value.get<int64_t>() >= 0)
        number = static_cast<uint64_t>(value.get<int64_t>());
    else throw std::runtime_error(std::string("Invalid BEN save field: ") + key);
    if (number > std::numeric_limits<T>::max())
        throw std::runtime_error(std::string("BEN save field out of range: ") + key);
    return static_cast<T>(number);
}
inline int32_t ReadSigned(const nlohmann::json& object, const char* key, int32_t fallback) {
    if (!object.contains(key)) return fallback;
    const auto& value = object.at(key);
    if (!value.is_number_integer())
        throw std::runtime_error(std::string("Invalid signed BEN save field: ") + key);
    if (value.is_number_unsigned() && value.get<uint64_t>() > INT32_MAX)
        throw std::runtime_error(std::string("BEN save field out of range: ") + key);
    const int64_t number = value.get<int64_t>();
    if (number < INT32_MIN || number > INT32_MAX)
        throw std::runtime_error(std::string("BEN save field out of range: ") + key);
    return static_cast<int32_t>(number);
}
template <size_t N>
inline void ReadDeck(const nlohmann::json& object, const char* key, uint32_t (&dest)[N]) {
    if (!object.contains(key)) return;
    const auto& values = object.at(key);
    if (!values.is_array() || values.size() != N)
        throw std::runtime_error(std::string("Invalid BEN save deck: ") + key);
    for (size_t i = 0; i < N; ++i) {
        // Reuse the strict numeric reader; get<uint32_t> alone would truncate.
        const nlohmann::json entry = {{"value", values.at(i)}};
        dest[i] = ReadUnsigned<uint32_t>(entry, "value", 0);
    }
}
inline BenSaveData DecodeProgress(const nlohmann::json& j) {
    if (!j.is_object()) throw std::runtime_error("BEN save section must be an object");
    BenSaveData next = NewProgress();
    next.version = ReadUnsigned<uint32_t>(j, "version", SaveVersion);
    if (next.version != SaveVersion)
        throw std::runtime_error("This BEN save requires a different director version");
    next.randomState = ReadUnsigned<uint32_t>(j, "randomState", next.randomState);
    next.narrativeSeed = ReadUnsigned<uint32_t>(j, "narrativeSeed", next.narrativeSeed);
    next.sessions = ReadUnsigned<uint32_t>(j, "sessions", next.sessions);
    next.sceneVisits = ReadUnsigned<uint32_t>(j, "sceneVisits", next.sceneVisits);
    next.lastScene = ReadUnsigned<uint32_t>(j, "lastScene", next.lastScene);
    next.cycleReturns = ReadUnsigned<uint32_t>(j, "cycleReturns", next.cycleReturns);
    next.eventCount = ReadUnsigned<uint32_t>(j, "eventCount", next.eventCount);
    next.eventsInLoop = ReadUnsigned<uint32_t>(j, "eventsInLoop", next.eventsInLoop);
    next.loops = ReadUnsigned<uint32_t>(j, "loops", next.loops);
    next.cooldownMs = ReadUnsigned<uint32_t>(j, "cooldownMs", next.cooldownMs);
    next.idleMs = ReadUnsigned<uint32_t>(j, "idleMs", next.idleMs);
    next.movingMs = ReadUnsigned<uint32_t>(j, "movingMs", next.movingMs);
    next.arrivalMs = ReadUnsigned<uint32_t>(j, "arrivalMs", next.arrivalMs);
    next.killMemoryMs = ReadUnsigned<uint32_t>(j, "killMemoryMs", next.killMemoryMs);
    next.observedKills = ReadUnsigned<uint32_t>(j, "observedKills", next.observedKills);
    next.deaths = ReadUnsigned<uint32_t>(j, "deaths", next.deaths);
    next.trialsWon = ReadUnsigned<uint32_t>(j, "trialsWon", next.trialsWon);
    next.trialsLost = ReadUnsigned<uint32_t>(j, "trialsLost", next.trialsLost);
    next.responses = ReadUnsigned<uint32_t>(j, "responses", next.responses);
    next.respondedEvent = ReadUnsigned<uint32_t>(j, "respondedEvent", next.respondedEvent);
    next.listens = ReadUnsigned<uint32_t>(j, "listens", next.listens);
    next.discoveries = ReadUnsigned<uint32_t>(j, "discoveries", next.discoveries);
    next.pactEvents = ReadUnsigned<uint32_t>(j, "pactEvents", next.pactEvents);
    next.lastKind = ReadUnsigned<uint32_t>(j, "lastKind", next.lastKind);
    next.lastOutcome = ReadUnsigned<uint32_t>(j, "lastOutcome", next.lastOutcome);
    next.lastText = ReadUnsigned<uint32_t>(j, "lastText", next.lastText);
    next.activeMs = ReadUnsigned<uint64_t>(j, "activeMs", next.activeMs);
    next.trust = ReadSigned(j, "trust", next.trust);
    next.defiance = ReadSigned(j, "defiance", next.defiance);
    next.mercy = ReadSigned(j, "mercy", next.mercy);
    next.tension = ReadSigned(j, "tension", next.tension);
    ReadDeck(j, "recentKinds", next.recentKinds);
    ReadDeck(j, "textCursor", next.textCursor);
    ReadDeck(j, "variantCursor", next.variantCursor);
    Normalize(next);
    return next;
}
} // namespace Ben

// Global C struct requires global ADL converters. Decode is transactional: the
// caller's progress changes only after every field and deck has validated.
inline void to_json(nlohmann::json& j, const BenSaveData& value) {
    auto p = value;
    Ben::Normalize(p);
    if (p.version != Ben::SaveVersion)
        throw std::runtime_error("Refusing to overwrite an unknown BEN save schema");
    j = nlohmann::json{
        {"version", p.version},
        {"randomState", p.randomState},
        {"narrativeSeed", p.narrativeSeed},
        {"sessions", p.sessions},
        {"sceneVisits", p.sceneVisits},
        {"lastScene", p.lastScene},
        {"cycleReturns", p.cycleReturns},
        {"eventCount", p.eventCount},
        {"eventsInLoop", p.eventsInLoop},
        {"loops", p.loops},
        {"cooldownMs", p.cooldownMs},
        {"idleMs", p.idleMs},
        {"movingMs", p.movingMs},
        {"arrivalMs", p.arrivalMs},
        {"killMemoryMs", p.killMemoryMs},
        {"observedKills", p.observedKills},
        {"deaths", p.deaths},
        {"trialsWon", p.trialsWon},
        {"trialsLost", p.trialsLost},
        {"responses", p.responses},
        {"respondedEvent", p.respondedEvent},
        {"listens", p.listens},
        {"discoveries", p.discoveries},
        {"pactEvents", p.pactEvents},
        {"lastKind", p.lastKind},
        {"lastOutcome", p.lastOutcome},
        {"lastText", p.lastText},
        {"activeMs", p.activeMs},
        {"trust", p.trust},
        {"defiance", p.defiance},
        {"mercy", p.mercy},
        {"tension", p.tension},
    };
    j["recentKinds"] = nlohmann::json::array();
    for (const auto value : p.recentKinds) j["recentKinds"].push_back(value);
    j["textCursor"] = nlohmann::json::array();
    for (const auto value : p.textCursor) j["textCursor"].push_back(value);
    j["variantCursor"] = nlohmann::json::array();
    for (const auto value : p.variantCursor) j["variantCursor"].push_back(value);
}
inline void from_json(const nlohmann::json& j, BenSaveData& value) {
    value = Ben::DecodeProgress(j);
}
#endif
