# AGENTS.md -- Big Servers

Source of truth is `D:\MTLiveMap\mt_loader\plugins\big_servers.cpp`; copy changes here by hand.

- Client patch: `kHideSig` starts at the `lea rdx,[L"hide"]`. The guard reads that first lea (`h+7+rel32(h+3)`); the
  patched store is the second `setne` at `h+34` (7 bytes, `0F 95 85 disp32` -> `C6 85 disp32 00`). The first
  build read the second lea and refused to patch (2026-11-20).
- After a game update: if the pattern stops matching, re-find it from the `L"hide"` string's xref (the field reads
  of the host-list parser sit together: is_dedi, hide, fps, modded).
- Server: `GameSession.MaxPlayers` (MTGameSession in this game) and `mh.netMaxPlayerCount` through
  `ExecuteConsoleCommand` (not a cheat cvar). The json is read from the build root; a `-DediConfig=` path is not.
- Untested: more than 100 real players at once.
