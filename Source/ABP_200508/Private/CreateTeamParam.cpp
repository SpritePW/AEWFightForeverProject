#include "CreateTeamParam.h"

FCreateTeamParam::FCreateTeamParam() {
    this->AnnounceId = 0;
    this->AEW_TagTeam = false;
    this->FanReaction = EFanReaction::Babyface;
    this->DLC_TagTeam = false;
    this->UnlockID = 0;
    this->CreatedDate = FDateTime(0);
    this->LastModifiedDate = FDateTime(0);
}

