/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "cbaseentity.h"
#include "econitemdefinition.hpp"

enum gear_slot_t : uint32_t
{
	GEAR_SLOT_INVALID = 0xffffffff,
	GEAR_SLOT_RIFLE = 0x0,
	GEAR_SLOT_PISTOL = 0x1,
	GEAR_SLOT_KNIFE = 0x2,
	GEAR_SLOT_GRENADES = 0x3,
	GEAR_SLOT_C4 = 0x4,
	GEAR_SLOT_RESERVED_SLOT6 = 0x5,
	GEAR_SLOT_RESERVED_SLOT7 = 0x6,
	GEAR_SLOT_RESERVED_SLOT8 = 0x7,
	GEAR_SLOT_RESERVED_SLOT9 = 0x8,
	GEAR_SLOT_RESERVED_SLOT10 = 0x9,
	GEAR_SLOT_RESERVED_SLOT11 = 0xa,
	GEAR_SLOT_BOOSTS = 0xb,
	GEAR_SLOT_UTILITY = 0xc,
	GEAR_SLOT_COUNT = 0xd,
	GEAR_SLOT_FIRST = 0x0,
	GEAR_SLOT_LAST = 0xc,
};

class CEconItemAttribute
{
public:
	virtual void unk() {};
private:
	[[maybe_unused]] uint8_t __pad0000[0x28]; // 0x0
public:
	// MNetworkEnable
	uint16_t m_iAttributeDefinitionIndex = 0.0; // 0x30	
private:
	[[maybe_unused]] uint8_t __pad0032[0x2]; // 0x32
public:
	// MNetworkEnable
	// MNetworkAlias "m_iRawValue32"
	uint32_t m_flValue = 0.0; // 0x34	
	// MNetworkEnable
	uint32_t m_flInitialValue = 0.0; // 0x38	
	// MNetworkEnable
	int32_t m_nRefundableCurrency = 0; // 0x3c	
	// MNetworkEnable
	bool m_bSetBonus = false; // 0x40	
	char pad[0x7];
};

class CAttributeList
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CAttributeList);

	SCHEMA_FIELD_POINTER(CUtlVector<CEconItemAttribute>, m_Attributes)
};

class C_CSPlayerPawn;
class C_EconItemView
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(C_EconItemView, 27);
	auto GetCustomPaintKitIndex() { return CALL_VIRTUAL(int, 2, this); }
	auto GetStaticData() {
		return CALL_VIRTUAL(CEconItemDefinition*, 13, this);
	}

	SCHEMA_FIELD(uint16_t, m_iItemDefinitionIndex)
	SCHEMA_FIELD(int32_t, m_iEntityQuality)
	SCHEMA_FIELD(uint32_t, m_iEntityLevel)
	SCHEMA_FIELD(bool, m_bInitialized)
	SCHEMA_FIELD(uint32_t, m_iItemIDHigh)
	SCHEMA_FIELD(uint32_t, m_iItemIDLow)
	SCHEMA_FIELD(uint64_t, m_iItemID)
	SCHEMA_FIELD(uint32_t, m_iAccountID)
	SCHEMA_FIELD_POINTER(CAttributeList, m_NetworkedDynamicAttributes)
	SCHEMA_FIELD_POINTER(CAttributeList, m_AttributeList)
	SCHEMA_FIELD(bool, m_bDisallowSOC)
	SCHEMA_FIELD(bool, m_bRestoreCustomMaterialAfterPrecache)
	SCHEMA_FIELD(bool, m_bIsStoreItem)
	SCHEMA_FIELD_POINTER(char, m_szCustomName)
};

class C_AttributeContainer
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(C_AttributeContainer);

	SCHEMA_FIELD_POINTER(C_EconItemView, m_Item)
};

class C_EconEntity : public C_BaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(C_EconEntity)

	SCHEMA_FIELD_POINTER(C_AttributeContainer, m_AttributeManager)
	SCHEMA_FIELD(float, m_flFallbackWear)
	SCHEMA_FIELD(int32_t, m_nFallbackPaintKit)
	SCHEMA_FIELD(int32_t, m_nFallbackSeed)
	SCHEMA_FIELD(uint32_t, m_OriginalOwnerXuidLow)
	SCHEMA_FIELD(uint32_t, m_OriginalOwnerXuidHigh)
};

class CBasePlayerWeaponVData : public CEntitySubclassVDataBase
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerWeaponVData)
	SCHEMA_FIELD(int, m_iMaxClip1)
};

class CCSWeaponBaseVData : public CBasePlayerWeaponVData
{
public:
	DECLARE_SCHEMA_CLASS(CCSWeaponBaseVData)

	SCHEMA_FIELD(gear_slot_t, m_GearSlot)
		SCHEMA_FIELD(int, m_nPrice)
		SCHEMA_FIELD(int, m_nPrimaryReserveAmmoMax);
};

class C_BasePlayerWeapon : public C_EconEntity
{
public:
	DECLARE_SCHEMA_CLASS(C_BasePlayerWeapon)

	//CCSWeaponBaseVData* GetWeaponVData() { return (CCSWeaponBaseVData*)GetVData(); }
};

class C_CSWeaponBase : public C_BasePlayerWeapon
{
public:
	DECLARE_SCHEMA_CLASS(C_CSWeaponBase)
	SCHEMA_FIELD(CHandle<C_CSPlayerPawn>, m_hPrevOwner)
};