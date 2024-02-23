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

#include "cs2fixes.h"
#include "common.h"
#include "detours.h"
#include "playermanager.h"
#include "gameconfig.h"
#include "serversideclient.h"
#include "te.pb.h"
#include "cs_gameevents.pb.h"

CS2Fixes g_CS2Fixes;

INetworkGameServer* g_pNetworkGameServer = nullptr;
CGameConfig* g_GameConfig = nullptr;
IVEngineServer2* g_pEngineServer2 = nullptr;

double g_flUniversalTime;

PLUGIN_EXPOSE(CS2Fixes, g_CS2Fixes);
bool CS2Fixes::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer2, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	CBufferStringGrowable<256> gamedirpath;
	g_pEngineServer2->GetGameDir(gamedirpath);

	std::string gamedirname = CGameConfig::GetDirectoryName(gamedirpath.Get());

	const char* gamedataPath = "addons/cs2fixes/gamedata/cs2fixes.games.txt";
	Message("Loading %s for game: %s\n", gamedataPath, gamedirname.c_str());

	g_GameConfig = new CGameConfig(gamedirname, gamedataPath);
	char conf_error[255] = "";
	if (!g_GameConfig->Init(g_pFullFileSystem, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlen, "Could not read %s: %s", g_GameConfig->GetPath().c_str(), conf_error);
		Panic("%s\n", error);
		return false;
	}

	bool bRequiredInitLoaded = true;

	if (!addresses::Initialize(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitDetours(g_GameConfig))
		bRequiredInitLoaded = false;

	return true;
}

void CS2Fixes::Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	g_ClientsPendingAddon.RemoveAll();
}

CUtlVector<CServerSideClient*>* GetClientList()
{
	if (!g_pNetworkGameServer)
		return nullptr;

	static int offset = g_GameConfig->GetOffset("CNetworkGameServer_ClientList");
	return (CUtlVector<CServerSideClient*> *)(&g_pNetworkGameServer[offset]);
}

CServerSideClient* GetClientBySlot(CPlayerSlot slot)
{
	CUtlVector<CServerSideClient*>* pClients = GetClientList();

	if (!pClients)
		return nullptr;

	return pClients->Element(slot.Get());
}

extern std::string g_sExtraAddon;

float g_flRejoinTimeout;
FAKE_FLOAT_CVAR(cs2f_extra_addon_timeout, "How long until clients are timed out in between connects for the extra addon", g_flRejoinTimeout, 15.f, false);

bool CS2Fixes::Hook_ClientConnect(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason)
{
	CServerSideClient* pClient = GetClientBySlot(slot);

	// We don't have an extra addon set so do nothing here
	if (g_sExtraAddon.empty())
		RETURN_META_VALUE(MRES_IGNORED, true);

	Message("Client %lli", xuid);

	// Store the client's ID temporarily as they will get reconnected once the extra addon is sent
	// This gets checked for in SendNetMessage so we don't repeatedly send the changelevel signon state
	// The only caveat to this is that there's no way for us to verify if the client has actually downloaded the extra addon,
	// since they're fully disconnected while downloading it, so the best we can do is use a timeout interval
	int index;
	ClientJoinInfo_t* pPendingClient = GetPendingClient(xuid, index);

	if (!pPendingClient)
	{
		// Client joined for the first time or after a timeout
		Msg(" will reconnect for addon\n");
		AddPendingClient(xuid);
	}
	else if ((g_flUniversalTime - pPendingClient->signon_timestamp) < g_flRejoinTimeout)
	{
		// Client reconnected within the timeout interval
		// If they already have the addon this happens almost instantly after receiving the signon message with the addon
		Msg(" has reconnected within the interval, allowing\n");
		g_ClientsPendingAddon.FastRemove(index);
	}
	else
	{
		Msg(" has reconnected after the timeout or did not receive the addon message, will send addon message again\n");
	}

	RETURN_META_VALUE(MRES_IGNORED, true);
	return true;
}

const char *CS2Fixes::GetLicense()
{
	return "GPL v3 License";
}

const char *CS2Fixes::GetVersion()
{
	return "1.5";
}

const char *CS2Fixes::GetDate()
{
	return __DATE__;
}

const char *CS2Fixes::GetLogTag()
{
	return "CS2Fixes";
}

const char *CS2Fixes::GetAuthor()
{
	return "xen, Poggu, and the Source2ZE community";
}

const char *CS2Fixes::GetDescription()
{
	return "A bunch of experiments thrown together into one big mess of a plugin.";
}

const char *CS2Fixes::GetName()
{
	return "CS2Fixes";
}

const char *CS2Fixes::GetURL()
{
	return "https://github.com/Source2ZE/CS2Fixes";
}
