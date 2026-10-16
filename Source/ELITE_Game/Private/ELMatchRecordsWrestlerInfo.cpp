#include "ELMatchRecordsWrestlerInfo.h"

FELMatchRecordsWrestlerInfo::FELMatchRecordsWrestlerInfo() {
    this->WrestlerID = EWrestlerID_N::None;
    this->WiningStreak = 0;
    this->EnterCountOnlyHostPad = 0;
    this->LastEnterDate = FDateTime(0);
}

