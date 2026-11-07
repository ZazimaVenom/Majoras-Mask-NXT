#ifndef PLAYER_BOTS_SCENE_POLICY_H
#define PLAYER_BOTS_SCENE_POLICY_H

// Inclusion lists deliberately fail closed. A newly added/unknown scene gets no
// enemy reinforcements. Ordinary interiors still receive peaceful visitors.
namespace PlayerBots {
enum class Biome { Safe, Field, Swamp, Snow, Coast, Canyon, Dungeon };
inline Biome SceneBiome(int scene) {
    switch (scene) {
        case SCENE_00KEIKOKU: return Biome::Field;
        case SCENE_24KEMONOMITI:
        case SCENE_20SICHITAI:
        case SCENE_20SICHITAI2:
        case SCENE_21MITURINMAE:
        case SCENE_26SARUNOMORI: return Biome::Swamp;
        case SCENE_13HUBUKINOMITI:
        case SCENE_14YUKIDAMANOMITI:
        case SCENE_17SETUGEN:
        case SCENE_17SETUGEN2: return Biome::Snow;
        case SCENE_30GYOSON:
        case SCENE_31MISAKI: return Biome::Coast;
        case SCENE_IKANA:
        case SCENE_IKANAMAE:
        case SCENE_F40:
        case SCENE_F41: return Biome::Canyon;
        case SCENE_MITURIN:
        case SCENE_HAKUGIN:
        case SCENE_SEA:
        case SCENE_INISIE_N:
        case SCENE_INISIE_R:
        case SCENE_CASTLE: return Biome::Dungeon;
        default: return Biome::Safe;
    }
}
inline bool IsCombatScene(int scene) { return SceneBiome(scene) != Biome::Safe; }
inline bool IsExcludedScene(int scene) {
    // Scripted boss arenas, timed/stealth challenges and cutscene-only maps are
    // deliberately left vanilla, not populated with actors that break scripts.
    switch (scene) {
        case SCENE_SPOT00:
        case SCENE_LAST_BS:
        case SCENE_OKUJOU:
        case SCENE_OPENINGDAN:
        case SCENE_MITURIN_BS:
        case SCENE_HAKUGIN_BS:
        case SCENE_INISIE_BS:
        case SCENE_SEA_BS:
        case SCENE_IKNINSIDE:
        case SCENE_KONPEKI_ENT:
        case SCENE_LOST_WOODS:
        case SCENE_KYOJINNOMA:
        case SCENE_SYATEKI_MIZU:
        case SCENE_SYATEKI_MORI:
        case SCENE_DEKUTES:
        case SCENE_BOWLING:
        case SCENE_GORONRACE:
        case SCENE_KOEPONARACE:
        case SCENE_F01_B:
        case SCENE_SECOM:
        case SCENE_TOUGITES:
        case SCENE_RANDOM:
        case SCENE_KAIZOKU:
        case SCENE_PIRATE:
        case SCENE_TORIDE:
        case SCENE_LAST_DEKU:
        case SCENE_LAST_GORON:
        case SCENE_LAST_ZORA:
        case SCENE_LAST_LINK:
        case SCENE_DANPEI: return true;
        default: return false;
    }
}
} // namespace PlayerBots
#endif
