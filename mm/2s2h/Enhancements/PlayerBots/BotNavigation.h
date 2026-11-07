#ifndef PLAYER_BOTS_NAVIGATION_H
#define PLAYER_BOTS_NAVIGATION_H

// Engine-independent navigation and population rules. See tests/player_bots.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace PlayerBots {
struct Point {
    float x = 0, y = 0, z = 0;
};
inline float DistanceSquared(Point a, Point b) {
    const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    return x * x + y * y + z * z;
}
inline float Distance(Point a, Point b) { return std::sqrt(DistanceSquared(a, b)); }
inline bool Finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

struct NavNode {
    Point pos;
    bool underwater = false;
    std::vector<int> links;
};

class Navigation {
  public:
    static constexpr int MaxNodes = 4096;
    std::vector<NavNode> nodes;

    void Clear() { nodes.clear(); }
    int Add(Point pos, bool underwater = false) {
        if (!Finite(pos) || nodes.size() >= MaxNodes) return -1;
        nodes.push_back({ pos, underwater, {} });
        return static_cast<int>(nodes.size()) - 1;
    }
    void Connect(int a, int b) {
        if (!Valid(a) || !Valid(b) || a == b) return;
        auto add = [](std::vector<int>& links, int id) {
            if (std::find(links.begin(), links.end(), id) == links.end()) links.push_back(id);
        };
        add(nodes[a].links, b);
        add(nodes[b].links, a);
    }
    bool Valid(int index) const { return index >= 0 && static_cast<size_t>(index) < nodes.size(); }
    int Nearest(Point point, bool allowWater, float maxDistance = 240.0f) const {
        if (!Finite(point) || !std::isfinite(maxDistance) || maxDistance < 0) return -1;
        int result = -1;
        float best = maxDistance * maxDistance;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (!allowWater && nodes[i].underwater) continue;
            const float distance = DistanceSquared(point, nodes[i].pos);
            if (distance < best) { best = distance; result = static_cast<int>(i); }
        }
        return result;
    }
    // Caller supplies live edge validation (doors, moving blocks, etc.). An exhausted
    // search returns no path; it never substitutes a straight line through a wall.
    template <typename EdgeAllowed>
    std::vector<int> FindPath(int start, int goal, bool allowWater, EdgeAllowed edgeAllowed,
                              int expansionBudget = MaxNodes) const {
        if (!Valid(start) || !Valid(goal) || (!allowWater && (nodes[start].underwater || nodes[goal].underwater)))
            return {};
        using Entry = std::pair<float, int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
        std::vector<float> score(nodes.size(), std::numeric_limits<float>::infinity());
        std::vector<int> parent(nodes.size(), -1);
        std::vector<bool> closed(nodes.size(), false);
        score[start] = 0;
        open.emplace(Distance(nodes[start].pos, nodes[goal].pos), start);
        while (!open.empty() && expansionBudget-- > 0) {
            const int current = open.top().second;
            open.pop();
            if (closed[current]) continue;
            if (current == goal) {
                std::vector<int> path;
                for (int i = current; i != -1; i = parent[i]) path.push_back(i);
                std::reverse(path.begin(), path.end());
                return path;
            }
            closed[current] = true;
            for (int next : nodes[current].links) {
                if (!Valid(next) || closed[next] || (!allowWater && nodes[next].underwater) ||
                    !edgeAllowed(current, next)) continue;
                const float candidate = score[current] + Distance(nodes[current].pos, nodes[next].pos);
                if (candidate < score[next]) {
                    score[next] = candidate;
                    parent[next] = current;
                    open.emplace(candidate + Distance(nodes[next].pos, nodes[goal].pos), next);
                }
            }
        }
        return {};
    }
};

inline int DesiredBotCount(int requested) { return std::clamp(requested, 10, 20); }
inline int DesiredReinforcements(int bots, int nativeEnemies, int perBot, int maximum) {
    return std::clamp(std::clamp(bots, 0, 20) * std::clamp(perBot, 1, 3) - std::max(0, nativeEnemies), 0,
                      std::clamp(maximum, 0, 32));
}
// A large deficit refills in staggered bursts rather than one monster every
// several seconds being overwhelmed before the next can join the battle.
inline float ReinforcementDelay(int baseSeconds, int deficit) {
    return std::max(0.20f, static_cast<float>(std::clamp(baseSeconds, 1, 30)) / std::clamp(deficit, 1, 15));
}
inline bool HasActorBudget(int actors) { return actors >= 0 && actors < 210; }
inline uint32_t NextRandom(uint32_t& state) {
    if (state == 0) state = 0xA341316Cu;
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
inline float RandomUnit(uint32_t& state) { return (NextRandom(state) & 0xFFFFFFu) / 16777216.0f; }
} // namespace PlayerBots
#endif
