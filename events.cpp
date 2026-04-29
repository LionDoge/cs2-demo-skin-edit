#include "utlvector.h"
#include "igameevents.h"
#include "eventlistener.h"

#include "common.h"
#include "entity/ccsplayerpawn.h"
#include "entity/ccsplayercontroller.h"

extern IGameEventManager2* g_gameEventManager;
CUtlVector<CGameEventListener*> g_vecEventListeners;
void RegisterEventListeners()
{
	static bool bRegistered = false;

	if (bRegistered || !g_gameEventManager)
		return;

	Msg("!! [Plugin] REGISTERING GAME EVENTS !!");
	FOR_EACH_VEC(g_vecEventListeners, i)
	{
		g_gameEventManager->AddListener(g_vecEventListeners[i], g_vecEventListeners[i]->GetEventName(), true);
	}

	bRegistered = true;
}

void UnregisterEventListeners()
{
	if (!g_gameEventManager)
		return;

	FOR_EACH_VEC(g_vecEventListeners, i)
	{
		g_gameEventManager->RemoveListener(g_vecEventListeners[i]);
	}

	g_vecEventListeners.Purge();
}

bool IsWeaponCarabine(CBasePlayerWeapon* pWeapon);
// workaround ammo not resetting for all weapons.
void round_start_callback(IGameEvent*);
CGameEventListener round_start_listener(round_start_callback, "round_start");
void round_start_callback(IGameEvent* pEvent)
{
	Msg("!! [Plugin] HANDLING EVENT round_start");
	for (int i = 1; i <= MAXPLAYERS; i++)
	{
		CCSPlayerController* controllerEntity = dynamic_cast<CCSPlayerController*>(g_pEntitySystem->GetBaseEntity((CEntityIndex)i));
		if (!controllerEntity)
			continue;
		CCSPlayerPawn* pPawn = static_cast<CCSPlayerPawn*>(controllerEntity->GetPawn());
		if(!pPawn || pPawn->m_iHealth() <= 0)
			continue;
		CPlayer_WeaponServices* pWeaponServices = pPawn->m_pWeaponServices.Get();
		if(!pWeaponServices)
			continue;
		FOR_EACH_VEC(*pWeaponServices->m_hMyWeapons.Get(), j)
		{
			CCSWeaponBase* pWeapon = (CCSWeaponBase*)(pWeaponServices->m_hMyWeapons.Get()->Element(j).Get());
			if (!IsWeaponCarabine(pWeapon))
				continue;
			CCSWeaponBaseVData* vdata = pWeapon->GetWeaponVData();
			if (pWeapon)
			{
				pWeapon->m_iClip1.Set(vdata->m_iMaxClip1);
				pWeapon->m_iClip2.Set(vdata->m_nPrimaryReserveAmmoMax);
				pWeapon->m_pReserveAmmo.Get()[0] = vdata->m_nPrimaryReserveAmmoMax; 

				// Reserve ammo has to be manually networked (we're accessing this array like a pointer through this implementation)
				// yes it's not pretty.
				static constexpr auto datatable_hash = hash_32_fnv1a_const("CBasePlayerWeapon");
				static constexpr auto prop_hash = hash_32_fnv1a_const("m_pReserveAmmo");
				static const auto key = schema::GetOffset("CBasePlayerWeapon", datatable_hash, "m_pReserveAmmo", prop_hash);
				SetStateChanged((Z_CBaseEntity*)pWeapon, key.offset);
				/*variant_t iReserveAmmo(vdata->m_nPrimaryReserveAmmoMax);
				pfnAcceptInput(pWeapon, "SetReserveAmmoAmount", pWeapon, pWeapon, &iReserveAmmo, 0);*/
			}
		}
	}
}
