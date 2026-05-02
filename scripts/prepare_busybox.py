#!/usr/bin/env python3
"""Copy a host BusyBox binary and neutralize CET endbr64 markers.

The host /usr/bin/busybox on this machine is statically linked, which is good
for AOS, but it is also built with CET branch protection. That inserts
`endbr64` as the first instruction of many functions. Current AOS/QEMU bring-up
does not expose IBT/CET to userspace, so those instructions fault with #UD.

For now, patch only the copied build artifact, replacing each `endbr64`
sequence with four NOP bytes. This keeps the imported test binary runnable
without modifying the system BusyBox installation.
"""

from __future__ import annotations

import pathlib
import shutil
import sys

ENDBR64 = b"\xF3\x0F\x1E\xFA"
NOP4 = b"\x90\x90\x90\x90"
NOTRACK_JUMPS = {
    b"\x3E\xFF\xE0": b"\xFF\xE0\x90",  # jmp rax
    b"\x3E\xFF\xE1": b"\xFF\xE1\x90",  # jmp rcx
    b"\x3E\xFF\xE2": b"\xFF\xE2\x90",  # jmp rdx
    b"\x3E\xFF\xE6": b"\xFF\xE6\x90",  # jmp rsi
    b"\x3E\xFF\xE7": b"\xFF\xE7\x90",  # jmp rdi
    b"\x3E\x41\xFF\xE2": b"\x41\xFF\xE2\x90",  # jmp r10
    b"\x3E\x41\xFF\xE6": b"\x41\xFF\xE6\x90",  # jmp r14
}


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: prepare_busybox.py <src> <dst>", file=sys.stderr)
        return 1

    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])

    if not src.is_file():
        print(f"missing source binary: {src}", file=sys.stderr)
        return 1

    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)

    data = dst.read_bytes()
    endbr_count = data.count(ENDBR64)
    if endbr_count == 0:
        print("warning: no endbr64 sequences found in copied busybox", file=sys.stderr)
    data = data.replace(ENDBR64, NOP4)

    notrack_count = 0
    for src, repl in NOTRACK_JUMPS.items():
        count = data.count(src)
        notrack_count += count
        data = data.replace(src, repl)

    dst.write_bytes(data)
    print(
        f"prepared busybox: patched {endbr_count} endbr64 sites and "
        f"{notrack_count} notrack branch sites into {dst}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
