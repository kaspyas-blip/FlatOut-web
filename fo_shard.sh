#!/bin/bash
# compile the given gen/all sources into $FO_WORK/obj (2 at a time)
cd "$(dirname "$0")"; source ./fo_env.sh; W=deps/wasi-sdk-34.0-x86_64-linux
export CC=$W/bin/clang CFLAGS="--target=wasm32-wasip1-threads -pthread -O2 -msimd128 -mbulk-memory -mnontrapping-fptoint -mtail-call -mexception-handling -mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=false -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS -fno-delete-null-pointer-checks -include runtime/web/compat/wasm_plat.h -Iruntime/web/compat -Iruntime -Igen/all -DNFSU2_GUEST_OBJ -Wno-unused-label -Wno-parentheses-equality"
printf "%s\n" "$@" | xargs -P 2 -I{} sh -c '$CC $CFLAGS -c -o $FO_WORK/obj/$(basename {} .c).o gen/all/{} 2>&1 | grep error | head -3; echo "built {}"'
