# Shared settings of the fo_*.sh scripts (sourced).
#   FO_WORK  build folder (generated C, objects, wasm), default ./work
#   FO_GAME  your FlatOut install folder (FlatOut.exe, data.bfs), needed to generate the code
export FO_WORK="${FO_WORK:-$PWD/work}"
export FO_GAME="${FO_GAME:-}"
mkdir -p "$FO_WORK/gen/all" "$FO_WORK/obj" "$FO_WORK/rt" "$FO_WORK/recomp"
# the scripts compile gen/all/*.c: it is the generated code in the work folder
[ -L gen/all ] || [ ! -e gen/all ] && ln -sfn "$FO_WORK/gen/all" gen/all
