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

#include "networkbasetypes.pb.h"

#include "cdetour.h"
#include "addresses.h"
#include "detours.h"
#include "playermanager.h"
#include "gameconfig.h"
#include "networksystem/inetworkserializer.h"

#define VPROF_ENABLED
#include "tier0/vprof.h"

#include "tier0/memdbgon.h"

DECLARE_DETOUR(SendNetMessage, Detour_SendNetMessage);
DECLARE_DETOUR(HostStateRequest, Detour_HostStateRequest);

std::string g_sExtraAddon;
FAKE_STRING_CVAR(cs2f_extra_addon, "The workshop ID of an extra addon to mount and send to clients", g_sExtraAddon, false);

void *FASTCALL Detour_HostStateRequest(void *a1, void **pRequest)
{
	// skip if we're doing anything other than changelevel
	if (g_sExtraAddon.empty() || V_strnicmp((char *)pRequest[2], "changelevel", 11))
		return HostStateRequest(a1, pRequest);

	// This offset hasn't changed in 6 years so it should be safe
	CUtlString *sAddonString = (CUtlString*)(pRequest + 11);

	// addons are simply comma-delimited, can have any number of them
	if (!sAddonString->IsEmpty())
		sAddonString->Format("%s,%s", sAddonString->Get(), g_sExtraAddon.c_str());
	else
		sAddonString->Set(g_sExtraAddon.c_str());

	return HostStateRequest(a1, pRequest);
}

extern double g_flUniversalTime;

void FASTCALL Detour_SendNetMessage(INetChannel *pNetChan, INetworkSerializable *pNetMessage, void *pData, int a4)
{
	NetMessageInfo_t *info = pNetMessage->GetNetMessageInfo();

	// 7 for signon messages
	if (info->m_MessageId != 7 || g_sExtraAddon.empty())
		return SendNetMessage(pNetChan, pNetMessage, pData, a4);

	ClientJoinInfo_t *pPendingClient = GetPendingClient(pNetChan);

	if (pPendingClient)
	{
		Message("Detour_SendNetMessage: Sending addon %s to client %lli\n", g_sExtraAddon.c_str(), pPendingClient->steamid);
		CNETMsg_SignonState *pMsg = (CNETMsg_SignonState *)pData;
		pMsg->set_addons(g_sExtraAddon.c_str());
		pMsg->set_signon_state(SIGNONSTATE_CHANGELEVEL);
		pPendingClient->signon_timestamp = g_flUniversalTime;
	}

	SendNetMessage(pNetChan, pNetMessage, pData, a4);
}

void Detour_Log()
{
	return;
}

CDetour<decltype(Detour_Log)> g_LoggingDetours[] =
{
	CDetour<decltype(Detour_Log)>(Detour_Log, "Msg"),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?ConMsg@@YAXPEBDZZ" ),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ" ),
	CDetour<decltype(Detour_Log)>(Detour_Log, "ConDMsg"),
	CDetour<decltype(Detour_Log)>(Detour_Log, "DevMsg"),
	CDetour<decltype(Detour_Log)>(Detour_Log, "Warning"),
	CDetour<decltype(Detour_Log)>(Detour_Log, "DevWarning"),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?DevWarning@@YAXPEBDZZ" ),
	CDetour<decltype(Detour_Log)>(Detour_Log, "LoggingSystem_Log"),
	CDetour<decltype(Detour_Log)>(Detour_Log, "LoggingSystem_LogDirect"),
	CDetour<decltype(Detour_Log)>(Detour_Log, "LoggingSystem_LogAssert"),
	//CDetour<decltype(Detour_Log)>( Detour_IsChannelEnabled, "LoggingSystem_IsChannelEnabled" ),
};

CON_COMMAND_F(toggle_logs, "Toggle printing most logs and warnings", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	static bool bBlock = false;

	if (!bBlock)
	{
		Message("Logging is now OFF.\n");

		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].EnableDetour();
	}
	else
	{
		Message("Logging is now ON.\n");

		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].DisableDetour();
	}

	bBlock = !bBlock;
}

bool InitDetours(CGameConfig *gameConfig)
{
	bool success = true;

	g_vecDetours.PurgeAndDeleteElements();

	for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
	{
		if (!g_LoggingDetours[i].CreateDetour(gameConfig))
			success = false;
	}

	if (!SendNetMessage.CreateDetour(gameConfig))
		success = false;
	SendNetMessage.EnableDetour();

	if (!HostStateRequest.CreateDetour(gameConfig))
		success = false;
	HostStateRequest.EnableDetour();

	return success;
}

void FlushAllDetours()
{
	g_vecDetours.Purge();
}
