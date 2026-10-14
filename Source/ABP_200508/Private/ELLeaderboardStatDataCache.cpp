#include "ELLeaderboardStatDataCache.h"

FELLeaderboardStatDataCache::FELLeaderboardStatDataCache() {
    this->LastRequestDateTime = FDateTime(0);
    this->LastQueryDateTime = FDateTime(0);
}

