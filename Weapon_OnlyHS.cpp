#include <stdio.h>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstdarg>
#include <cctype>

#include "Weapon_OnlyHS.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Weapon_OnlyHS g_Weapon_OnlyHS;
PLUGIN_EXPOSE(Weapon_OnlyHS, g_Weapon_OnlyHS);

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars* gpGlobals = nullptr;

IUtilsApi* g_pUtils = nullptr;

static constexpr int MAX_PLAYERS = 64;
#ifndef HITGROUP_HEAD
#define HITGROUP_HEAD 1
#endif

static bool g_debugLog = false;
static float g_centerHintCooldown = 2.0f;
static std::unordered_set<std::string> g_GlobalHSWeapons;
static std::unordered_map<std::string, std::unordered_map<std::string,bool>> g_MapOverrides;
static std::unordered_map<std::string,bool> g_MapInherit;
static std::unordered_set<std::string> g_ActiveHSWeapons;
static std::unordered_map<std::string,std::string> g_Phrases;
static int g_LastHP[MAX_PLAYERS] = {0};
static int g_LastArmor[MAX_PLAYERS] = {0};
static float g_LastHintTime[MAX_PLAYERS] = {0.0f};
static bool g_SnapshotTimerRunning = false;
static std::string g_LastMapKey;

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

static inline std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static inline void TrimInPlace(std::string& s)
{
    auto ns = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), ns));
    s.erase(std::find_if(s.rbegin(), s.rend(), ns).base(), s.end());
}

static inline std::string NormalizeMapToken(const char* mapname)
{
    if (!mapname) return {};
    std::string s(mapname);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    TrimInPlace(s);
    size_t pos = s.find_last_of("/\\"); if (pos != std::string::npos) s = s.substr(pos + 1);
    pos = s.find_last_of('.'); if (pos != std::string::npos) s = s.substr(0, pos);
    return s;
}

static inline std::string GetCurrentMapFromGlobals()
{
    char tmp[256] = {0};
    if (g_pUtils && g_pUtils->GetCGlobalVars()) g_SMAPI->Format(tmp, sizeof(tmp), "%s", g_pUtils->GetCGlobalVars()->mapname);
    return NormalizeMapToken(tmp);
}

static inline std::string NormalizeWeapon(const char* w)
{
    if (!w) return {};
    std::string s = ToLower(w);
    if (s.rfind("weapon_", 0) == 0) s.erase(0, 7);
    if (s == "usps" || s == "usp" || s == "usp_s" || s == "usp-s" || s == "usp silencer") s = "usp_silencer";
    if (s == "m4a1s" || s == "m4a1-s" || s == "m4a1 s" || s == "m4a1_s") s = "m4a1_silencer";
    if (s == "five-seven" || s == "five_seven") s = "fiveseven";
    if (s == "cz75") s = "cz75a";
    return s;
}

static inline const char* P(const char* key, const char* fallback = "")
{
    auto it = g_Phrases.find(key);
    return it != g_Phrases.end() ? it->second.c_str() : fallback;
}

static void RebuildActiveWeapons(const char*)
{
    std::string mapKey = GetCurrentMapFromGlobals();
    bool inherit = true;
    auto itInh = g_MapInherit.find(mapKey);
    if (itInh != g_MapInherit.end()) inherit = itInh->second;
    if (inherit) g_ActiveHSWeapons = g_GlobalHSWeapons; else g_ActiveHSWeapons.clear();
    auto it = g_MapOverrides.find(mapKey);
    if (it != g_MapOverrides.end())
        for (const auto& kv : it->second)
            if (kv.second) g_ActiveHSWeapons.insert(kv.first); else g_ActiveHSWeapons.erase(kv.first);
    g_LastMapKey = mapKey;
    if (g_debugLog)
    {
        Dbg("Active weapons for map '%s' (inherit=%d): %zu", mapKey.c_str(), inherit ? 1 : 0, g_ActiveHSWeapons.size());
        for (const auto& w : g_ActiveHSWeapons) Dbg("  [+] %s", w.c_str());
    }
}

