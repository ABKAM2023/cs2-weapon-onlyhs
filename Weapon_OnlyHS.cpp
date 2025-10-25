#include <stdio.h>
#include <string.h>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstdarg>

#include "Weapon_OnlyHS.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars* gpGlobals = nullptr;
IUtilsApi* g_pUtils = nullptr;

Weapon_OnlyHS g_Weapon_OnlyHS;
PLUGIN_EXPOSE(Weapon_OnlyHS, g_Weapon_OnlyHS);

static constexpr int MAX_PLAYERS = 64;
#ifndef HITGROUP_HEAD
#define HITGROUP_HEAD 1
#endif

static bool g_debugLog = false;
static float g_centerHintCooldown = 2.0f;

static std::unordered_set<std::string> g_Weapon_OnlyHSWeapons;
static std::unordered_map<std::string, std::string> g_Phrases;

static int g_LastHP[MAX_PLAYERS] = {0};
static int g_LastArmor[MAX_PLAYERS] = {0};
static float g_LastHintTime[MAX_PLAYERS] = {0.0f};
static bool  g_SnapshotTimerRunning = false;

CGameEntitySystem* GameEntitySystem()
{
    return g_pUtils->GetCGameEntitySystem();
}

void StartupServer()
{
    g_pGameEntitySystem = GameEntitySystem();
    g_pEntitySystem = g_pUtils->GetCEntitySystem();
    gpGlobals = g_pUtils->GetCGlobalVars();
}

static inline void Dbg(const char* fmt, ...)
{
    if (!g_debugLog) return;
    char buf[1024];
    va_list va; va_start(va, fmt);
    V_vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);
    ConColorMsg(Color(120,200,255,255), "[Weapon_OnlyHS] %s\n", buf);
}

static inline std::string SanitizeWeapon(const char* w)
{
    if (!w) return {};
    std::string s(w);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)tolower(c); });
    const char* pref = "weapon_";
    if (s.rfind(pref, 0) == 0) s.erase(0, strlen(pref));
    return s;
}

static inline bool IsAliveSlot(int slot)
{
    CCSPlayerController* c = CCSPlayerController::FromSlot(slot);
    if (!c) return false;
    CCSPlayerPawn* p = c->GetPlayerPawn();
    return p && p->IsAlive();
}

static inline const char* P(const char* key, const char* fallback = "")
{
    auto it = g_Phrases.find(key);
    return (it != g_Phrases.end()) ? it->second.c_str() : fallback;
}

static void LoadConfig()
{
    g_debugLog = false;
    g_centerHintCooldown = 2.0f;
    g_Weapon_OnlyHSWeapons.clear();

    KeyValues::AutoDelete kv("Weapon_OnlyHS");
    const char* path = "addons/configs/Weapon_OnlyHS/settings.ini";

    if (!kv->LoadFromFile(g_pFullFileSystem, path))
    {
        Dbg("No config at %s (using defaults)", path);
        g_Weapon_OnlyHSWeapons.insert("deagle");
        Dbg("Config: cooldown=%.2f, weapons=%zu",
            g_centerHintCooldown, g_Weapon_OnlyHSWeapons.size());
        for (const auto& w : g_Weapon_OnlyHSWeapons)
            Dbg("  weapon[%s] = HS-only", w.c_str());
        return;
    }

    g_debugLog = kv->GetInt("debug_log", 0) != 0;
    g_centerHintCooldown  = kv->GetFloat("center_hint_cooldown", 2.0f);

    if (KeyValues* w = kv->FindKey("weapons", /*bCreate=*/false))
    {
        for (KeyValues* it = w->GetFirstSubKey(); it; it = it->GetNextKey())
        {
            if (it->GetFirstSubKey() != nullptr)
                continue;

            const char* kname = it->GetName();
            if (!kname || !*kname)
                continue;

            int en = it->GetInt(0);
            if (!en)
                continue;

            std::string name = kname;
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c){ return (char)tolower(c); });

            if (name.rfind("weapon_", 0) == 0)
                name.erase(0, 7);

            g_Weapon_OnlyHSWeapons.insert(name);
        }
    }
    else
    {
        Dbg("Section \"weapons\" not found in %s", path);
    }

    Dbg("Config: cooldown=%.2f, weapons=%zu",
        g_centerHintCooldown, g_Weapon_OnlyHSWeapons.size());
    if (g_debugLog)
        for (const auto& nm : g_Weapon_OnlyHSWeapons)
            Dbg("  weapon[%s] = HS-only", nm.c_str());
}

static void LoadPhrases()
{
    KeyValues::AutoDelete kv("Phrases");
    const char* path = "addons/translations/weapon_onlyhs.phrases.txt";
    if (!kv->LoadFromFile(g_pFullFileSystem, path))
    {
        Dbg("No phrases at %s (fallbacks)", path);
        g_Phrases.clear();
        g_Phrases["Center_Weapon_OnlyHS_Hint"] = "Headshot only for this weapon";
        return;
    }

    const char* lang = g_pUtils ? g_pUtils->GetLanguage() : "en";
    g_Phrases.clear();
    for (KeyValues* p = kv->GetFirstTrueSubKey(); p; p = p->GetNextTrueSubKey())
        g_Phrases[p->GetName()] = p->GetString(lang);
    Dbg("Phrases loaded (%zu keys)", g_Phrases.size());
}

