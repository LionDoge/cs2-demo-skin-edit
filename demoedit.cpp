#include "demoedit.h"
#include "iserver.h"
#include "icvar.h"
#include "convar.h"
#include "entitysystem.h"
#include "module.h"
#include "utils/plat.h"
#include "dbg.h"
#include "engine/igameeventsystem.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbaseviewmodel.h"
#include "common.h"
#include "schemasystem/schemasystem.h"
#include "steam/steam_api.h"

#define VPROF_ENABLED
#include "vprof.h"
#include "tier0/memdbgon.h"
#include "funchook.h"
#include "binutils.h"
#include "enginetoclient.h"
#include "steam/isteamfriends.h"
#include "ctimer.h"
#include <optional>
#include <map>

double g_flUniversalTime = 0.0;
float g_flLastTickedTime = 0.0f;

DemoEditPlugin g_DemoEditPlugin;

std::map<uint16_t, int32_t> skinReplacementMap{};

IGameEventSystem *g_gameEventSystem = nullptr;
IGameEventManager2 *g_gameEventManager = nullptr;
INetworkGameServer *g_pNetworkGameServer = nullptr;
INetworkClientService* g_pNetworkClService = nullptr;
INetworkGameClient *g_pNetworkGameClient = nullptr;
CSchemaSystem *g_pSchemaSystem2 = nullptr;
CGlobalVars *gpGlobals = nullptr;
CSteamAPIContext g_steamAPI;
IVEngineServer2 *g_pEngineServer2 = nullptr;
CGameEntitySystem* g_pEntitySystem = nullptr;
ICvar* icvar = nullptr;
IServerGameClients* gameclients = nullptr;
IServerGameDLL* server = nullptr;
IToolFramework2* g_pToolFramework = nullptr;
ISource2GameClients* g_pGameClients = nullptr;
ISource2EngineToClient* g_pEngineToClient = nullptr;

#define CREATE_FUNCHOOK_BASIC(funchookHandle, originalFunction, hookedFunction) \
	funchookHandle = funchook_create(); \
	funchook_prepare(funchookHandle, (void**)(&originalFunction), reinterpret_cast<void*>(hookedFunction)); \
	funchook_install(funchookHandle, 0);

class GameSessionConfiguration_t { };
SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const*, int, uint64);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
SH_DECL_HOOK2(IGameEventManager2, LoadEventsFromFile, SH_NOATTRIB, 0, int, const char*, bool);
SH_DECL_HOOK3(IGameEventManager2, AddListener, SH_NOATTRIB, 0, bool, IGameEventListener2*, const char*, bool);
SH_DECL_MANUALHOOK1_void(HK_FrameStageNotify, 36, 0, 0, int);
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, 
	const uint64*, INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t);
SH_DECL_MANUALHOOK2(HK_FireEventClientSide, 8, 0, 0, bool, IGameEvent*, bool)
SH_DECL_MANUALHOOK1(HK_SetGlobals, 11, 0, 0, void*, void*)
SH_DECL_HOOK1_void(CEntityInstance, PostDataUpdate, SH_NOATTRIB, 0, int)
// offset sourced from: https://github.com/advancedfx/advancedfx/blob/9cba9eaa5cc73202e9c9841ab886dc796294a4a6/AfxHookSource2/ClientEntitySystem.cpp#L370
SH_DECL_MANUALHOOK2(HK_OnAddEntity, 15, 0, 0, void*, CEntityInstance*, uint32_t)

CGameEntitySystem* GameEntitySystem()
{
#ifdef PLATFORM_WINDOWS
	static int offset = 88;
#else
	static int offset = 80;
#endif
	return *reinterpret_cast<CGameEntitySystem**>((uintptr_t)(g_pGameResourceServiceServer)+offset);
}

typedef void (__fastcall* AttributeAdd_t)(void* attributeList, const char* attribName, float val);
typedef CEntityInstance* (__fastcall* GetEntityFromIndex_t)(void* pEntityList, int index);
CModule* g_clientModule;
CModule* g_serverModule;
CModule* g_engine2Module;
void** g_pEntityList;
GetEntityFromIndex_t g_pfnGetEntityFromIndex;
AttributeAdd_t g_pfnAttributeAdd;

