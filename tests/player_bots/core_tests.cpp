// No ROM, renderer, or engine doubles: tests the actual navigation/population code.
#include "../../mm/2s2h/Enhancements/PlayerBots/BotNavigation.h"
#include <iostream>
#include <cstdlib>
#include <set>

// Use the actual scene table, retaining its numeric ordering and unset entries.
#define DEFINE_SCENE(name, enumValue, ...) enumValue,
#define DEFINE_SCENE_UNSET(enumValue) enumValue,
enum SceneId {
#include "../../mm/include/tables/scene_table.h"
    SCENE_ID_MAX
};
#undef DEFINE_SCENE
#undef DEFINE_SCENE_UNSET
#include "../../mm/2s2h/Enhancements/PlayerBots/BotScenePolicy.h"
using namespace PlayerBots;
int assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
auto openEdge = [](int, int) { return true; };
float Cost(const Navigation& graph, const std::vector<int>& path) {
    float cost = 0;
    for (size_t i = 1; i < path.size(); ++i) cost += Distance(graph.nodes[path[i-1]].pos, graph.nodes[path[i]].pos);
    return cost;
}
float Dijkstra(const Navigation& graph, int start, int goal) {
    std::vector<float> costs(graph.nodes.size(), std::numeric_limits<float>::infinity());
    std::priority_queue<std::pair<float,int>, std::vector<std::pair<float,int>>, std::greater<>> queue;
    queue.emplace(0,start); costs[start] = 0;
    while (!queue.empty()) {
        auto [cost, id] = queue.top(); queue.pop();
        if (cost != costs[id]) continue;
        if (id == goal) return cost;
        for (int next : graph.nodes[id].links) {
            float candidate = cost + Distance(graph.nodes[id].pos, graph.nodes[next].pos);
            if (candidate < costs[next]) { costs[next] = candidate; queue.emplace(candidate,next); }
        }
    }
    return std::numeric_limits<float>::infinity();
}
int main() {
    Navigation nav;
    CHECK(nav.Nearest({},false) == -1);
    CHECK(nav.FindPath(0,0,false,openEdge).empty());
    CHECK(nav.Add({0,0,0}) == 0);
    CHECK(nav.Add({1,0,0}) == 1);
    CHECK(nav.Add({2,0,0}) == 2);
    nav.Connect(0,1); nav.Connect(1,2); nav.Connect(0,1); nav.Connect(0,0); nav.Connect(-1,1); nav.Connect(0,99);
    CHECK(nav.nodes[0].links.size() == 1);
    CHECK((nav.FindPath(0,2,false,openEdge) == std::vector<int>{0,1,2}));
    CHECK((nav.FindPath(2,0,false,openEdge) == std::vector<int>{2,1,0}));
    CHECK(nav.FindPath(0,2,false,[](int a,int b) { return a!=1 || b!=2; }).empty());
    CHECK(nav.FindPath(0,2,false,openEdge,0).empty());
    CHECK(nav.FindPath(0,2,false,openEdge,1).empty());
    CHECK(nav.FindPath(0,2,false,openEdge,-3).empty());
    CHECK((nav.FindPath(1,1,false,openEdge) == std::vector<int>{1}));
    CHECK(nav.FindPath(-1,2,false,openEdge).empty());
    CHECK(nav.FindPath(0,99,false,openEdge).empty());
    nav.nodes[1].underwater = true;
    CHECK(nav.FindPath(0,2,false,openEdge).empty());
    CHECK(nav.FindPath(0,2,true,openEdge).size() == 3);
    CHECK(nav.FindPath(1,1,false,openEdge).empty());
    CHECK(nav.Nearest({1,0,0},true) == 1);
    CHECK(nav.Nearest({1,0,0},false) != 1);
    CHECK(nav.Nearest({1,0,0},false,0.4f) == -1);
    CHECK(nav.Nearest({0,0,0},false,-1) == -1);
    CHECK(nav.Add({NAN,0,0}) == -1);
    CHECK(nav.Add({0,INFINITY,0}) == -1);
    CHECK(nav.Nearest({NAN,0,0},true) == -1);
    nav.Clear(); CHECK(nav.nodes.empty());
    for (int i=0; i<Navigation::MaxNodes; ++i) CHECK(nav.Add({float(i),0,0}) == i);
    CHECK(nav.Add({0,0,1}) == -1);
    for (int i=1; i<Navigation::MaxNodes; ++i) nav.Connect(i-1,i);
    CHECK(nav.FindPath(0,4095,false,openEdge).size() == 4096);
    CHECK(nav.FindPath(0,4095,false,openEdge,1600).empty());
    // Seeded sparse graphs: independent shortest-path oracle, both reachable
    // and disconnected cases. Euclidean edge lengths must give optimal A* cost.
    uint32_t rng = 721;
    int graphs = 100, paths = 0;
    for (int test=0;test<graphs;++test) {
        Navigation graph;
        for (int i=0;i<80;++i) graph.Add({RandomUnit(rng)*800,RandomUnit(rng)*80,RandomUnit(rng)*800});
        for (int i=0;i<120;++i) graph.Connect(NextRandom(rng)%80,NextRandom(rng)%80);
        for (int query=0;query<12;++query) {
            const int a=NextRandom(rng)%80,b=NextRandom(rng)%80;
            auto path=graph.FindPath(a,b,true,openEdge);
            const float reference=Dijkstra(graph,a,b);
            CHECK(path.empty() == !std::isfinite(reference));
            if (!path.empty()) {
                CHECK(path.front()==a && path.back()==b);
                CHECK(std::fabs(Cost(graph,path)-reference) < 0.02f);
                for (size_t j=1;j<path.size();++j)
                    CHECK(std::find(graph.nodes[path[j-1]].links.begin(),graph.nodes[path[j-1]].links.end(),path[j]) != graph.nodes[path[j-1]].links.end());
            }
            ++paths;
        }
    }
    CHECK(DesiredBotCount(-999)==10 && DesiredBotCount(15)==15 && DesiredBotCount(999)==20);
    CHECK(DesiredReinforcements(15,0,2,28)==28);
    CHECK(DesiredReinforcements(15,7,2,28)==23);
    CHECK(DesiredReinforcements(15,100,2,28)==0);
    CHECK(DesiredReinforcements(-10,0,2,28)==0);
    CHECK(DesiredReinforcements(2147483647,0,100,100)==32);
    CHECK(DesiredReinforcements(20,0,3,-1)==0);
    CHECK(DesiredReinforcements(20,0,3,0)==0);
    CHECK(DesiredReinforcements(20,0,3,12)==12);
    CHECK(ReinforcementDelay(3,1)==3.0f);
    CHECK(std::fabs(ReinforcementDelay(3,15)-0.2f)<0.001f);
    CHECK(ReinforcementDelay(-1,100)==0.2f);
    CHECK(ReinforcementDelay(999,0)==30.0f);
    CHECK(ReinforcementDelay(3,6)==0.5f);
    CHECK(HasActorBudget(209) && !HasActorBudget(210) && !HasActorBudget(255) && !HasActorBudget(-1));
    uint32_t a=0,b=0;
    for (int i=0;i<10000;++i) { CHECK(NextRandom(a)==NextRandom(b)); CHECK(a!=0); float unit=RandomUnit(a); RandomUnit(b); CHECK(unit>=0 && unit<1); }
    const int peaceful[] = { SCENE_TOWN,SCENE_ICHIBA,SCENE_BACKTOWN,SCENE_CLOCKTOWER,SCENE_YADOYA,SCENE_POSTHOUSE,SCENE_8ITEMSHOP,SCENE_LABO,SCENE_FISHERMAN,SCENE_KAJIYA,SCENE_16GORON_HOUSE,SCENE_33ZORACITY,SCENE_F01,SCENE_TAKARAKUJI,SCENE_22DEKUCITY,SCENE_TENMON_DAI };
    for (int scene:peaceful) { CHECK(!IsCombatScene(scene)); CHECK(!IsExcludedScene(scene)); }
    const std::set<int> approved = {SCENE_00KEIKOKU,SCENE_24KEMONOMITI,SCENE_20SICHITAI,SCENE_20SICHITAI2,SCENE_21MITURINMAE,SCENE_26SARUNOMORI,SCENE_13HUBUKINOMITI,SCENE_14YUKIDAMANOMITI,SCENE_17SETUGEN,SCENE_17SETUGEN2,SCENE_30GYOSON,SCENE_31MISAKI,SCENE_IKANA,SCENE_IKANAMAE,SCENE_F40,SCENE_F41,SCENE_MITURIN,SCENE_HAKUGIN,SCENE_SEA,SCENE_INISIE_N,SCENE_INISIE_R,SCENE_CASTLE};
    for (int scene=-20;scene<SCENE_ID_MAX+30;++scene) {
        CHECK(IsCombatScene(scene)==(approved.count(scene)!=0));
        CHECK(!IsExcludedScene(scene) || !IsCombatScene(scene));
    }
    CHECK(IsExcludedScene(SCENE_LAST_BS) && IsExcludedScene(SCENE_MITURIN_BS));
    CHECK(IsExcludedScene(SCENE_GORONRACE) && IsExcludedScene(SCENE_SYATEKI_MIZU));
    std::cout << "PASS: " << assertions << " assertions; " << graphs << " random graphs, " << paths << " A*/Dijkstra comparisons; scene policy, capacities, deterministic RNG.\n";
}
