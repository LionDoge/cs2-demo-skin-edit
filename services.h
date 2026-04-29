#pragma once

#include "entity/ccsplayerpawn.h"
#include "entity/ccsweaponbase.h"

class CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayerPawnComponent);

	SCHEMA_FIELD(C_CSPlayerPawn*, __m_pChainEntity)

		C_CSPlayerPawn* GetPawn() { return __m_pChainEntity; }
};

class CPlayer_WeaponServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_WeaponServices);

	SCHEMA_FIELD_POINTER(CUtlVector<CHandle<C_BasePlayerWeapon>>, m_hMyWeapons)
	SCHEMA_FIELD(CHandle<C_BasePlayerWeapon>, m_hActiveWeapon)
};