typedef void* (__fastcall* GetAvatarImage_t)(void* csAvatarImage);
typedef const char* (__fastcall* GetTeamName_t)(void* ccsteam);
//typedef void (__fastcall* GetPlayerName_t)(void* ths, char* name, unsigned int maxLength, byte flags);
typedef void(__fastcall* SetModel_t)(C_BaseModelEntity* ths, const char* modelName);
typedef bool(__fastcall* FireEventClientSide_t)(IGameEvent* pEvent, bool bDontBroadcast);
typedef const char* (__fastcall* GetPlayerName_t)(CCSPlayerController* ths);
typedef void(__fastcall* AddOrSetAttribByName_t)(CAttributeList* pAttributeList, const char* attribName, float val);
typedef bool(__fastcall* LoadPaintKit_t)(void* skinData, KeyValues* paintKitKv, void* unk);
typedef bool(__fastcall* LoadStickerKit_t)(void* skinData, KeyValues* stickerKitKv, void* unk, void* unk2);
typedef bool(__fastcall* AddKeychain_t)(void* itemView, C_EconItemView* pItemView, bool unk);
typedef void* (__fastcall* OnAddEntity_t)(void* This, CEntityInstance* pInstance, uint32_t handle);
typedef void* (__fastcall *fn_SetGlobals)(void*, void*);
typedef void* (__fastcall *fn_SetMeshGroupMask)(CGameSceneNode* node, uint32_t mask);
funchook_t *g_fHookGetAvatarImage = nullptr;
funchook_t* g_fHookGetTeamName = nullptr;
funchook_t* g_fHookGetPlayerName = nullptr;
funchook_t* g_fHookSetModel = nullptr;
funchook_t* g_fHookFireEventClientSide = nullptr;
funchook_t* g_fHookLoadPaintKit = nullptr;
funchook_t* g_fHookLoadStickerKit = nullptr;
funchook_t* g_fHookAddKeychain = nullptr;
GetAvatarImage_t g_pfnGetAvatarImage = nullptr;
GetTeamName_t g_pfnGetTeamName = nullptr;
GetPlayerName_t g_pfnGetPlayerName = nullptr;
SetModel_t g_pfnSetModel = nullptr;
FireEventClientSide_t g_pfnFireEventClientSide = nullptr;
AddOrSetAttribByName_t g_pfnAddOrSetItemAttrib = nullptr;
LoadPaintKit_t g_pfnLoadPaintKit = nullptr;
AddKeychain_t g_pfnAddKeychain = nullptr;
LoadStickerKit_t g_pfnLoadStickerKit = nullptr;
fn_SetMeshGroupMask g_pfnSetMeshGroupMask = nullptr;

fn_SetGlobals orig_pfnSetGlobals = nullptr;

void* __fastcall Hook_SetGlobals(void* globals)
{
	gpGlobals = (CGlobalVars*)globals;
	RETURN_META_VALUE(MRES_IGNORED, nullptr);
}

const char* __fastcall Hook_GetTeamName(void* ccsteam)
{
	const char* name = g_pfnGetTeamName(ccsteam);
	return "";
}

std::map<int32_t, bool> g_mapLegacyModelSkins;
bool __fastcall Hook_LoadPaintKit(void* skinData, KeyValues* paintKitKv, void* unk)
{
	int defindex = *(int*)skinData;
	//Msg("loadpaintkit: %d\n", defindex);
	g_mapLegacyModelSkins[defindex] = paintKitKv->GetInt("use_legacy_model", 0);
	g_pfnLoadPaintKit(skinData, paintKitKv, unk);
	return true;
}

// replaces all stickers with sugarface boris sticker
bool __fastcall Hook_LoadStickerKit(void* skinData, KeyValues* stickerKitKv, void* unk, void* unk2)
{
	int defindex = *(int*)skinData;
	//Msg("loadpaintkit: %d\n", defindex);
	auto name = stickerKitKv->GetString("name", "");
	if(V_stristr(name, "graffiti") == NULL && V_stristr(name, "spray") == NULL)
		stickerKitKv->SetString("sticker_material", "sugarface_capsule/boris");
	g_pfnLoadStickerKit(skinData, stickerKitKv, unk, unk2);
	return true;
}

CEconItemAttribute* FindAttributeByIndex(CUtlVector<CEconItemAttribute>& attrs, uint16_t idx)
{
	FOR_EACH_VEC(attrs, i)
	{
		if (attrs[i].m_iAttributeDefinitionIndex == idx)
		{
			return &attrs[i];
		}
	}
	return nullptr;
}

CON_COMMAND_F(addskin, "", FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 3)
	{
		Msg("Usage: addskin <weaponId> <defindex>");
		return;
	}

	int weaponIdx = V_atoi(args[1]);
	int defindex = V_atoi(args[2]);

	Msg("addskin: weaponIdx=%d, defindex=%d\n", weaponIdx, defindex);

	skinReplacementMap[weaponIdx] = defindex;
}

CON_COMMAND_F(remskin, "", FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: remskin <weaponId>");
		return;
	}

	int weaponIdx = V_atoi(args[1]);
	skinReplacementMap.erase(weaponIdx);
}

CON_COMMAND_F(addkeychain, "", FCVAR_LINKED_CONCOMMAND)
{
	auto plr = (CCSPlayerController*)g_pfnGetEntityFromIndex(*g_pEntityList, 1);
	if(!plr)
	{
		Msg("No player found!\n");
		return;
	}
	auto pawnHandle = plr->m_hPlayerPawn.Get();
	auto pawn = (C_CSPlayerPawn*)g_pfnGetEntityFromIndex(*g_pEntityList, pawnHandle.GetEntryIndex());
	if (!pawn)
	{
		Msg("No pawn found!\n");
		return;
	}
	auto wepServices = pawn->m_pWeaponServices.Get();
	if (!wepServices)
	{
		Msg("No weapon services found!\n");
		return;
	}
	auto wepHandle = wepServices->m_hActiveWeapon.Get();
	auto wep = (C_CSWeaponBase*)g_pfnGetEntityFromIndex(*g_pEntityList, wepHandle.GetEntryIndex());
	if (!wep)
	{
		Msg("No weapon found!\n");
		return;
	}
	g_pfnAddKeychain(wep, wep->m_AttributeManager->m_Item(), false);
}

