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
#include "cbaseplayerpawn.h"
#include "viewmodelservices.h"

enum CSPlayerState
{
	STATE_ACTIVE = 0x0,
	STATE_WELCOME = 0x1,
	STATE_PICKINGTEAM = 0x2,
	STATE_PICKINGCLASS = 0x3,
	STATE_DEATH_ANIM = 0x4,
	STATE_DEATH_WAIT_FOR_KEY = 0x5,
	STATE_OBSERVER_MODE = 0x6,
	STATE_GUNGAME_RESPAWN = 0x7,
	STATE_DORMANT = 0x8,
	NUM_PLAYER_STATES = 0x9,
};


class C_CSPlayerPawnBase : public C_BasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS(C_CSPlayerPawnBase);
	SCHEMA_FIELD(QAngle, m_angEyeAngles)
	SCHEMA_FIELD(CSPlayerState, m_iPlayerState)
	SCHEMA_FIELD_POINTER(CCSPlayer_ViewModelServices, m_pViewModelServices)
	//SCHEMA_FIELD(CHandle<CCSPlayerController>, m_hOriginalController)

	/*CCSPlayerController *GetOriginalController()
	{
		return m_hOriginalController().Get();
	}*/

	bool IsBot()
	{
		return m_fFlags() & FL_BOT;
	}
};


class C_CS2HudModelArms : public C_BaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(C_CS2HudModelArms);
};

class C_CSPlayerPawn : public C_CSPlayerPawnBase
{
public:
	DECLARE_SCHEMA_CLASS(C_CSPlayerPawn);

	SCHEMA_FIELD(float, m_flVelocityModifier)
	SCHEMA_FIELD_POINTER(C_EconItemView, m_EconGloves)
	SCHEMA_FIELD(bool, m_bNeedToReApplyGloves)
	SCHEMA_FIELD(CHandle<C_CS2HudModelArms>, m_hHudModelArms)
};