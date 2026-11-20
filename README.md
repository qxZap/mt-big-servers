# Big Servers

Motor Town servers with more than 100 slots: see and join them, and host one. One dll; it runs the client part in
the game and the server part on a dedicated server (`MotorTownServer` in the exe name).

## Client: big servers in the server list

The backend's `/hosts` list marks servers `"hide":2`, and the game leaves those out of its browser. Checked over
304 listed servers on 2026-11-20: the only thing that set it was more than 100 slots. Full, passworded and modded
servers all stay `0`.

Where the list is parsed, the game reads `hide` with `GetIntegerField` and stores `!= 0` into a bool
(`setne byte [rbp+disp32]`). Big Servers patches that store to `mov byte [rbp+disp32], 0`. The site is found by a
byte pattern, and is only patched when the string the lookup uses there really is `L"hide"`; otherwise it logs and
leaves the game alone.

## Server: more than 100 slots from the json alone

`MaxPlayers` in `DedicatedServerConfig.json` is clamped to `mh.netMaxPlayerCount` (100), and the engine's session
keeps its own limit (`?MaxPlayers=` on the launch line). Big Servers sets the cvar live and every `GameSession`'s
`MaxPlayers` to the json's value, rechecked every 5 s (a new session after a map change gets it too). Tested
2026-11-20: session 100 -> 1000, listed with `max_players` 1000.

## Settings

`plugins\big_servers.ini`: `[Client] ShowHidden=1`, `[Server] MaxPlayers=0` (0 = the json's), `[General] Disabled=1`.

## Install

Needs [Plugin Loader](https://github.com/qxZap/mt-plugin-loader) (`version.dll`) in `MotorTown\Binaries\Win64\`.
Put `mt_plugin_BigServers.dll` in `MotorTown\Binaries\Win64\plugins\`, on the game, the server, or both.

## Build

`build.bat` (MSVC 2022, x64) writes `out\mt_plugin_BigServers.dll`.