CON_COMMAND_F(changekeychainid, "", FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 2)
	{
		return;
	}
	int newId = V_atoi(args[1]);
	auto plr = (CCSPlayerController*)g_pfnGetEntityFromIndex(*g_pEntityList, 1);
	if (!plr)
	{
		Msg("No player found!\n");
		return;
	}
	auto pawnHandle = plr->m_hPlayerPawn.Get();
	auto pawn = (C_CSPlayerPawn*)g_pfnGetEntityFromIndex(*g_pEntityList, pawnHandle.GetEntryIndex());
	if (!pawn)
	{
		Msg("No pawn found!\n");
		return;
	}
	auto wepServices = pawn->m_pWeaponServices.Get();
	if (!wepServices)
	{
		Msg("No weapon services found!\n");
		return;
	}
	auto wepHandle = wepServices->m_hActiveWeapon.Get();
	auto wep = (C_CSWeaponBase*)g_pfnGetEntityFromIndex(*g_pEntityList, wepHandle.GetEntryIndex());
	if (!wep)
	{
		Msg("No weapon found!\n");
		return;
	}
	auto attrsList = wep->m_AttributeManager->m_Item()->m_NetworkedDynamicAttributes.Get();
	g_pfnAddOrSetItemAttrib(attrsList, "keychain slot 0 id", newId);
	g_pfnAddOrSetItemAttrib(attrsList, "keychain slot 0 offset x", 0);
	g_pfnAddOrSetItemAttrib(attrsList, "keychain slot 0 offset y", 0);
	g_pfnAddOrSetItemAttrib(attrsList, "keychain slot 0 offset z", 0);
}

bool g_nameSwapEnabled = false;
int g_iTargetPlayerId = 1;
bool g_bDontRemoveAttributes = true;
FAKE_BOOL_CVAR(d_enablenameswap, "cheat_demoedit", g_nameSwapEnabled, false, FCVAR_NONE);
FAKE_INT_CVAR(d_targetplayerid, "cheat_demoedit", g_iTargetPlayerId, 1, FCVAR_NONE);
FAKE_BOOL_CVAR(d_dontremoveattrs, "cheat_demoedit", g_bDontRemoveAttributes, true, FCVAR_NONE);

std::optional<CUtlVector<CHandle<C_BasePlayerWeapon>>*> GetWeaponsOfPlayer(CCSPlayerController* plr)
{
	CHandle<C_CSPlayerPawn> pawnHandle = plr->m_hPlayerPawn;
	C_CSPlayerPawn* pawn = (C_CSPlayerPawn*)g_pfnGetEntityFromIndex(*g_pEntityList, pawnHandle.GetEntryIndex());
	if (!pawn)
	{
		return std::nullopt;
	}
	CCSPlayer_WeaponServices* wepServices = (CCSPlayer_WeaponServices*)(pawn->m_pWeaponServices.Get());
	if (!wepServices)
	{
		//Msg("WeaponServices is null for player %s\n", plr->m_sSanitizedPlayerName.Get().Get());
		return std::nullopt;
	}
	return std::make_optional(wepServices->m_hMyWeapons.Get());
}

