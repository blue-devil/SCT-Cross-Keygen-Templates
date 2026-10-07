#!/usr/bin/env bash
# Build SCT-SDL2 (SDL2 + Dear ImGui + libxmp-lite) for 5 targets:
#   ./build.sh                 # release (default): aarch64 x64 x86 win-x64 win-x86
#   ./build.sh win-x64         # subset
#   BUILD=debug ./build.sh     # -O0 -g, no strip, no scrub
#
# Release binaries are stripped and scrubbed (no symtab/debug/comment, no home
# paths, no GCC idents), like the sct-raylib_raygui project.
set -euo pipefail
cd "$(dirname "$0")"

echo " #######################################################################"
echo " #                                                                     #"
echo " #   -=[         SCT Cross Keygen Builder : imgui + sdl2         ]=-   #"
echo " #     _______ _______ _______                                         #"
echo " #    |    ...|    ...|    ...|  Author : Blue DeviL                   #"
echo " #    |  _____|    .__|_   .._|  E-mail : bluedevil.SCT@proton.me      #"
echo " #    | |_____|   .|    |  .|    Date   : 07/10/2026                   #"
echo " #    |_____.:|   :|    |  :|    WEB    : github.com/blue-devil        #"
echo " #     _____|:|   :|__  |  :|                                          #"
echo " #    |    .::|   .:::| |  :|  --/   Freedom, doesn't come from   \--  #"
echo " #    |_______|_______| |___|  --\_     second-hand thoughts.    _/--  #"
echo " #                                                                     #"
echo " #######################################################################"
echo ""

JOBS=$(nproc)

CWARN_C="-Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wstrict-prototypes \
         -Wmissing-prototypes -Wconversion -Wno-sign-conversion -Wundef \
         -Wcast-qual -Wwrite-strings"
CWARN_CXX="-Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wconversion \
           -Wno-sign-conversion -Wundef -Wcast-qual -Wwrite-strings \
           -Wold-style-cast -Wuseless-cast"

if [[ ${BUILD:-release} == debug ]]; then
    OPT="-O0 -g"
    STRIP=0
else
    OPT="-O2 -DNDEBUG"
    STRIP=1
fi

PKGS="sdl2 imgui_impl_sdl2 imgui_impl_opengl3"

# build_app <name> <cxx> <pkgcfg> <plat> <exe> [extra_ldflags]
build_app() {
    local name=$1 cxx=$2 pkg=$3 plat=$4 exe=$5 extra=$6
    echo "== app $name ($cxx)"
    local out=build/$name
    mkdir -p "$out"

    local prefix=""
    [[ $cxx == *-g++ ]] && prefix="${cxx%g++}"

    # package cflags used as-is: the -I paths sit inside the system prefix,
    # and marking them -isystem breaks the cross C++ stdlib (include_next).
    # imgui/SDL headers are warning-clean under our $CWARN set (imgui's
    # casts are silenced with pragmas at the include sites in our sources).
    local cflags libs
    cflags=$("$pkg" --cflags $PKGS)
    libs=$("$pkg" --static --libs $PKGS)

    # C sources (banner/xmplayer need SDL headers; assets is data only)
    local cc=${prefix}gcc
    [[ -z $prefix ]] && cc=cc
    for f in banner xmplayer assets; do
        $cc -std=c11 $OPT -fno-ident -DLIBXMP_STATIC $CWARN_C -c "src/$f.c" -o "$out/$f.o" \
            -Isrc -isystem "prebuilt/libxmp-lite/$plat/include/libxmp-lite" $cflags
    done

    # C++ sources
    for f in main ui; do
        $cxx -std=c++17 $OPT -fno-ident $CWARN_CXX -c "src/$f.cpp" -o "$out/$f.o" \
            -Isrc $cflags
    done

    # Windows: icon resource + GUI subsystem
    if [[ $exe == ".exe" ]]; then
        "${prefix}windres" src/sct.rc -O coff -o "$out/sct_res.o"
    fi

    local link=("$cxx" -s -o "$out/sct$exe" "$out"/*.o \
        "prebuilt/libxmp-lite/$plat/lib/libxmp-lite.a" $libs)
    if [[ $STRIP == 1 ]]; then
        "${link[@]}" $extra
        "${prefix}strip" --strip-all --remove-section=.comment "$out/sct$exe"
        python3 src/scrub_ident.py "$out/sct$exe" >/dev/null
    else
        "${link[@]}" $extra
    fi
    echo "   -> $out/sct$exe"
}

# Regenerate embedded assets
python3 src/assets_gen.py

TARGETS=("$@")
[[ ${#TARGETS[@]} -eq 0 ]] && TARGETS=(aarch64 x64 x86 win-x64 win-x86)

for t in "${TARGETS[@]}"; do
    case $t in
        aarch64)
            build_app aarch64 g++ pkg-config linux-aarch64 "" "-lGL -lm -ldl -lpthread" ;;
        x64)
            build_app x64 x86_64-linux-gnu-g++ x86_64-linux-gnu-pkg-config linux-x64 "" "-lGL -lm -ldl -lpthread" ;;
        x86)
            build_app x86 i686-linux-gnu-g++ i686-linux-gnu-pkg-config linux-x86 "" "-lGL -lm -ldl -lpthread" ;;
        win-x64)
            build_app win-x64 x86_64-w64-mingw32-g++ x86_64-w64-mingw32-pkg-config windows-x64 ".exe" "-static -mwindows" ;;
        win-x86)
            build_app win-x86 i686-w64-mingw32-g++ i686-w64-mingw32-pkg-config windows-x86 ".exe" "-static -mwindows" ;;
        *) echo "unknown target $t"; exit 1 ;;
    esac
done
echo "all done."
