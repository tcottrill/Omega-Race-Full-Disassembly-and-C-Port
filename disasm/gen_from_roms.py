#!/usr/bin/env python3
"""Rebuild everything ROM-derived from a user-supplied Omega Race ROM set.

The repository does not ship Midway's ROM images. This script takes the
standard MAME `omegrace` set and regenerates, in order:

  1. the working dumps the disasm tools read:
       omega_dump.bin   64 KB main-CPU address-space image
                        (program 0x0000-0x3FFF + vector ROM 0x9000-0x9FFF)
       sound_k5.bin     sound-board program
       dvgprom.bin      DVG state PROM
  2. the generated C data files the port compiles in
     (../c_src/, no ROM files needed at the port's runtime):
       omega_vecrom.c   vector ROM image (written by this script)
       omega_dvgprom.c  DVG state PROM + wall list   (gen_dvgprom.py)
       omega_pagerom.c  attract/message page scripts (gen_pagerom.py)
       omega_postrom.c  POST screen display lists    (gen_postrom.py)
       omega_sndrom.c   sound command table + scripts (gen_sndrom.py)
       omega_shapes.c/h named shapes as line segments (vecxref.py, which
                        also refreshes vec_names.py and
                        shapes_preview.html in disasm/)
  3. with --listings, the annotated disassemblies too:
       omega_main.asm / omega_sound.asm  (z80trace.py)
       omega_vecrom.asm                  (dvgdasm.py, needs vec_names.py)

Usage (from disasm\\):
    python gen_from_roms.py <romdir> [--listings]

<romdir> must contain the MAME omegrace set members:
    omega.m7  omega.l7  omega.k7  omega.j7    program   (4 KB each)
    omega.e1  omega.f1                        vector ROM (2 KB each)
    sound.k5                                  sound board (2 KB)
    dvgprom.bin (or dvcprom.bin)              DVG state PROM (256 B)
A zip is fine too: point <romdir> at the extracted directory.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# main-CPU address-space image: (set member, load address, size) -
# the same map the Z80 rig uses (ref.cpp Load_Roms).
DUMP_MAP = [
    ("omega.m7", 0x0000, 0x1000),
    ("omega.l7", 0x1000, 0x1000),
    ("omega.k7", 0x2000, 0x1000),
    ("omega.j7", 0x3000, 0x1000),
    ("omega.e1", 0x9000, 0x0800),
    ("omega.f1", 0x9800, 0x0800),
]


def find_rom(romdir, names, size):
    for n in names:
        p = os.path.join(romdir, n)
        if os.path.isfile(p):
            got = os.path.getsize(p)
            if got != size:
                sys.exit("%s is %d bytes, expected %d - wrong set?"
                         % (p, got, size))
            return p
    sys.exit("missing %s in %s" % (" / ".join(names), romdir))


def build_dumps(romdir):
    mem = bytearray(0x10000)
    for name, addr, size in DUMP_MAP:
        p = find_rom(romdir, [name], size)
        mem[addr:addr + size] = open(p, "rb").read()
    open(os.path.join(HERE, "omega_dump.bin"), "wb").write(mem)
    print("omega_dump.bin  assembled (64 KB)")

    for out, names, size in [
            ("sound_k5.bin", ["sound.k5", "sound_k5.bin"], 0x800),
            ("dvgprom.bin",  ["dvgprom.bin", "dvcprom.bin"], 0x100)]:
        src = find_rom(romdir, names, size)
        open(os.path.join(HERE, out), "wb").write(open(src, "rb").read())
        print("%-15s copied from %s" % (out, os.path.basename(src)))
    return mem


def write_vecrom_c(mem):
    """../c_src/omega_vecrom.c: the 0x9000-0x9FFF vector ROM verbatim."""
    lines = [
        "/* omega_vecrom.c - generated: vector ROM (omega.e1+f1) image,",
        " * mapped at DVG address 0x9000-0x9FFF. The DVG interpreter",
        " * JSRLs into this exactly like the hardware did. */",
        "const unsigned char omega_vecrom[0x1000] = {",
    ]
    rom = mem[0x9000:0xA000]
    for i in range(0, 0x1000, 16):
        row = ",".join("0x%02X" % b for b in rom[i:i + 16])
        lines.append("    %s," % row)
    lines.append("};")
    open(os.path.join(HERE, "..", "c_src", "omega_vecrom.c"),
         "w").write("\n".join(lines) + "\n")
    print("omega_vecrom.c  written")


def run(script, *args):
    print("--- %s %s" % (script, " ".join(args)))
    subprocess.run([sys.executable, os.path.join(HERE, script), *args],
                   cwd=HERE, check=True)


def main():
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    listings = "--listings" in sys.argv
    if len(argv) != 1:
        sys.exit(__doc__)
    romdir = argv[0]

    mem = build_dumps(romdir)
    write_vecrom_c(mem)
    run("gen_dvgprom.py")            # -> ../c_src/omega_dvgprom.c
    run("gen_pagerom.py")            # -> ../c_src/omega_pagerom.c
    run("gen_postrom.py")            # -> ../c_src/omega_postrom.c
    run("gen_sndrom.py")             # -> ../c_src/omega_sndrom.c
    run("vecxref.py")     # -> ../c_src/omega_shapes.c/h, vec_names.py

    if listings:
        run("z80trace.py", "main")   # -> omega_main.asm
        run("z80trace.py", "sound")  # -> omega_sound.asm
        run("dvgdasm.py")            # -> omega_vecrom.asm

    print("\nDone. Build the port with c_src\\build_all.bat.")


if __name__ == "__main__":
    main()