void ApplyTargetPlayerSkins()
{
	if (!g_pfnGetEntityFromIndex || !g_pEntityList)
	{
		Msg("WARN: Can't apply skins, entity list not ready!\n");
		return;
	}
	CCSPlayerController* plr = (CCSPlayerController*)g_pfnGetEntityFromIndex(*g_pEntityList, g_iTargetPlayerId);
	if (plr)
	{
		int plrTeam = plr->m_iTeamNum();
		CHandle<C_CSPlayerPawn> pawnHandle = plr->m_hPlayerPawn;
		C_CSPlayerPawn* pawn = (C_CSPlayerPawn*)g_pfnGetEntityFromIndex(*g_pEntityList, pawnHandle.GetEntryIndex());
		if (!pawn)
		{
			return;
		}
		CCSPlayer_WeaponServices* wepServices = (CCSPlayer_WeaponServices*)(pawn->m_pWeaponServices.Get());
		if (!wepServices)
		{
			//Msg("WeaponServices is null for player %s\n", plr->m_sSanitizedPlayerName.Get().Get());
			return;
		}
		auto wepList = wepServices->m_hMyWeapons();
		auto activeWepHandle = wepServices->m_hActiveWeapon.Get();

		uint16_t activeDefIndex = 0;
		auto activeWep = (C_CSWeaponBase*)g_pfnGetEntityFromIndex(*g_pEntityList, activeWepHandle.GetEntryIndex());
		if (activeWep)
		{
			C_AttributeContainer* activeAttribContainer = activeWep->m_AttributeManager();
			if (activeAttribContainer)
			{
				C_EconItemView* activeWepView = activeAttribContainer->m_Item();
				if (activeWepView)
				{
					activeDefIndex = activeWepView->m_iItemDefinitionIndex();
				}
			}
		}
		//CUtlVector<CHandle<C_BasePlayerWeapon>>* wepList = (CUtlVector<CHandle<C_BasePlayerWeapon>>*)((unsigned char*)wepServices + 0x40);
		bool isActiveWeaponLegacy = false;
		FOR_EACH_VEC(*wepList, i)
		{
			C_CSWeaponBase* currentWep = (C_CSWeaponBase*)g_pfnGetEntityFromIndex(*g_pEntityList, (wepList)->Element(i).GetEntryIndex());
			if (!currentWep) {
				return;
			}

			C_AttributeContainer* attribContainer = currentWep->m_AttributeManager();
			if (!attribContainer)
			{
				return;
			}
			C_EconItemView* pWeaponItemView = attribContainer->m_Item();
			if (!pWeaponItemView)
			{
				return;
			}
			// set name tag to null
			char* name = currentWep->m_AttributeManager->m_Item()->m_szCustomName;
			*name = '\0';

			uint16_t defindex = pWeaponItemView->m_iItemDefinitionIndex();
			if ((defindex >= 43 && defindex <= 50)) // ignore bomb and nades.
				continue;

			// Only modify our weapons.
			uint32_t xuidLow = currentWep->m_OriginalOwnerXuidLow();
			uint32_t xuidHigh = currentWep->m_OriginalOwnerXuidHigh();
			uint64_t xuid = ((uint64_t)xuidLow) | ((uint64_t)xuidHigh << 32);
			//Msg("%d\n", xuid);
			if (xuid != plr->m_steamID())
				continue;

			int32_t newPaintKit = -1;
			if (skinReplacementMap.contains(defindex))
			{
				auto sceneNode = currentWep->m_CBodyComponent()->m_pSceneNode();
				if (sceneNode)
				{
					if (g_mapLegacyModelSkins.contains(newPaintKit))
					{
						if (defindex == activeDefIndex)
						{
							isActiveWeaponLegacy = g_mapLegacyModelSkins[newPaintKit];
						}
						CSkeletonInstance* skel = (CSkeletonInstance*)sceneNode;
						skel->m_modelState().m_MeshGroupMask() = g_mapLegacyModelSkins[newPaintKit] ? 1 : 2;
						g_pfnSetMeshGroupMask(sceneNode, g_mapLegacyModelSkins[newPaintKit] ? 1 : 2);
					}
				}
				newPaintKit = skinReplacementMap[defindex];
				currentWep->m_nFallbackPaintKit() = newPaintKit;
				currentWep->m_flFallbackWear() = 0.1f;
				currentWep->m_nFallbackSeed() = 0;
			}

			if (defindex < 500 && !g_bDontRemoveAttributes) { // not a knife (likely)
				pWeaponItemView->m_iEntityQuality() = 0;
			}
			currentWep->m_fFlags() |= EF_IS_PRE_SPAWN;
			pWeaponItemView->m_bIsStoreItem() = true;
			pWeaponItemView->m_iItemIDLow() = -1;
			pWeaponItemView->m_iItemIDHigh() = -1;

			auto attrs = pWeaponItemView->m_NetworkedDynamicAttributes.Get()->m_Attributes.Get();
			if (defindex < 500)
			{
				FOR_EACH_VEC_BACK(*attrs, k)
				{
					auto attr = attrs->Element(k);
					// from items_game.txt
					// stickers stuff is between these ids
					// removing stattrak "kill eater" will cause crashes when someone gets a kill with that weapon.
					if (attr.m_iAttributeDefinitionIndex == 6
						|| attr.m_iAttributeDefinitionIndex == 7
						|| attr.m_iAttributeDefinitionIndex == 8) // paint kit stuff
					{
						continue;
					}
					if (
						!(attr.m_iAttributeDefinitionIndex >= 80 && attr.m_iAttributeDefinitionIndex <= 89))
					{
						attrs->Remove(k);
					}
				}
			}
			if (!g_bDontRemoveAttributes)
				pWeaponItemView->m_NetworkedDynamicAttributes()->m_Attributes.Get()->RemoveAll();

			if (newPaintKit != -1)
			{
				g_pfnAddOrSetItemAttrib(pWeaponItemView->m_NetworkedDynamicAttributes.Get(), "set item texture prefab", newPaintKit);
				g_pfnAddOrSetItemAttrib(pWeaponItemView->m_NetworkedDynamicAttributes.Get(), "set item texture seed", 0.0f);
				g_pfnAddOrSetItemAttrib(pWeaponItemView->m_NetworkedDynamicAttributes.Get(), "set item texture wear", 0.1f);

				
			}

			pWeaponItemView->m_bDisallowSOC() = false;
			pWeaponItemView->m_bInitialized() = true;
		}

		// get vm arms
		auto viewModelHandle = pawn->m_hHudModelArms.Get();
		auto viewModel = g_pfnGetEntityFromIndex(*g_pEntityList, viewModelHandle.GetEntryIndex());
		if (viewModel)
		{
			auto sceneNode = ((C_BaseViewModel*)viewModel)->m_CBodyComponent()->m_pSceneNode();
			if (sceneNode)
			{
				auto childSceneNode = sceneNode->m_pChild();
				while (childSceneNode)
				{
					auto ownerOfChild = (C_BaseEntity*)childSceneNode->m_pOwner();
					if (ownerOfChild)
					{
						auto ownerOfViewModelHandle = ownerOfChild->m_hOwnerEntity();
						auto ownerOfViewModel = g_pfnGetEntityFromIndex(*g_pEntityList, ownerOfViewModelHandle.GetEntryIndex());
						//if (ownerOfViewModel == activeWep)
						{
							CSkeletonInstance* skel = (CSkeletonInstance*)sceneNode;
							skel->m_modelState().m_MeshGroupMask() = isActiveWeaponLegacy ? 1 : 2;
							g_pfnSetMeshGroupMask(sceneNode, isActiveWeaponLegacy ? 1 : 2);
						}
					}

					childSceneNode = childSceneNode->m_pNextSibling();
				}
			}
		}
		
		//C_EconItemView* gloves = pawn->m_EconGloves();
		//if (gloves != nullptr)
		//{
		//	gloves->m_iEntityQuality() = 0;
		//	gloves->m_iEntityLevel() = 1;
		//	gloves->m_iItemDefinitionIndex() = 5028; // default t gloves.
		//	gloves->m_iItemIDHigh() = (16384 & 0xFFFFFFFF) >> 32;
		//	gloves->m_iItemIDLow() = 16384 & 0xFFFFFFFF;
		//	gloves->m_iAccountID() = 76561198130701252;
		//	gloves->m_bInitialized() = true;
		//	pawn->m_bNeedToReApplyGloves() = true;
		//}
		
	}
}

