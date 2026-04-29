#pragma once

#include "entity/cbaseplayerpawn.h"

class C_CSPlayerPawnBase : public C_BasePlayerPawn {
public:
    SCHEMA_FIELD(CCSPlayer_WeaponServices*, m_pWeaponServices)
};
