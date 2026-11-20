// Big Servers -- plugins\mt_plugin_BigServers.dll. One dll, both sides (the user, 2026-11-20: "if it's on the client,
// it runs the client part; if it's on the server, it runs the server part").
//
// Client: the game's server list leaves out servers the backend marks hidden. On 2026-11-20 the one thing that set
//   "hide":2 in mt.racers.co.kr/hosts was more than 100 slots (full, passworded and modded servers all stay 0, checked
//   over 304 listed servers), so a big server could not be found or joined. Where the list is parsed, the game reads
//   the "hide" field into a bool (setne byte [rbp+..]); that store is patched to always write 0. Found by byte pattern,
//   and only patched when the string the code looks up there really is L"hide".
// Server: the json's MaxPlayers is clamped to mh.netMaxPlayerCount (100) and the engine's session keeps its own limit
//   (the ?MaxPlayers= launch option). This sets the cvar live and every GameSession's MaxPlayers to the target, so a
//   big server needs only "MaxPlayers" in DedicatedServerConfig.json.
//
// Settings: plugins\big_servers.ini
//   [General] Disabled=1
//   [Client]  ShowHidden=1
//   [Server]  MaxPlayers=0   (0: DedicatedServerConfig.json's MaxPlayers)

#include <string>
#include "../include/mt_plugin.hpp"

using namespace mt;

static const wchar_t* INI = L"big_servers.ini";

static bool onServer() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return wcsstr(exe, L"MotorTownServer") != nullptr;
}

// ---- client ----
// lea rdx,[L"hide"] | mov rcx,rbx | setne [rbp+a] (the field before) | call GetIntegerField("hide") | test eax,eax |
// lea rdx,[next field] | mov rcx,rbx | setne [rbp+b] <- hide's result, this one | call | lea rdx,.. | mov [rbp+..],eax
static const char* kHideSig = "48 8D 15 ?? ?? ?? ?? 48 8B CB 0F 95 85 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 48 8D 15 ?? ?? ?? ?? "
                              "48 8B CB 0F 95 85 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 89 85";
static void showHidden() {
    auto hits = find_sig(kHideSig);
    if (hits.size() != 1) { MT_LOG("client: the server list's hide check was not found (%d matches) -- hidden servers stay hidden", (int)hits.size()); return; }
    uintptr_t h = hits[0];
    uintptr_t str = h + 7 + get<int32_t>(h + 3);  // the first lea: the field the call after the first setne looks up
    wchar_t w[5] = {0};
    if (!api->read(str, w, 10) || wcsncmp(w, L"hide", 5) != 0) { MT_LOG("client: the field read there is not \"hide\" -- left alone"); return; }
    uint8_t b[7] = {0xC6, 0x85, 0, 0, 0, 0, 0x00};  // mov byte [rbp+disp32], 0
    memcpy(b + 2, (void*)(h + 37), 4);
    bool ok = api->patch_bytes(h + 34, b, sizeof b) != 0;
    MT_LOG("client: %s", ok ? "servers the backend hides (over 100 slots) are shown in the server list" : "PATCH FAILED");
}

// ---- server ----
static int target;
static uintptr_t cSession, fExec, ksl;
static int oMax = -1, pCmd = -1;
static int jsonMaxPlayers() {  // DedicatedServerConfig.json at the build root (Binaries\Win64\..\..\..)
    // shortcut: ignores a -DediConfig= path on the command line; read that too if someone keeps the json elsewhere.
    std::wstring p = gameDir() + L"..\\..\\..\\DedicatedServerConfig.json";
    FILE* f = _wfopen(p.c_str(), L"rb");
    if (!f) return 0;
    std::string s; char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    fclose(f);
    size_t at = s.find("\"MaxPlayers\"");
    if (at == std::string::npos) return 0;
    at = s.find(':', at);
    return at == std::string::npos ? 0 : atoi(s.c_str() + at + 1);
}
static void serverTick(void*) {
    static bool cvarSet;
    if (!cvarSet && fExec && ksl && pCmd >= 0) {
        wchar_t cmd[64];
        swprintf_s(cmd, L"mh.netMaxPlayerCount %d", target);
        unsigned char p[0x40] = {0};
        struct { const wchar_t* d; int n, m; } fs = {cmd, (int)wcslen(cmd) + 1, (int)wcslen(cmd) + 1};
        memcpy(p + pCmd, &fs, sizeof fs);
        api->process_event(ksl, fExec, p);
        cvarSet = true;
        MT_LOG("server: mh.netMaxPlayerCount %d", target);
    }
    for (int i = 0, n = api->num_objects(); i < n; i++) {
        uintptr_t o = api->object(i);
        if (!o || isCDO(o) || !api->is_a(classOf(o), cSession)) continue;
        int was = get<int>(o + oMax);
        if (was == target) continue;
        wr(o + oMax, &target, 4);
        MT_LOG("server: %s MaxPlayers %d -> %d", objName(classOf(o)).c_str(), was, target);
    }
}
static DWORD WINAPI serverRun(LPVOID) {
    if (!api->wait_engine(INFINITE)) return 0;
    cSession = api->find_object("Class", "GameSession", "/Script/Engine");
    ksl = api->find_object("KismetSystemLibrary", "Default__KismetSystemLibrary", "/Script/Engine");
    fExec = api->find_object("Function", "ExecuteConsoleCommand", "KismetSystemLibrary");
    if (cSession) oMax = off(cSession, "MaxPlayers");
    if (fExec) pCmd = off(fExec, "Command");
    if (!cSession || oMax < 0) { MT_LOG("server: GameSession.MaxPlayers not found -- nothing changed"); return 0; }
    MT_LOG("server: %d slots", target);
    while (!api->stopping()) {  // the session is made when the world loads, and again on a map change
        if (api->game_thread_ready()) api->run_on_game_thread(serverTick, nullptr);
        Sleep(5000);
    }
    return 0;
}

extern "C" __declspec(dllexport) int PluginInit(const MtApi* a) {
    if (!attach(a)) return 1;
    if (iniDisabled(INI)) return 0;
    if (onServer()) {
        target = iniInt(INI, L"Server", L"MaxPlayers", 0);
        if (target <= 0) target = jsonMaxPlayers();
        if (target <= 100) { MT_LOG("server: %d slots -- the game handles up to 100 itself, nothing to do", target); return 0; }
        CloseHandle(CreateThread(nullptr, 0, serverRun, nullptr, 0, nullptr));
    } else if (iniInt(INI, L"Client", L"ShowHidden", 1) != 0) {
        showHidden();
    }
    return 0;
}