void RemoveAllWeaponStickersForPlayer(CCSPlayerController* plr)
{
	if (!g_pfnGetEntityFromIndex || !g_pEntityList)
	{
		Msg("WARN: entity list not ready!\n");
		return;
	}

	auto wepListOpt = GetWeaponsOfPlayer(plr);
	if (!wepListOpt.has_value())
		return;
	auto wepList = wepListOpt.value();
	FOR_EACH_VEC(*wepList, j)
	{
		auto currentWep = (C_CSWeaponBase*)g_pfnGetEntityFromIndex(*g_pEntityList, (wepList)->Element(j).GetEntryIndex());
		if (!currentWep)
			continue;
		C_AttributeContainer* attribContainer = currentWep->m_AttributeManager();
		if (!attribContainer)
			continue;
		C_EconItemView* pWeaponItemView = attribContainer->m_Item();
		if (!pWeaponItemView)
			continue;
		auto attrs = pWeaponItemView->m_NetworkedDynamicAttributes.Get()->m_Attributes.Get();
		if (pWeaponItemView->m_iItemDefinitionIndex() < 500)
		{
			FOR_EACH_VEC_BACK(*attrs, k)
			{
				auto attr = attrs->Element(k);
				// from items_game.txt
				// stickers stuff is between these ids
				// removing stattrak "kill eater" will cause crashes when someone gets a kill with that weapon.
				if (
					!(attr.m_iAttributeDefinitionIndex >= 80 && attr.m_iAttributeDefinitionIndex <= 89))
				{
					attr.m_flValue = 0;
				}
			}
		}
	}
}

void Hook_PostDataUpdate(int updateType)
{
	ApplyTargetPlayerSkins();
	RETURN_META(MRES_IGNORED);
}

void* Hook_OnAddEntity(CEntityInstance* ent, uint32_t handle)
{
	if (ent && ent->GetEntityIndex() == g_iTargetPlayerId)
	{
		Msg("Hooked target player entity instance created!\n");
		SH_ADD_HOOK(CEntityInstance, PostDataUpdate, ent, Hook_PostDataUpdate, false);
	}
	RETURN_META_VALUE(MRES_IGNORED, nullptr);
}

static uint64 g_iFlagsToRemove = (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY);
static constexpr const char* pUnCheatCvars[] = { "bot_stop", "bot_freeze", "bot_zombie" };
static constexpr const char* pUnCheatCmds[] = { "report_entities", "endround" };

void DemoEditPlugin::Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
	INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	static void (IGameEventSystem:: * PostEventAbstract)(CSplitScreenSlot, bool, int, const uint64*,
		INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t) = &IGameEventSystem::PostEventAbstract;

	NetMessageInfo_t* pInfo = pEvent->GetNetMessageInfo();
	NetworkMessageId msgId = pInfo->m_MessageId;
	if (msgId == 118 || msgId == 110 || msgId == 111 || msgId == 124)
	{
		*(uint64_t*)clients = 0;
		RETURN_META(MRES_HANDLED);
	}
}

void UnlockConVars()
{
	if (!g_pCVar)
		return;

	int iUnhiddenConVars = 0;

	for (ConVarRefAbstract ref(ConVarRef((uint16)0)); ref.IsValidRef(); ref = ConVarRefAbstract(ConVarRef(ref.GetAccessIndex() + 1)))
	{
		for (int i = 0; i < sizeof(pUnCheatCvars) / sizeof(*pUnCheatCvars); i++)
			if (!V_strcmp(ref.GetName(), pUnCheatCvars[i]))
				ref.RemoveFlags(FCVAR_CHEAT);

		if (!ref.IsFlagSet(g_iFlagsToRemove))
			continue;

		ref.RemoveFlags(g_iFlagsToRemove);
		iUnhiddenConVars++;
	}

	Msg("Removed hidden flags from %d convars\n", iUnhiddenConVars);
}

