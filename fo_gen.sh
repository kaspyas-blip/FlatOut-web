#!/bin/bash
# Lift FlatOut.exe (tools/genall.py, address-hashed shards), recompile only the
# shards whose source changed, then link $FO_WORK/flatout.wasm.
set -e
cd "$(dirname "$0")"
source ./fo_env.sh
EXE=${EXE:-$FO_GAME/FlatOut.exe}
# guest functions replaced or wrapped by runtime/fo_overrides.c (must match that file)
OV=${OV:-0x4d96c0,0x5e62a0,0x5eb890,0x53f625,0x55800a,0x558068,0x561948,0x5273a2,0x5df660,0x504de0,0x506880,0x506480}
G=$FO_WORK/gen/all
mkdir -p $FO_WORK/gen/sums
(cd $G && for f in shard_*.c fntab.c; do [ -f $FO_WORK/obj/${f%.c}.o ] && sha1sum $f; done) > $FO_WORK/gen/sums/before || true
GENOUT=$G RELOCS=$FO_WORK/recomp/relocs.txt SEEDS=$FO_WORK/recomp/seeds.txt X87PC=1 OVERRIDE=$OV HOOKS=${HOOKS:-} \
  python3 tools/genall.py $EXE 64 > $FO_WORK/gen/genall.log 2>&1
grep -E "functions  |MISSING|traps \(" $FO_WORK/gen/genall.log
CH=$(cd $G && sha1sum shard_*.c fntab.c | diff - $FO_WORK/gen/sums/before | grep "^<" | awk '{print $3}' || true)
echo "changed: $(echo $CH | wc -w)"
[ -z "$CH" ] || ./fo_shard.sh $CH > $FO_WORK/shard.log
[ -n "$NOLINK" ] || ./fo_link.sh
[ -n "$NOLINK" ] || ls -la $FO_WORK/flatout.wasm