static void LoadConfig()
{
    g_debugLog = false;
    g_centerHintCooldown = 2.0f;
    g_GlobalHSWeapons.clear();
    g_MapOverrides.clear();
    g_MapInherit.clear();

    KeyValues::AutoDelete kv("Weapon_OnlyHS");
    const char* path = "addons/configs/Weapon_OnlyHS/settings.ini";
    if (!kv->LoadFromFile(g_pFullFileSystem, path))
    {
        g_GlobalHSWeapons.insert("deagle");
        return;
    }

    g_debugLog = kv->GetInt("debug_log", 0) != 0;
    g_centerHintCooldown = kv->GetFloat("center_hint_cooldown", 2.0f);

    if (KeyValues* w = kv->FindKey("weapons", false))
        for (KeyValues* it = w->GetFirstSubKey(); it; it = it->GetNextKey())
        {
            if (it->GetFirstSubKey() != nullptr) continue;
            const char* kname = it->GetName(); if (!kname || !*kname) continue;
            if (!it->GetInt(0)) continue;
            std::string name = NormalizeWeapon(kname);
            if (!name.empty()) g_GlobalHSWeapons.insert(name);
        }

    if (KeyValues* maps = kv->FindKey("maps", false))
        for (KeyValues* m = maps->GetFirstTrueSubKey(); m; m = m->GetNextTrueSubKey())
        {
            const char* mapName = m->GetName(); if (!mapName || !*mapName) continue;
            std::string mapKey = NormalizeMapToken(mapName);
            bool inherit = m->GetInt("inherit", 1) != 0;
            g_MapInherit[mapKey] = inherit;

            if (KeyValues* mw = m->FindKey("weapons", false))
                for (KeyValues* it = mw->GetFirstSubKey(); it; it = it->GetNextKey())
                {
                    if (it->GetFirstSubKey() != nullptr) continue;
                    const char* wname = it->GetName(); if (!wname || !*wname) continue;
                    int en = it->GetInt(0);
                    std::string w = NormalizeWeapon(wname);
                    if (!w.empty()) g_MapOverrides[mapKey][w] = en != 0;
                }

            for (KeyValues* it = m->GetFirstSubKey(); it; it = it->GetNextKey())
            {
                if (!V_stricmp(it->GetName(), "inherit") || !V_stricmp(it->GetName(), "weapons")) continue;
                if (it->GetFirstSubKey() != nullptr) continue;
                const char* wname = it->GetName(); if (!wname || !*wname) continue;
                int en = it->GetInt(0);
                std::string w = NormalizeWeapon(wname);
                if (!w.empty()) g_MapOverrides[mapKey][w] = en != 0;
            }
        }
}

static void LoadPhrases()
{
    KeyValues::AutoDelete kv("Phrases");
    const char* path = "addons/translations/weapon_onlyhs.phrases.txt";
    if (!kv->LoadFromFile(g_pFullFileSystem, path))
    {
        g_Phrases.clear();
        g_Phrases["Center_Weapon_OnlyHS_Hint"] = "Headshot only for this weapon";
        return;
    }
    const char* lang = g_pUtils ? g_pUtils->GetLanguage() : "en";
    g_Phrases.clear();
    for (KeyValues* p = kv->GetFirstTrueSubKey(); p; p = p->GetNextTrueSubKey()) g_Phrases[p->GetName()] = p->GetString(lang);
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
    std::fill(std::begin(g_LastHintTime), std::end(g_LastHintTime), 0.0f);
    g_SnapshotTimerRunning = false;
    LoadConfig();
    LoadPhrases();
    std::string now = GetCurrentMapFromGlobals();
    Dbg("MapStart: gpGlobals='%s'", now.c_str());
    RebuildActiveWeapons(nullptr);
    StartSnapshotTimerIfNeeded();
}

static void OnRoundStart(const char*, IGameEvent*, bool)
{
    std::string now = GetCurrentMapFromGlobals();
    if (now != g_LastMapKey)
    {
        Dbg("round_start: detected map change '%s' -> '%s'", g_LastMapKey.c_str(), now.c_str());
        RebuildActiveWeapons(nullptr);
    }
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
        g_LastHP[slot] = p->m_iHealth();
        g_LastArmor[slot] = p->m_ArmorValue();
        return -1.0f;
    });
}

static void OnPlayerHurt(const char*, IGameEvent* ev, bool)
{
    if (!ev) return;
    int victim = ev->GetInt("userid", -1);
    int attacker = ev->GetInt("attacker", -1);
    if (victim < 0 || victim >= MAX_PLAYERS) return;

    const char* raw = ev->GetString("weapon", "");
    std::string wep = NormalizeWeapon(raw);
    if (g_debugLog) Dbg("hurt weapon='%s' norm='%s'", raw, wep.c_str());
    if (wep.empty() || g_ActiveHSWeapons.find(wep) == g_ActiveHSWeapons.end()) return;

    int hitgroup = ev->GetInt("hitgroup", 0);
    if (hitgroup == HITGROUP_HEAD) return;

    CCSPlayerController* c = CCSPlayerController::FromSlot(victim);
    if (!c) return;
    CCSPlayerPawn* p = c->GetPlayerPawn();
    if (!p || !p->IsAlive()) return;

    int curHP = p->m_iHealth();
    int curArmor = p->m_ArmorValue();

    if (curHP != g_LastHP[victim])
    {
        p->m_iHealth(g_LastHP[victim]);
        if (g_pUtils) g_pUtils->SetStateChanged(p, "CBaseEntity", "m_iHealth");
    }
    if (curArmor != g_LastArmor[victim])
    {
        p->m_ArmorValue(g_LastArmor[victim]);
        if (g_pUtils) g_pUtils->SetStateChanged(p, "CCSPlayerPawn", "m_ArmorValue");
    }

    if (attacker >= 0 && attacker < MAX_PLAYERS && gpGlobals && g_pUtils)
    {
        float now = gpGlobals->curtime;
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
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
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
    g_pUtils->HookEvent(g_PLID, "player_hurt", OnPlayerHurt);
    g_pUtils->HookEvent(g_PLID, "round_start", OnRoundStart);
    g_pUtils->MapStartHook(g_PLID, OnMapStart);

    g_ActiveHSWeapons = g_GlobalHSWeapons;
    StartSnapshotTimerIfNeeded();
}

const char* Weapon_OnlyHS::GetLicense()
{
    return "GPL";
}

const char* Weapon_OnlyHS::GetVersion()
{
    return "1.1";
}

const char* Weapon_OnlyHS::GetDate()
{
    return __DATE__;
}

const char* Weapon_OnlyHS::GetLogTag()
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