void UnlockConCommands()
{
	if (!g_pCVar)
		return;

	int iUnhiddenConCommands = 0;

	ConCommandData* data = g_pCVar->GetConCommandData(ConCommandRef());
	for (ConCommandRef ref = ConCommandRef((uint16)0); ref.GetRawData() != data; ref = ConCommandRef(ref.GetAccessIndex() + 1))
	{
		for (int i = 0; i < sizeof(pUnCheatCmds) / sizeof(*pUnCheatCmds); i++)
			if (!V_strcmp(ref.GetName(), pUnCheatCmds[i]))
				ref.RemoveFlags(FCVAR_CHEAT);

		if (!ref.IsFlagSet(g_iFlagsToRemove))
			continue;

		ref.RemoveFlags(g_iFlagsToRemove);
		iUnhiddenConCommands++;
	}

	Msg("Removed hidden flags from %d commands\n", iUnhiddenConCommands);
}
bool g_namesApplied = false;
void ReplacePlayerName(int slot, const char* newName);
void ApplyPlayerNamesSequential(int skipId);

constexpr auto KNIFE_MODEL_T = "weapons/models/knife/knife_default_t/weapon_knife_default_t.vmdl";
constexpr auto KNIFE_MODEL_CT = "weapons/models/knife/knife_default_ct/weapon_knife_default_ct.vmdl";

enum ClientFrameStage_t : int
{
	FRAME_UNDEFINED = -1,
	FRAME_START,
	FRAME_NET_UPDATE_START,
	FRAME_NET_UPDATE_POSTDATAUPDATE_START,
	FRAME_NET_UPDATE_POSTDATAUPDATE_END,
	FRAME_NET_UPDATE_END,
	FRAME_RENDER_START,
	FRAME_RENDER_END
};

CON_COMMAND_F(hook_postdataupd, "", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	auto plr = g_pfnGetEntityFromIndex(*g_pEntityList, g_iTargetPlayerId);
	if (plr) 
	{
		SH_ADD_HOOK(CEntityInstance, PostDataUpdate, plr, Hook_PostDataUpdate, false);
	}
}

void* Hook_LevelInitPreEntity(void* unk1, void* unk2)
{
	RETURN_META_VALUE(MRES_IGNORED, nullptr);
}

struct EntityListIterator {
	int index = -1;

	bool IsValid() const {
		return 0 < index;
	}

	int GetIndex() const {
		return index;
	}
};

