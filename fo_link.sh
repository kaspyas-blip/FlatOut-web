#!/bin/bash
cd "$(dirname "$0")"; source ./fo_env.sh; W=deps/wasi-sdk-34.0-x86_64-linux
OUTW=${OUTW:-$FO_WORK/flatout.wasm}
$W/bin/clang --target=wasm32-wasip1-threads -pthread -O2 -msimd128 -mbulk-memory -mnontrapping-fptoint -mtail-call -mexception-handling \
  -o $OUTW $FO_WORK/obj/*.o $FO_WORK/rt/*.o \
  -Wl,--import-memory,--export-memory,--shared-memory,--max-memory=4294967296 \
  -Wl,--initial-memory=1073741824,--global-base=671088640 -Wl,-z,stack-size=8388608 -Wl,--no-stack-first -Wl,--error-limit=0 \
  -lsetjmp -lwasi-emulated-mman -lwasi-emulated-signal -lwasi-emulated-process-clocks 2>&1
