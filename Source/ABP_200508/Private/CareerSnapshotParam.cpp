#include "CareerSnapshotParam.h"

FCareerSnapshotParam::FCareerSnapshotParam() {
    this->ScenarioNo = ECareerScenarioNo::None;
    this->Week = 0;
    this->Location = ECareerStageType::NONE;
    this->Turn = 0;
    this->DateTime = FDateTime(0);
}