bool DemoEditPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	SteamAPI_Init();
	// Required to get the IMetamodListener events
	g_SMAPI->AddListener(this, this);
	g_clientModule = new CModule("/csgo/bin/win64/", "client");
	g_serverModule = new CModule("/csgo/bin/win64/", "server");
	g_engine2Module = new CModule("/bin/win64/", "engine2");
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer2, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineToClient, ISource2EngineToClient, SOURCE2ENGINETOCLIENT_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSource2Client, ISource2Client, SOURCE2CLIENT_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);

	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &DemoEditPlugin::Hook_PostEvent), false);
	//SH_ADD_MANUALHOOK(HK_LevelInitPreEntity, g_pSource2Client, Hook_LevelInitPreEntity, false);

	// Hook GlobalVars to get global vars
	SH_ADD_MANUALHOOK(HK_SetGlobals, g_pSource2Client, Hook_SetGlobals, false);

	// Find FireEventClientSide and hook.
	/*void** gameEvetManagerVtbl = (void**)g_clientModule->FindVirtualTable("CGameEventManager");
	SH_ADD_MANUALDVPHOOK(HK_FireEventClientSide, gameEvetManagerVtbl, Hook_FireEventClientSide, false);*/

	int err;
	// search SFUI_WinPanel_Team_Win_Team in client, look for the function that sets the team name dialog var.
	/*const byte sig_getTeamName[] = "\x40\x53\x48\x83\xEC\x20\x48\x8D\x99\xC4\x08\x00\x00";
	g_pfnGetTeamName = (GetTeamName_t)(g_clientModule->FindSignature(sig_getTeamName, sizeof(sig_getTeamName) - 1, err));
	CREATE_FUNCHOOK_BASIC(g_fHookGetTeamName, g_pfnGetTeamName, Hook_GetTeamName);*/

	const byte sig_setOrAddAtrib[] = "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x55\x57\x41\x55\x41\x56\x41\x57\x48\x8B\xEC";
	g_pfnAddOrSetItemAttrib = (AddOrSetAttribByName_t)(g_clientModule->FindSignature(sig_setOrAddAtrib, sizeof(sig_setOrAddAtrib) - 1, err));
	if (err == SIG_NOT_FOUND)
	{
		Msg("[DemoEdit] !!!! SedOrAddAttribute failed to find signature!\n");
		return false;
	}
	else if (err == SIG_FOUND_MULTIPLE)
	{
		Msg("[DemoEdit] !!!! SedOrAddAttribute found multiple signatures, this may end up crashing!\n");
	}

	const byte sig_loadPaintKit[] = "\x48\x89\x5C\x24\x10\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x50\x49\x8B\x40\x08";
	g_pfnLoadPaintKit = (LoadPaintKit_t)(g_clientModule->FindSignature(sig_loadPaintKit, sizeof(sig_loadPaintKit) - 1, err));
	if (err == SIG_NOT_FOUND)
	{
		Msg("[DemoEdit] !!!! LoadPaintKit failed to find signature!\n");
		return false;
	}
	else if (err == SIG_FOUND_MULTIPLE)
	{
		Msg("[DemoEdit] !!!! LoadPaintKit found multiple signatures, this may end up crashing!\n");
	}
	CREATE_FUNCHOOK_BASIC(g_fHookLoadPaintKit, g_pfnLoadPaintKit, Hook_LoadPaintKit);

	const byte sig_setMeshGroupMask[] = "\x48\x89\x5C\x24\x2A\x48\x89\x74\x24\x2A\x57\x48\x83\xEC\x2A\x48\x8D\x99\x2A\x2A\x2A\x2A\x48\x8B\x71";
	g_pfnSetMeshGroupMask = (fn_SetMeshGroupMask)(g_clientModule->FindSignature(sig_setMeshGroupMask, sizeof(sig_setMeshGroupMask) - 1, err));
	if (err == SIG_NOT_FOUND)
	{
		Msg("[DemoEdit] !!!! SetMeshGroupMask failed to find signature!\n");
		return false;
	}
	else if (err == SIG_FOUND_MULTIPLE)
	{
		Msg("[DemoEdit] !!!! SetMeshGroupMask found multiple signatures, this may end up crashing!\n");
	}

	// https://github.com/advancedfx/advancedfx/blob/9cba9eaa5cc73202e9c9841ab886dc796294a4a6/AfxHookSource2/main.cpp#L1157
	{
		Section* textSection = g_clientModule->GetSection(".text");
		Afx::BinUtils::MemRange textRange = Afx::BinUtils::MemRange::FromSize((size_t)textSection->m_pBase, textSection->m_iSize);
		auto unkFn = Afx::BinUtils::FindPatternString(textRange, "40 55 53 48 8d ac 24 ?? ?? ?? ?? 48 81 ec ?? ?? ?? ?? 48 8b 0d ?? ?? ?? ?? 33 d2 e8 ?? ?? ?? ??");
		if (!unkFn.IsEmpty()) {
			g_pEntityList = (void **)(unkFn.Start+18+7+*(int*)(unkFn.Start+18+3));
			void* pFnGetHighestEntityIterator = (void*)(unkFn.Start + 27 + 5 + *(int*)(unkFn.Start + 27 + 1));

			// see near "no such entity %d\n" called with pEntityList and uint
			// or near "Format: ent_find_index <index>\n" called only with uint and there's pEntityList inside with uint
			auto fnGetEntityFromIndexMem = Afx::BinUtils::FindPatternString(textRange, "4c 8d 49 10 81 fa fe 7f 00 00");
			if (!fnGetEntityFromIndexMem.IsEmpty()) {
				g_pfnGetEntityFromIndex = (GetEntityFromIndex_t)(fnGetEntityFromIndexMem.Start);
			}

		}
		else {
			Msg("[DemoEdit] Failed to find pattern required for entitylist!\n");
			return false;
		}
	}

	if (g_pEntityList && *g_pEntityList)
	{
		void** vtable = **(void****)g_pEntityList;
		SH_ADD_MANUALDVPHOOK(HK_OnAddEntity, vtable, Hook_OnAddEntity, true);
	}

	UnlockConCommands();
	UnlockConVars();
	META_CONVAR_REGISTER(FCVAR_RELEASE | FCVAR_CLIENTDLL | FCVAR_SPONLY);
	return true;
}
CON_COMMAND_F(getplayers, "get players", FCVAR_CLIENTDLL | FCVAR_LINKED_CONCOMMAND)
{
	if(!g_pEntityList || !g_pfnGetEntityFromIndex)
	{
		Msg("Entity list not found\n");
		return;
	}

	for (int i = 1; i <= 64; i++)
	{
		CEntityInstance* ent = g_pfnGetEntityFromIndex(*g_pEntityList, i);
		CCSPlayerController* plr = dynamic_cast<CCSPlayerController*>(ent);
		if (plr != nullptr && !plr->m_bIsHLTV)
		{
			Msg("Player %d: %s | steamid: %llu\n", i, plr->m_sSanitizedPlayerName.Get().Get(), plr->m_steamID.Get());
			CHandle<C_CSPlayerPawn> pawnHandle = plr->m_hPlayerPawn();
			CEntityInstance* pawnEnt = (CEntityInstance*)(g_pfnGetEntityFromIndex(*g_pEntityList, pawnHandle.GetEntryIndex()));
			C_CSPlayerPawn* pawn = dynamic_cast<C_CSPlayerPawn*>(pawnEnt);
			if (!pawn)
				continue;
			CCSPlayer_WeaponServices* wepServices = (CCSPlayer_WeaponServices*)(pawn->m_pWeaponServices.Get());
			if (!wepServices)
			{
				Msg("\twepServices null!");
				continue;
			}
			auto wepList = wepServices->m_hMyWeapons();
			FOR_EACH_VEC(*wepList, i) 
			{
				auto wepHandle = (*wepList)[i];
				C_BasePlayerWeapon* wep = (C_BasePlayerWeapon*)(g_pfnGetEntityFromIndex(*g_pEntityList, wepHandle.GetEntryIndex()));
				if (!wep)
				{
					Msg("\twep null!");
					continue;
				}

				C_EconItemView* itemView = wep->m_AttributeManager->m_Item();
				if (!itemView)
				{
					Msg("\titemView null!");
					continue;
				}
				uint64_t xuid = ((uint64_t)wep->m_OriginalOwnerXuidLow()) | ((uint64_t)wep->m_OriginalOwnerXuidHigh() << 32);
				Msg("\tWep %d: defindex: %d | paintkit: %d ownerXuid: %d\n", i, itemView->m_iItemDefinitionIndex(), wep->m_nFallbackPaintKit(), xuid);
				auto attrs = itemView->m_NetworkedDynamicAttributes.Get()->m_Attributes.Get();
				FOR_EACH_VEC(*attrs, i) {
					auto attr = attrs->Element(i);
					Msg("\t\tAttr: %d, val %f\n", attr.m_iAttributeDefinitionIndex, attr.m_flValue);
				}
			}
		}
	}
}

