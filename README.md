# FlatOut in the browser

A fan project that runs the PC game *FlatOut* (Bugbear, 2004) in Google Chrome. The game's x86 code is
translated to C ahead of time (static recompilation) and compiled to WebAssembly; Direct3D 9 runs on WebGPU,
DirectSound on Web Audio, DirectInput on the browser's keyboard and gamepad API, and LAN play on a virtual LAN
of the local server. You bring your own copy of the game.

> Not affiliated with or endorsed by Bugbear Entertainment or THQ Nordic. **This repository contains none of
> the game's code or data.** The recompiled module is made on your machine from your own `FlatOut.exe` and is
> never committed (`.gitignore`); please do not publish it or the game files.

This is a port of [nfsu2-web](docs/NFSU2-README.md) (MIT), the same approach for *Need for Speed: Underground 2*:
its recompiler (`tools/`) and its Windows/Direct3D/DirectSound runtime (`runtime/`, `web/`) do the heavy lifting.
The FlatOut-specific parts are listed below.

## How it works

1. **Recompile (on your machine).** The recompiler in `tools/` disassembles your `FlatOut.exe` (capstone, pefile),
   finds its functions from seeds and pointer slots (`flatout/recomp/`; FlatOut.exe ships without relocations, so
   `flatout/tools/synth_relocs.py` rebuilds them) and emits one C function per guest function into `gen/all/`.
   wasi-sdk compiles that C into `work/flatout.wasm.gz`. None of this output is in the repository.
2. **Run the Windows the game expects.** `runtime/` is a small Windows implementation in C that the recompiled
   code calls instead of the real DLLs: memory, files, threads, the Win32 calls FlatOut needs, Direct3D 9
   (translated to WebGPU with WGSL shaders in `runtime/web/`), DirectSound (Web Audio), DirectInput (keyboard and
   gamepad) and Winsock (a virtual LAN).
3. **Host it in the browser.** `web/` is the page (TypeScript in `web/src`) that loads the module in Chrome, and the
   local server (`web/fo-server.mjs`, plus the Rust server in `web/server-rs` inherited from nfsu2-web) that serves
   the page, streams `FlatOut.exe` and the `*.bfs` archives from your own game folder, and switches LAN packets
   between browsers.
4. **Pack it.** `flatout/pack.sh` puts the page, the module and `start.bat`/`stop.bat` into `work/FlatOutWeb`,
   which you copy next to your `FlatOut.exe`.

## Repository layout

| Folder | What is in it |
|---|---|
| `flatout/` | FlatOut-specific build and pack scripts, recompiler inputs (`recomp/`) and tools (`tools/`) |
| `tools/` | the static recompiler (x86 → C) and helper scripts (texture/music packs, diagnostics) |
| `runtime/` | the Windows/Direct3D/DirectSound/DirectInput/Winsock runtime the recompiled code links against |
| `web/` | the browser page, the local servers (Node.js and Rust), page tests and deployment files |
| `gen/server/` | small stub tables for the server build (the full generated code `gen/all/` is never committed) |
| `tests/` | test scripts for the runtime, the web page, networking and free roam |
| `docs/` | design notes, the player's guide and VPS hosting (written for nfsu2-web, mostly apply here too) |
| `win/roam_asi/` | the free-roam ASI mod for native NFSU2, inherited from nfsu2-web |
| `discord-bot/` | the support bot for the project's Discord forums, inherited from nfsu2-web |
| `app/`, `deps/` | app registry defaults; the place for the wasi-sdk toolchain (not committed) |

## What you need

- **The game:** your own PC copy of FlatOut 1 (tested with the Steam version): `FlatOut.exe` 2,845,088 bytes,
  SHA-256 `0eb60d43cde606039a0cdb998725a0e2043edbf9580ab67b64cd782102e8d852`, and `data.bfs`.
- **To build:** Linux x86-64 (or WSL 2), Python 3 with `capstone` and `pefile`, Node.js 22+, and
  [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases) 34 unpacked as `deps/wasi-sdk-34.0-x86_64-linux`.
- **To play:** a recent Google Chrome (WebGPU) on Windows, and Node.js to run the small local server.

## Build

```sh
pip install capstone pefile
FO_GAME="/path/to/FlatOut" ./flatout/build.sh     # recompiles FlatOut.exe -> work/flatout.wasm.gz, builds web/dist
./flatout/pack.sh                                 # -> work/FlatOutWeb
```

Copy `work/FlatOutWeb` into your FlatOut folder (next to `FlatOut.exe`) and run `start.bat`: it starts the
server (`server.mjs`) and opens `http://localhost:8080/`. The server reads the game files from the folder one
level up and serves only `FlatOut.exe`, `*.bfs` and `filesystem.ini`. `stop.bat` stops it (or type `stop` in its
window). Saves live in the browser (Saves > Download backup).

The game picks its language from the browser's (Czech, English, German, French, Italian, Spanish, Polish,
Hungarian); `?env=FO_LANGID=0x409` on the page address overrides it (development server only).

### LAN play

Everyone who opens the page from the **same** server sees each other in the game's LAN menu (`start-lan.bat`
listens on the network). Browsers allow WebGPU and shared memory only on `https://` or `localhost`, so friends
need an HTTPS tunnel to your machine. The server sends them the game data: play only with friends who own the
game, and do not publish the address.

## What is FlatOut-specific

| Where | What |
|---|---|
| `flatout/tools/synth_relocs.py` | FlatOut.exe has no relocations: rebuilds the pointer slots and function seeds (`flatout/recomp/`) from a disassembly catalog |
| `flatout/tools/fix_impargs.py` | real stdcall argument counts of imports the heuristic gets wrong (a wrong count shifts the guest stack) |
| `flatout/tools/bfs.py` | reader for the game's `bfs1` archives (for inspection) |
| `runtime/fo_hle.c` | Win32 the game needs beyond NFSU2: launcher dialogs, keyboard hook, locale, file mapping, winmm/acm, DirectSound entry points |
| `runtime/fo_overrides.c` | native replacements for MMX/CRT memcpy (computed jumps), plus diagnostic wrappers |
| `runtime/winsock.c` | overlapped `WSASendTo`/`WSARecvFrom` and WSA events for FlatOut's LAN code |
| `runtime/com.c` | `DrawPrimitive` (sky, clouds), FVF declarations, 16-bit render targets, diagnostics (`FO_*` env) |
| `runtime/web/vsemu_wgsl.inc` | `a0`-relative constant rows in matrix instructions (vertex skinning) |
| `web/fo-server.mjs`, `web/fo-build.mjs` | local server with virtual LAN; page build without esbuild |
| `fo_*.sh` | build scripts (`FO_WORK`, `FO_GAME`) |

## Known limits

- The intro videos are not played.
- LAN multiplayer is experimental.

## License

MIT, see [LICENSE](LICENSE). FlatOut is a trademark of its owners; this project only contains code written for it.
