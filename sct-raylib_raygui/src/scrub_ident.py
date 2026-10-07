#!/usr/bin/env python3
"""Zero out GCC ".ident" strings and build-machine home paths left in a
linked binary.

mingw objects (crt, libgcc, winpthreads, prebuilt static libs) carry
"GCC: (GNU) x.y.z" records in ".rdata$zzz" sections, merged by the linker
into ".rdata" where strip cannot remove them; winpthreads also embeds its
build machine's source paths via assert file names. None of these bytes are
referenced at run time, so overwriting them with NUL is safe; file size and
layout are unchanged.

Usage: scrub_ident.py <binary>
"""
import re
import sys

PATTERN = re.compile(rb"GCC: \([^)\x00]*\) [0-9][0-9.]*[^\x00]*\x00")

# Build-machine home paths smuggled in via assert files (e.g. winpthreads);
# the app never references /home or /Users at run time, so blank them all.
HOME_PATH = re.compile(rb"/(?:home|Users)/[^\x00]{0,300}\x00")


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    path = sys.argv[1]
    with open(path, "rb") as f:
        data = bytearray(f.read())

    count = 0
    for pattern in (PATTERN, HOME_PATH):
        for m in pattern.finditer(data):
            data[m.start():m.end()] = b"\x00" * (m.end() - m.start())
            count += 1

    if count:
        with open(path, "wb") as f:
            f.write(data)
    print(f"scrub_ident: {path}: {count} string(s) removed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