CON_COMMAND_F(getents, "get ents", FCVAR_CLIENTDLL | FCVAR_LINKED_CONCOMMAND)
{
	if (!g_pEntityList || !g_pfnGetEntityFromIndex)
	{
		Msg("Entity list not found\n");
		return;
	}

	for (int i = 1; i <= 1600; i++)
	{
		CEntityInstance* ent = g_pfnGetEntityFromIndex(*g_pEntityList, i);
		if (ent)
		{
			Msg("%d: class %s\n", i, ent->m_pEntity->GetClassname());
		}
	}
}

CON_COMMAND_F(steam_api_init, "steamapi test", FCVAR_SPONLY)
{
	g_steamAPI.Init();
	Msg("SteamAPI initialized\n");
};

CON_COMMAND_F(steam_api_test, "steamapi test", FCVAR_SPONLY)
{
	if(args.ArgC() < 3)
	{
		Msg("Usage: steam_api_test <key> <value>\n");
		return;
	}
	
	char* key = new char[V_strlen(args[1])];
	V_strcpy(key, args[1]);
	char* val = new char[V_strlen(args[2])];
	V_strcpy(val, args[2]);
	ISteamFriends* friends = g_steamAPI.SteamFriends();
	friends->SetRichPresence(key, val);
};


void DemoEditPlugin::Hook_ClientPutInServer(CPlayerSlot slot, char const* pszName, int type, uint64 xuid)
{
	Msg("ClientPutInServer: %s xuid: %ull\n", pszName, xuid);
}

bool DemoEditPlugin::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &DemoEditPlugin::Hook_StartupServer), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &DemoEditPlugin::Hook_ClientPutInServer), true);
	ConVar_Unregister();
	funchook_uninstall(g_fHookLoadPaintKit, 0);
	return true;
}

void DemoEditPlugin::Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	g_pNetworkGameServer = g_pNetworkServerService->GetIGameServer();
	//gpGlobals = g_pNetworkGameServer->GetGlobals();
	g_pEntitySystem = GameEntitySystem();
}

bool DemoEditPlugin::Hook_AddListener(IGameEventListener2* listener, const char* name, bool bServerSide)
{
	META_CONPRINTF("GameEventManager2: AddListener: %s, serverSide: %d\n", name, bServerSide);

	RETURN_META_VALUE(MRES_SUPERCEDE, 0);
}

int DemoEditPlugin::Hook_LoadEventsFromFile(const char* filename, bool bSearchAll)
{
	ExecuteOnce(g_gameEventManager = META_IFACEPTR(IGameEventManager2));

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

//void DemoEditPlugin::Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
//{
//	//VPROF_ENTER_SCOPE(__FUNCTION__);
//	/**
//	 * simulating:
//	 * ***********
//	 * true  | game is ticking
//	 * false | game is not ticking
//	 */
//
//	if (simulating && g_bHasTicked)
//	{
//		g_flUniversalTime += gpGlobals->curtime - g_flLastTickedTime;
//	}
//
//	g_flLastTickedTime = gpGlobals->curtime;
//	g_bHasTicked = true;
//
//	//VPROF_EXIT_SCOPE();
//}
PLUGIN_EXPOSE(DemoEditPlugin, g_DemoEditPlugin);

bool DemoEditPlugin::Pause(char *error, size_t maxlen)
{
	return true;
}

bool DemoEditPlugin::Unpause(char *error, size_t maxlen)
{
	return true;
}

void DemoEditPlugin::AllPluginsLoaded()
{
	Msg("[DemoEdit] All plugins loaded\n");
}

const char *DemoEditPlugin::GetLicense()
{
	return "GPL v3 License";
}

const char *DemoEditPlugin::GetVersion()
{
	return "1.0";
}

const char *DemoEditPlugin::GetDate()
{
	return __DATE__;
}

const char *DemoEditPlugin::GetLogTag()
{
	return "MultiWeaponPlugin";
}

const char *DemoEditPlugin::GetAuthor()
{
	return "Lion Doge";
}

const char *DemoEditPlugin::GetDescription()
{
	return "Edit demo skins";
}

const char *DemoEditPlugin::GetName()
{
	return "DemoEdit";
}

const char *DemoEditPlugin::GetURL()
{
	return "";
}