static void StartSnapshotTimerIfNeeded()
{
    if (g_SnapshotTimerRunning || !g_pUtils) return;
    g_SnapshotTimerRunning = true;

    g_pUtils->CreateTimer(0.0f, []() -> float {
        for (int s = 0; s < MAX_PLAYERS; ++s)
        {
            CCSPlayerController* c = CCSPlayerController::FromSlot(s);
            if (!c) continue;
            CCSPlayerPawn* p = c->GetPlayerPawn();
            if (!p || !p->IsAlive()) continue;

            g_LastHP[s] = p->m_iHealth();
            g_LastArmor[s] = p->m_ArmorValue();
        }
        return 0.0f;
    });
}

static void OnMapStart(const char*)
{
    std::fill(std::begin(g_LastHP), std::end(g_LastHP), 0);
    std::fill(std::begin(g_LastArmor), std::end(g_LastArmor), 0);
    std::fill(std::begin(g_LastHintTime),std::end(g_LastHintTime), 0.0f);
    g_SnapshotTimerRunning = false;

    LoadConfig();
    LoadPhrases();
    StartSnapshotTimerIfNeeded();
}

static void OnPlayerSpawn(const char*, IGameEvent* ev, bool)
{
    if (!ev || !g_pUtils) return;
    int slot = ev->GetInt("userid", -1);
    if (slot < 0 || slot >= MAX_PLAYERS) return;

    g_pUtils->CreateTimer(0.05f, [slot]() -> float {
        CCSPlayerController* c = CCSPlayerController::FromSlot(slot);
        if (!c) return -1.0f;
        CCSPlayerPawn* p = c->GetPlayerPawn();
        if (!p) return -1.0f;
        g_LastHP[slot]    = p->m_iHealth();
        g_LastArmor[slot] = p->m_ArmorValue();
        return -1.0f;
    });
}

static void OnPlayerHurt(const char*, IGameEvent* ev, bool)
{
    if (!ev) return;

    const int victim   = ev->GetInt("userid",   -1);
    const int attacker = ev->GetInt("attacker", -1);

    if (victim < 0 || victim >= MAX_PLAYERS)   return;

    const std::string wep = SanitizeWeapon(ev->GetString("weapon", ""));
    if (wep.empty() || g_Weapon_OnlyHSWeapons.find(wep) == g_Weapon_OnlyHSWeapons.end())
        return;

    const int hitgroup = ev->GetInt("hitgroup", 0);
    if (hitgroup == HITGROUP_HEAD)
        return;

    CCSPlayerController* c = CCSPlayerController::FromSlot(victim);
    if (!c) return;
    CCSPlayerPawn* p = c->GetPlayerPawn();
    if (!p || !p->IsAlive()) return;

    const int curHP    = p->m_iHealth();
    const int curArmor = p->m_ArmorValue();

    if (curHP != g_LastHP[victim]) {
        p->m_iHealth(g_LastHP[victim]);
        if (g_pUtils) g_pUtils->SetStateChanged(p, "CBaseEntity", "m_iHealth");
    }
    if (curArmor != g_LastArmor[victim]) {
        p->m_ArmorValue(g_LastArmor[victim]);
        if (g_pUtils) g_pUtils->SetStateChanged(p, "CCSPlayerPawn", "m_ArmorValue");
    }

    if (attacker >= 0 && attacker < MAX_PLAYERS && gpGlobals && g_pUtils)
    {
        const float now = gpGlobals->curtime;
        if (now - g_LastHintTime[attacker] >= g_centerHintCooldown)
        {
            g_LastHintTime[attacker] = now;
            g_pUtils->PrintToCenter(attacker, P("Center_Weapon_OnlyHS_Hint", "Headshot only for this weapon"));
        }
    }
}

bool Weapon_OnlyHS::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();

    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY   (GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY   (GetEngineFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

    g_SMAPI->AddListener(this, this);
    return true;
}

bool Weapon_OnlyHS::Unload(char* error, size_t maxlen)
{
    if (g_pUtils) g_pUtils->ClearAllHooks(g_PLID);
    ConVar_Unregister();
    return true;
}

void Weapon_OnlyHS::AllPluginsLoaded()
{
    int ret = META_IFACE_FAILED;

    g_pUtils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED || !g_pUtils)
    {
        std::string s = "meta unload " + std::to_string(g_PLID);
        if (engine) engine->ServerCommand((s + "\n").c_str());
        return;
    }

    g_pUtils->StartupServer(g_PLID, StartupServer);

    LoadConfig();
    LoadPhrases();

    g_pUtils->HookEvent(g_PLID, "player_spawn", OnPlayerSpawn);
    g_pUtils->HookEvent(g_PLID, "player_hurt",  OnPlayerHurt);
    g_pUtils->MapStartHook(g_PLID, OnMapStart);

    StartSnapshotTimerIfNeeded();
}

const char* Weapon_OnlyHS::GetLicense()
{
	return "GPL";
}

const char* Weapon_OnlyHS::GetVersion()
{
	return "1.0";
}

const char* Weapon_OnlyHS::GetDate()
{
	return __DATE__;
}

const char *Weapon_OnlyHS::GetLogTag()
{
	return "[Weapon_OnlyHS]";
}

const char* Weapon_OnlyHS::GetAuthor()
{
	return "ABKAM";
}

const char* Weapon_OnlyHS::GetDescription()
{
	return "Weapon OnlyHS";
}

const char* Weapon_OnlyHS::GetName()
{
	return "Weapon OnlyHS";
}

const char* Weapon_OnlyHS::GetURL()
{
	return "https://discord.gg/ChYfTtrtmS";
}