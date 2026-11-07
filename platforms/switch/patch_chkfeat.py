#!/usr/bin/env python3
"""Replace every CHKFEAT X16 (HINT #40) in an AArch64 ELF's executable segments with NOP.

GCC 16's libgcc unwinder probes for Guarded Control Stacks with CHKFEAT before reading GCSPR_EL0.
The Switch's Cortex-A57 has no GCS and executes CHKFEAT as a NOP, so X16 stays 1 and the read is
skipped. Some emulators (Ryujinx 1.3.3) clear X16 instead, then fault on the MRS the first time a
C++ exception unwinds. A NOP is exactly what the hardware does, so the patch changes nothing on the
console.
"""
import struct
import sys

CHKFEAT_X16 = 0xD503251F
NOP = 0xD503201F
MRS_GCSPR_EL0 = 0xD53B2520  # | Rt
MOV_XZR = 0xAA1F03E0  # MOV Xd, XZR | Rd


def main(path):
    with open(path, "r+b") as f:
        data = bytearray(f.read())
        if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
            sys.exit(f"{path}: not a little-endian ELF64")
        e_phoff, = struct.unpack_from("<Q", data, 0x20)
        e_phentsize, e_phnum = struct.unpack_from("<HH", data, 0x36)
        patched = 0
        for i in range(e_phnum):
            p_type, p_flags, p_offset, _, _, p_filesz = struct.unpack_from(
                "<IIQQQQ", data, e_phoff + i * e_phentsize)
            if p_type != 1 or not (p_flags & 1):  # PT_LOAD, PF_X
                continue
            for off in range(p_offset, p_offset + p_filesz - 3, 4):
                w = struct.unpack_from("<I", data, off)[0]
                if w == CHKFEAT_X16:
                    struct.pack_into("<I", data, off, NOP)
                    patched += 1
                elif (w & ~0x1F) == MRS_GCSPR_EL0:
                    # Unreachable once CHKFEAT reports no GCS, but JITs that translate whole
                    # functions reject the unknown system register: MOV Xt, XZR instead.
                    struct.pack_into("<I", data, off, MOV_XZR | (w & 0x1F))
                    patched += 1
        f.seek(0)
        f.write(data)
    print(f"patch_chkfeat: {patched} CHKFEAT replaced with NOP in {path}")


if __name__ == "__main__":
    main(sys.argv[1])
