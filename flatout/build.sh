#!/bin/bash
# Builds flatout.wasm.gz (the game's code, statically recompiled to WebAssembly)
# from YOUR OWN copy of FlatOut.exe, and the web page (web/dist).
#
#   FO_GAME=/path/to/FlatOut ./flatout/build.sh
#
# Needs: wasi-sdk 34 unpacked as deps/wasi-sdk-34.0-x86_64-linux (Linux x86-64),
#        python3 with capstone and pefile, node 22+.
# Output: work/flatout.wasm.gz and web/dist.  Nothing of the game is in this repository;
# the generated code stays in work/ (git-ignored) and must not be published.
set -e
cd "$(dirname "$0")/.."
source ./fo_env.sh
: "${FO_GAME:?set FO_GAME to your FlatOut install folder (the one with FlatOut.exe and data.bfs)}"
EXE="$FO_GAME/FlatOut.exe"
W=deps/wasi-sdk-34.0-x86_64-linux
[ -x "$W/bin/clang" ] || { echo "wasi-sdk 34 is missing: unpack it to $W (https://github.com/WebAssembly/wasi-sdk/releases)"; exit 1; }
python3 -c "import capstone, pefile" || { echo "python3 needs capstone and pefile"; exit 1; }
[ -f "$EXE" ] || { echo "no $EXE"; exit 1; }

# the relocation and function lists in flatout/recomp were made for this exe (Steam/GOG FlatOut 1, 2845088 bytes)
WANT=0eb60d43cde606039a0cdb998725a0e2043edbf9580ab67b64cd782102e8d852
HAVE=$(sha256sum "$EXE" | cut -d' ' -f1)
[ "$HAVE" = "$WANT" ] || echo "WARNING: FlatOut.exe sha256 $HAVE is not the tested one ($WANT): the build may not work"

cp flatout/recomp/relocs.txt flatout/recomp/seeds.txt "$FO_WORK/recomp/"

echo "== 1/5 import argument counts"
RELOCS="$FO_WORK/recomp/relocs.txt" IMPOUT="$FO_WORK/gen/all/impargs.c" python3 tools/argcount.py "$EXE" > "$FO_WORK/gen/argcount.log"
python3 flatout/tools/fix_impargs.py "$FO_WORK/gen/all/impargs.c"

echo "== 2/5 recompiling FlatOut.exe (takes a while)"
NOLINK=1 ./fo_gen.sh
./fo_shard.sh impargs.c > /dev/null

echo "== 3/5 runtime"
./fo_build_rt.sh | tee "$FO_WORK/rt.log"; grep -q "RT_DONE 0" "$FO_WORK/rt.log" || { echo "runtime build failed (see $FO_WORK/rt.log)"; exit 1; }
CFLAGS="--target=wasm32-wasip1-threads -pthread -O2 -msimd128 -mbulk-memory -mnontrapping-fptoint -mtail-call -mexception-handling -mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=false -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS -fno-delete-null-pointer-checks -include runtime/web/compat/wasm_plat.h -Iruntime/web/compat -Iruntime -Igen/server"
for f in fntab impargs modinfo; do $W/bin/clang $CFLAGS -c -o "$FO_WORK/rt/srv_$f.o" gen/server/$f.c; done

./fo_link.sh
ls -la "$FO_WORK/flatout.wasm"

echo "== 4/5 compress"
gzip -kf -9 "$FO_WORK/flatout.wasm"

echo "== 5/5 web page"
(cd web && node fo-build.mjs)
echo "done: $FO_WORK/flatout.wasm.gz and web/dist  (./flatout/pack.sh makes the FlatOutWeb folder)"
