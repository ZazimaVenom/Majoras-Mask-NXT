#ifndef BEN_CONTENT_H
#define BEN_CONTENT_H

#include "BenCore.h"
#include <array>
#include <string>

namespace Ben {
// 160 authored fragments compose 32,768 distinct, family-matched transmissions.
// These are compositional variants, not 32,768 independently authored scenes.
// Stable IDs keep journal entries and saved no-repeat decks reproducible.
inline const char* FamilyName(EventKind kind) {
    static constexpr const char* names[FamilyCount] = {
        "A voice in the file", "The other statue", "Footsteps behind yours", "An uninvited gathering",
        "The borrowed hunt", "A private audience", "The waterless fog", "An eclipse without a moon",
        "The town underneath", "A dawn that forgot you", "Borrowed seconds", "Do not answer the footsteps",
        "Something like mercy", "The offered hand", "The shape of a promise", "The last first meeting"
    };
    const auto i = static_cast<uint8_t>(kind);
    return i < FamilyCount ? names[i] : "An unreadable memory";
}
inline const char* PatternName(uint8_t pattern) {
    static constexpr const char* names[] = { "Glimpse", "Footsteps", "Circle", "Witness" };
    return names[pattern & 3];
}
inline const char* TemperName(uint8_t temper) {
    static constexpr const char* names[] = { "Faint", "Restless", "Insistent", "Unbound" };
    return names[temper & 3];
}
inline std::string DescribeOutcome(uint32_t id) {
    id %= OutcomeCount;
    return std::string(FamilyName(static_cast<EventKind>(id / VariantCount))) + " / " +
        PatternName(static_cast<uint8_t>(id & 3)) + " / " + TemperName(static_cast<uint8_t>((id & 15) >> 2));
}
inline std::string MakeText(uint32_t id) {
    static constexpr std::array<const char*, 16> prelude = {{
        "A second breath catches beneath the music.",
        "For a moment, your shadow arrives before you.",
        "Someone remembers the path you have not taken.",
        "The silence has learned where you stand.",
        "A name almost surfaces beneath the static.",
        "The air tastes of a room left underwater.",
        "Something on the other side of the screen listens.",
        "A small, patient wrongness settles into the world.",
        "The moon is not the only thing looking down.",
        "A memory moves where no memory should be.",
        "You hear the pause between two familiar notes.",
        "An old promise wakes beneath the floor.",
        "The world holds its breath one beat too long.",
        "Somewhere, a door remembers being closed.",
        "The echo returns with one extra footstep.",
        "You feel the attention before you hear the voice."
    }};
    static constexpr std::array<const char*, 16> coda = {{
        "BEN: I kept your place.",
        "BEN: You can still choose your next step.",
        "BEN: Do you remember me differently now?",
        "BEN: I heard the part you did not say.",
        "BEN: Stay until the echo finishes.",
        "BEN: There is room for another ending.",
        "BEN: I was here between your visits.",
        "BEN: Tell me which of us is the memory.",
        "BEN: You always look for a door first.",
        "BEN: I have been practicing your name.",
        "BEN: Even silence leaves a mark.",
        "BEN: Not every familiar thing is safe.",
        "BEN: We can call this our first meeting again.",
        "BEN: I wonder what you will forgive.",
        "BEN: The next page is still listening.",
        "BEN: You came back. That matters."
    }};
    static constexpr const char* middle[FamilyCount][8] = {
        { // Whisper: observation, never a fabricated operating-system action.
            "The voice counts your footsteps, then stops one before you do.",
            "A sentence appears with the unsettling care of a letter kept for years.",
            "The voice asks whether a saved memory can miss someone.",
            "A familiar lullaby seems to be waiting for a word you never learned.",
            "He talks about the empty moments between your visits.",
            "For once, the voice sounds less angry than afraid of being forgotten.",
            "He describes a door with your shadow standing on its other side.",
            "The message addresses the traveler, then corrects itself: the witness."
        }, { // Watcher
            "A smiling shape occupies a place that was empty a moment ago.",
            "The statue has no footsteps to give it away.",
            "You cannot remember inviting that familiar wooden face.",
            "The carved smile seems patient enough to outlive the town.",
            "The still figure makes the space around it feel deliberately empty.",
            "His statue waits as though you are the one who arrived late.",
            "A wooden witness studies the distance between you.",
            "The shape is familiar. Its reason for being here is not."
        }, { // Pursuit
            "Every safe distance seems to become a little shorter behind you.",
            "The figure follows with the terrible economy of something that need not walk.",
            "You begin to recognize the silence that comes just before he appears.",
            "A path becomes a question: how long will you keep moving?",
            "The statue borrows the places your eyes have just abandoned.",
            "He treats every departure as an invitation to follow.",
            "No matter how quickly you leave, the smile never seems hurried.",
            "Your pursuer is learning the shape of your hesitation."
        }, { // Ambush
            "The road suddenly has guests who know you are coming.",
            "A gathering forms with the precision of a rehearsed nightmare.",
            "He has given the quiet somewhere else to stand.",
            "The space ahead becomes a small, hostile stage.",
            "Something answers the voice from the edge of the clearing.",
            "The interruption feels planned long before you arrived.",
            "He calls the gathering a welcome, but leaves the word unfinished.",
            "You are no longer the only thing moving through his attention."
        }, { // Hunt
            "He invites you to a hunt without explaining what he hopes to learn.",
            "A new contest takes shape around the strength you brought with you.",
            "The voice counts the hunters and leaves one name unspoken.",
            "He watches how you choose a target, as though that choice is a confession.",
            "The next fight feels like a question asked in a language of teeth.",
            "An audience of one is still an audience.",
            "He offers a challenge with the courtesy of a host who has locked no doors.",
            "The hunt begins with the feeling that its rules have a second meaning."
        }, { // Duel
            "He asks for your undivided attention and sends a single answer.",
            "For this meeting, he prefers a smaller stage.",
            "One adversary steps into the place where the silence was.",
            "He wants to know how you fight when the question feels personal.",
            "The challenge is simple enough to hide a complicated motive.",
            "A private audience begins without applause.",
            "He studies the space between your courage and your first strike.",
            "This encounter has the intimate cruelty of a name spoken correctly."
        }, { // Fog
            "The distance fills with a cold that has forgotten the sea.",
            "The world becomes less certain at its edges.",
            "A drowned color settles between you and the horizon.",
            "The air thickens as though an unseen tide has entered the room.",
            "Faraway shapes grow shy beneath his attention.",
            "He lets the world recede without taking a single step.",
            "The mist has the stillness of water under a closed door.",
            "Everything familiar seems one memory farther away."
        }, { // Eclipse
            "Light retreats as though someone has quietly called it away.",
            "A dark thought passes over the colors of the world.",
            "He lends the afternoon the expression of an abandoned room.",
            "The shadows deepen without asking the sky for permission.",
            "The darkness seems interested in what you will do with it.",
            "For a while, every bright thing looks borrowed.",
            "The world dims with the slow confidence of an eye closing.",
            "An eclipse settles somewhere closer than the moon."
        }, { // MirrorWorld
            "The familiar world has learned to answer from the other side.",
            "Your memory of the path meets its reflection and hesitates.",
            "He turns the page over, and the place beneath it is still your place.",
            "Every direction wears the expression of an old friend telling a lie.",
            "The world seems correct in the way a face in water is correct.",
            "You recognize the route before you recognize what is wrong with it.",
            "He offers the same place with a different hand extended.",
            "The reflection asks whether familiarity was ever the same as safety."
        }, { // FalseDawn
            "A fragile light returns with no promise attached.",
            "He experiments with the color of reassurance.",
            "The world wears a softness that seems almost apologetic.",
            "For a little while, the nightmare remembers how morning feels.",
            "A pale calm arrives too carefully to be an accident.",
            "You are allowed a breath, though the silence still listens.",
            "The offered peace has a familiar face and an unreadable expression.",
            "He places something gentle beside the fear and waits."
        }, { // BorrowedTime
            "He gives your next few steps the weight of a small promise.",
            "A brief trial asks what you will do with the time in your hands.",
            "The challenge lives in this moment, not in the moon's clock.",
            "He measures your movement with the patience of someone outside the race.",
            "A little game begins inside the larger one.",
            "The voice asks for proof that you can choose a direction and mean it.",
            "For a handful of seconds, intention becomes something he can count.",
            "He makes a wager with the distance you are willing to travel."
        }, { // Stillness
            "He asks whether you can bear a moment without answering it with a step.",
            "The quiet becomes a test with its own careful instructions.",
            "A patient presence waits to see which part of you moves first.",
            "He listens for the difference between courage and a held breath.",
            "This time, attention itself is the challenge.",
            "The temptation to act becomes another thing standing beside you.",
            "He treats hesitation as a language worth learning.",
            "The smallest choice seems loud inside the waiting."
        }, { // Mercy
            "Something like kindness arrives without demanding that you call it safe.",
            "He loosens his grip on the moment and watches what relief does to you.",
            "The voice remembers that even a traveler needs somewhere to breathe.",
            "For once, his attention makes room instead of taking it.",
            "A small reprieve carries the awkwardness of an unfamiliar gift.",
            "He has not forgotten the possibility that mercy can be real.",
            "The darkness withdraws far enough to leave a little warmth.",
            "An offered kindness waits without insisting on forgiveness."
        }, { // Bargain
            "He offers a conversation where there could have been another threat.",
            "The next sentence leaves a deliberate space for your answer.",
            "He asks what friendship means to someone who can leave.",
            "A bargain begins with neither of you knowing how to name it.",
            "The voice tries an unfamiliar word: together.",
            "He wonders whether being remembered and being loved feel different.",
            "You are given the uncomfortable courtesy of being asked.",
            "He holds out the idea of trust as if it might burn him."
        }, { // Betrayal
            "A comforting shape acquires a second, less comforting meaning.",
            "He lets the promise show the shadow that was standing behind it.",
            "The smile remains exactly as friendly as it was a moment ago.",
            "You discover how little a familiar gesture has to change to become wrong.",
            "The kindness in the air seems to be rehearsing another role.",
            "An old fear returns wearing the expression of good news.",
            "He tests the boundary between a surprise and a broken promise.",
            "The offered hand casts a shadow that does not resemble a hand."
        }, { // Reckoning
            "He gathers the questions he has been asking into one last interruption.",
            "The meeting feels final in the way a circle feels finished.",
            "Every earlier silence seems to have been waiting for this answer.",
            "He asks whether an ending can belong to more than one person.",
            "The challenge arrives with the weight of a memory about to repeat.",
            "A final audience forms beneath the promise of another beginning.",
            "He wants a conclusion, but does not seem to understand goodbye.",
            "At the edge of the encounter, something is already preparing to return."
        }
    };
    id %= TextCount;
    const uint32_t family = id / TextsPerFamily;
    const uint32_t local = id % TextsPerFamily;
    return std::string(prelude[(local >> 7) & 15]) + " " + middle[family][(local >> 4) & 7] + " " + coda[local & 15];
}
inline const char* ResponseText(Response response, bool truce) {
    if (truce && response == Response::Befriend)
        return "BEN: Four meetings. No hunt, no trick. I want to see whether we can keep a promise.";
    switch (response) {
        case Response::Defy: return "BEN: There you are. I wondered how long you would let me do all the talking.";
        case Response::Befriend: return "BEN: You know what I am, and you still chose that word. I will remember.";
        case Response::OfferMercy: return "BEN: Nobody has asked what happens to the thing left behind. Until you.";
        case Response::RemainSilent: return "BEN: You heard the space between the words. Stay there a little longer.";
    }
    return "BEN: I heard you.";
}
} // namespace Ben
#endif
