#!/bin/bash
# FlatOut runtime compile (subset of nfsu2-web's runtime) -> $FO_WORK/rt
cd "$(dirname "$0")"
source ./fo_env.sh
W=deps/wasi-sdk-34.0-x86_64-linux; CC=$W/bin/clang
TARGET="--target=wasm32-wasip1-threads -pthread"
FEAT="-msimd128 -mbulk-memory -mnontrapping-fptoint -mtail-call -mexception-handling -mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=false"
DEFS="-D_WASI_EMULATED_MMAN -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS"
CFLAGS="$TARGET -O2 $FEAT $DEFS -fno-delete-null-pointer-checks -include runtime/web/compat/wasm_plat.h -Iruntime/web/compat -Iruntime -Igen/all -DNFS_ENHANCED=0"
OUT=$FO_WORK/rt; mkdir -p $OUT
RT="${RT:-autotest mem dispatch genexec heap watchdog win32 win32_ext com debug res fileio vsemu lifecycle framerate dinput_pad dikeys keyboard dinput_kbd winsock module modheap enhance fo_hle fo_stubs fo_overrides}"
fail=0
for f in $RT; do src=runtime/$f.c; [ -f $src ] || continue
  [ -n "$ONLY" ] && [[ " $ONLY " != *" $f "* ]] && continue
  $CC $CFLAGS -c -o $OUT/$f.o $src 2>&1 | grep -E "error" | head -8 || true; [ -f $OUT/$f.o ] || { echo "FAILED $f"; fail=1; }
done
for f in $(cd runtime/web && ls *.c | sed 's/\.c$//'); do
  [ -n "$ONLY" ] && [[ " $ONLY " != *" web_$f "* ]] && continue
  case "$f" in thread_exit) fl="$TARGET -O2 $FEAT" ;; *) fl="$CFLAGS" ;; esac
  $CC $fl -c -o $OUT/web_$f.o runtime/web/$f.c 2>&1 | grep -E "error" | head -8 || true
done
echo RT_DONE $fail
