# GoldenEye: Source on Source SDK 2013 (TF2 branch, 2025+)

This tree is Valve's current `source-sdk-2013` (master @ `be57e3d0`) with GoldenEye: Source 5.0 merged in.
It builds a 64-bit Linux `client.so` and `server.so` that run on the Steam app
**Source SDK Base 2013 Multiplayer** (243750).

## Layout

| Path | What |
|---|---|
| `src/game/{client,server,shared}/ges/` | GE:S game code (unchanged apart from API fixes) |
| `src/game/client/client_ges.vpc`, `src/game/server/server_ges.vpc` | VPC projects (generated from Valve's HL2MP projects + the GE:S source trees) |
| `src/vpc_scripts/ges_base.vpc`, `ges_thirdparty.vpc` | GE:S defines / Python + Boost include paths |
| `src/thirdparty/python3/` | CPython 3.4.10 headers (`linux64/pyconfig.h`) and the built-in module list |
| `src/thirdparty/boost/` | Boost 1.83 header subset (Boost.Python, format, typeof) |
| `src/lib/ges/linux64/` | Prebuilt static `python34.a` and `boost_python.a` |
| `src/buildges` | Build script (Steam Runtime / podman), like Valve's `buildallprojects` |

## Building (Linux)

Requirements: Source SDK Base 2013 Multiplayer installed, `podman`.

```bash
cd src
./buildges            # or: ./buildges debug
```

Output lands in `game/mod_ges/bin/linux64/`. Copy `client.so` and `server.so` into
`steamapps/sourcemods/gesource/bin/linux64/`.

To rebuild Python/Boost: `src/thirdparty/build_python_linux64.sh` (inside the sniper SDK container).

## Installing / running

1. `gesource/bin/linux64/client.so` + `server.so` (from the build).
2. `gesource/gameinfo.txt` must use `SteamAppId 243750` and mount the SDK 2013 `hl2` VPKs
   (see `game/mod_ges/gameinfo.txt` in this repo). The old SDK 2007 file used AppId 218.
3. Restart Steam, launch "GoldenEye: Source (v5.0)" from the library.

The engine's `hl2.sh` picks `bin/linux64/server.so` automatically and starts `hl2_linux64`.

## What changed vs. the archived GE:S repo

* Re-based from the 2015 SDK 2013 (`mp/src`, 32-bit) onto the 2025 TF2-branch SDK via a 3-way merge
  (base = Valve `0d8dceea`, ours = GE:S, theirs = Valve master). 225 files merged cleanly; 36 were
  resolved by hand (`hl2mp_player`, `c_hl2mp_player`, vgui `Panel`, activity lists, gamemovement, ...).
* `CHL2MP_Player`: kept GE:S's (Tony Sergi's) multiplayer anim-state networking, added Valve's
  NextBot hooks, script description, `GetMaxAmmo`, threat helpers, pickup cvar.
* vgui `Panel`: GE:S's `cp<offset>` "center in parent" layout kept (new flags moved to bits 19/20,
  since Valve now uses 17/18). `vgui_controls` is built with `GE_DLL` when `/ges` is passed to VPC.
* API updates: `EHANDLE::FromIndex` -> `UnsafeFromIndex`, no implicit `CBaseHandle` -> `CHandle`,
  `IViewPortPanel::GetPreferredActionSet`, mixed-type `min/max` -> `MIN/MAX`, duplicate
  `SendProxy_SendNonLocalDataTable` removed (now in `player.cpp`).
* Python: embedded CPython **3.4.10** (static), matching the 3.4 stdlib that ships in
  `gesource/python/lib`; Boost.Python 1.83 replaces the old bundled copy.
* `ge_webrequest`: on Linux, libcurl is loaded at runtime (`dlopen`) instead of statically linked.
* Music: Windows keeps FMOD; on Linux `ges/fmod_compat` implements the FMOD subset the music manager
  uses on top of [miniaudio](https://github.com/mackron/miniaudio) (vendored in `src/thirdparty/miniaudio`).
  Shader Editor is Windows-only and not built.

## Known gaps / next steps

* Windows build: VPC projects exist but Windows Python 3.4 / Boost / FMOD / curl libs are not in
  the repo yet.
* Distributable builds should be made with `./buildges` (sniper runtime). The first binaries were
  produced on a newer glibc host and need glibc >= 2.35 (fine on current distros and inside the
  Steam runtime container, which uses the host glibc when it is newer).
