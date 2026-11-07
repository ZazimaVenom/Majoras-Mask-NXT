#include <cstddef>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <climits>
static const float POOL_BALANCE_UNLIKELY_RATIO = 0.85f;
static const float POOL_BALANCE_LIMIT_RATIO = 0.9f;
static std::size_t checksInPool = 0;
static std::size_t itemsInPool = 0;
static std::size_t junkInPool = 0;
static int balanceStatus = 0;
void refresh(std::size_t checks, std::size_t items, std::size_t junk) {
checksInPool = checks;
itemsInPool = items;
junkInPool = junk;
    if (static_cast<float>(checksInPool) * POOL_BALANCE_LIMIT_RATIO <
        static_cast<float>(itemsInPool - junkInPool)) {
        balanceStatus = 2;
        // If there are only slightly more checks than non-junk items, balancing is unlikely
    } else if (static_cast<float>(checksInPool) * POOL_BALANCE_UNLIKELY_RATIO <
               static_cast<float>(itemsInPool - junkInPool)) {
        balanceStatus = 1;
    } else {
        balanceStatus = 0;
    }
}
int main() {
unsigned cases=0;
for (std::size_t checks=0;checks<=200;checks++) {
  for (std::size_t important=0;important<=200;important++) {
    for (std::size_t junk : {std::size_t(0),std::size_t(7)}) {
      refresh(checks,important+junk,junk);
      int expected = static_cast<float>(checks) * 0.9f < static_cast<float>(important) ? 2 :
                     static_cast<float>(checks) * 0.85f < static_cast<float>(important) ? 1 : 0;
      assert(balanceStatus == expected); ++cases;
    }
  }
}
refresh(static_cast<std::size_t>(INT_MAX)+4,static_cast<std::size_t>(INT_MAX)+9,3);
const std::size_t importantItems = itemsInPool-junkInPool;
std::vector<int> checkExclusionList(12);
char buffer[256];
std::snprintf(buffer,sizeof(buffer),"%zu checks  |  %zu important items  |  %zu junk fill  |  %zu excluded",
checksInPool, importantItems, junkInPool, checkExclusionList.size());
const std::string expected = std::to_string(checksInPool) + " checks  |  " + std::to_string(importantItems) + " important items  |  3 junk fill  |  12 excluded";
assert(buffer == expected); ++cases;
std::printf("PASS: %u source-extracted pool-metric/format cases including counts above INT_MAX.\n",cases);
}
