#!/usr/bin/env bash
# Build SCT (static raylib + libxmp-lite) for 5 targets:
#   ./build.sh aarch64 x64 x86 win-x64 win-x86
set -euo pipefail
cd "$(dirname "$0")"

echo " #######################################################################"
echo " #                                                                     #"
echo " #   -=[       SCT Cross Keygen Builder : raylib + raygui        ]=-   #"
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


RAYLIB_SRC="$PWD/third_party/raylib"
JOBS=$(nproc)

# write_toolchain <file> <system_name> <processor> <compiler> <root> [rc]
write_toolchain() {
    cat > "$1" <<EOF
set(CMAKE_SYSTEM_NAME $2)
set(CMAKE_SYSTEM_PROCESSOR $3)
set(CMAKE_C_COMPILER $4)
set(CMAKE_CXX_COMPILER ${4%%gcc}g++)
${6:+set(CMAKE_RC_COMPILER $6)}
set(CMAKE_FIND_ROOT_PATH $5)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
EOF
}

build_raylib() { # name [toolchain]
    local name=$1 tc=${2:-} tcabs=""
    [[ -n $tc ]] && tcabs="$PWD/$tc"
    local b=build/raylib-$name
    if [[ -f $b/done ]]; then echo "== raylib $name (cached)"; return; fi
    echo "== raylib $name"
    rm -rf "$b" && mkdir -p "$b"
    (cd "$b" && cmake "$RAYLIB_SRC" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-fno-ident \
        ${tcabs:+-DCMAKE_TOOLCHAIN_FILE="$tcabs"} \
        -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_GAMES=OFF \
        >/dev/null && cmake --build . -j"$JOBS" >/dev/null)
    touch "$b/done"
}

build_app() { # name cc plat syslibs exe
    local name=$1 cc=$2 plat=$3 syslibs=$4 exe=$5
    echo "== app $name ($cc)"
    local out=build/$name lib raylib_a
    mkdir -p "$out"
    raylib_a=$(find build/raylib-$name -name 'libraylib.a' | head -1)
    [[ -n $raylib_a ]] || { echo "libraylib.a not found for $name"; exit 1; }
    # Strict warnings for our sources; third_party is -isystem so warnings
    # inside raylib.h/raygui.h are not reported against our code.
    for f in main banner xmplayer sct_textbox assets; do
        "$cc" -std=c11 -O2 -fno-ident -DLIBXMP_STATIC $CWARN -c "src/$f.c" -o "$out/$f.o" \
            -Isrc -isystem third_party -isystem third_party/raylib/src \
            -isystem "prebuilt/libxmp-lite/$plat/include/libxmp-lite"
    done
    # Binutils prefix: "x86_64-w64-mingw32-gcc" -> "x86_64-w64-mingw32-", "cc" -> "".
    local prefix=""
    [[ $cc == *-gcc ]] && prefix="${cc%gcc}"
    # Windows: compile src/sct.rc (GLFW_ICON) so title bar, taskbar and Explorer
    # show the icon. The .o lands in $out and is linked by the *.o glob below.
    if [[ $exe == ".exe" ]]; then
        "${prefix}windres" src/sct.rc -O coff -o "$out/sct_res.o"
    fi
    # -s strips at link time; the target's own strip (host strip can't read x86
    # ELF or PE) then drops anything left, incl. the GCC version note.
    "$cc" -s -o "$out/sct$exe" "$out"/*.o \
        "prebuilt/libxmp-lite/$plat/lib/libxmp-lite.a" "$raylib_a" $syslibs
    "${prefix}strip" --strip-all --remove-section=.comment "$out/sct$exe"
    # mingw crt/libgcc "GCC: (GNU) x.y.z" idents get merged into .rdata, where
    # strip can't reach them; blank them in place (unreferenced bytes).
    python3 src/scrub_ident.py "$out/sct$exe" >/dev/null
    echo "   -> $out/sct$exe"
}

CWARN="-Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wstrict-prototypes \
       -Wmissing-prototypes -Wconversion -Wno-sign-conversion -Wundef \
       -Wcast-qual -Wwrite-strings"

LINUX_LIBS="-lGL -lX11 -lXrandr -lXinerama -lXcursor -lXi -ldl -lpthread -lrt -lm"
# -mwindows: GUI subsystem, so Explorer launches don't open a console window
WINDOWS_LIBS="-mwindows -lopengl32 -lgdi32 -lwinmm -lws2_32"

TARGETS=("$@")
[[ ${#TARGETS[@]} -eq 0 ]] && TARGETS=(aarch64 x64 x86 win-x64 win-x86)

# Regenerate embedded assets
python3 src/assets_gen.py

mkdir -p build
write_toolchain build/tc-x64.cmake  Linux  x86_64  x86_64-linux-gnu-gcc /usr/x86_64-linux-gnu
write_toolchain build/tc-x86.cmake  Linux  i686    i686-linux-gnu-gcc   /usr/i686-linux-gnu
write_toolchain build/tc-wx64.cmake Windows AMD64   x86_64-w64-mingw32-gcc /usr/x86_64-w64-mingw32 x86_64-w64-mingw32-windres
write_toolchain build/tc-wx86.cmake Windows X86     i686-w64-mingw32-gcc   /usr/i686-w64-mingw32     i686-w64-mingw32-windres

for t in "${TARGETS[@]}"; do
    case $t in
        aarch64)
            build_raylib aarch64
            build_app aarch64 cc linux-aarch64 "$LINUX_LIBS" ""
            ;;
        x64)
            build_raylib x64 build/tc-x64.cmake
            build_app x64 x86_64-linux-gnu-gcc linux-x64 "$LINUX_LIBS" ""
            ;;
        x86)
            build_raylib x86 build/tc-x86.cmake
            build_app x86 i686-linux-gnu-gcc linux-x86 "$LINUX_LIBS" ""
            ;;
        win-x64)
            build_raylib win-x64 build/tc-wx64.cmake
            build_app win-x64 x86_64-w64-mingw32-gcc windows-x64 "$WINDOWS_LIBS" ".exe"
            ;;
        win-x86)
            build_raylib win-x86 build/tc-wx86.cmake
            build_app win-x86 i686-w64-mingw32-gcc windows-x86 "$WINDOWS_LIBS" ".exe"
            ;;
        *) echo "unknown target $t"; exit 1 ;;
    esac
done
echo "all done."
