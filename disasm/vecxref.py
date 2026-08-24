#!/usr/bin/env python3
"""Vector-ROM shape cross-reference, naming, and C export.

- Names character shapes from the game's own glyph tables:
    0x3A2E: 26 words, 'A'..'Z'   (used by DRAW_TEXT / CHAR_GLYPH_EMIT)
    0x3FD3: '0'..'9' + punctuation region (digit path, (c-0x2F)*2)
- Scans the whole main ROM for JSRL/JMPL words (0xC800-0xCFFF /
  0xE800-0xEFFF at any byte offset) to count references per shape.
- Groups remaining shapes into families: runs of consecutive
  subroutines with identical vector counts = pre-rotated frame sets.
- Writes:
    vec_names.py     name map (imported by dvgdasm.py)
    omega_shapes.h/c all shapes as C line-segment arrays for OpenGL
"""

MEM = bytearray(open("omega_dump.bin", "rb").read())
LO, HI = 0x9000, 0xA000


def rw(a):
    return MEM[a] | (MEM[a + 1] << 8)


def s12(v):
    return v - 4096 if v & 0x800 else v


# ---------------------------------------------------------------------------
# enumerate subroutines (same linear walk as dvgdasm)
# ---------------------------------------------------------------------------

def decode_op(a):
    w1 = rw(a)
    op = w1 >> 12
    if op <= 9 or op == 0xA:
        return 4, op
    return 2, op


# Glyph-table entry points. A linear sweep only starts a new shape
# after a terminator, but two glyphs are deliberately entered in the
# MIDDLE of the preceding one and share its tail:
#   0x98EE  'F'  - 'E' is just the extra bottom bar, then falls into F
#   0x9B24  ' '  - the trailing blank advance at the end of '7'
# Without seeding these the sweep never lists them and they end up with
# no name, no listing label, and no entry in the C shape table.
def glyph_entry_points():
    pts = set()
    for i in range(26):                              # 'A'..'Z'
        pts.add(0x8000 + ((rw(0x3A2E + i * 2) & 0xFFF) << 1))
    for i in range(18):                              # '/'..'@' digit path
        if 0x3FD3 + i * 2 >= 0x4000:
            break
        pts.add(0x8000 + ((rw(0x3FD3 + i * 2) & 0xFFF) << 1))
    pts.add(0x8000 + ((0xCD92 & 0xFFF) << 1))        # DRAW_TEXT space
    pts.add(0x8000 + ((0xC00F & 0xFFF) << 1))        # DRAW_TEXT period
    return {p for p in pts if LO <= p < HI}


ENTRY_POINTS = glyph_entry_points()

subs = []          # list of start addrs
a = LO
new = True
while a < HI:
    if a == 0x9406:
        # stray 2-byte word here desyncs a linear sweep; the hardware
        # enters the next shape at 0x9408 (confirmed by runtime census)
        a = 0x9408
        new = True
        continue
    if new or a in ENTRY_POINTS:
        if not subs or subs[-1] != a:
            subs.append(a)
        new = False
    n, op = decode_op(a)
    if op in (0xB, 0xD, 0xE):
        new = True
    a += n
sub_set = set(subs)

# ---------------------------------------------------------------------------
# reference scan
# ---------------------------------------------------------------------------

refs = {}          # shape addr -> list of referencing addresses
for pos in range(0x0000, 0x4000 - 1):
    w = rw(pos)
    hi4 = w >> 12
    if hi4 in (0xC, 0xE):
        tgt = 0x8000 + ((w & 0xFFF) << 1)
        if tgt in sub_set:
            refs.setdefault(tgt, []).append(pos)
# vec ROM internal references
for pos in range(LO, HI - 1, 2):
    w = rw(pos)
    if w >> 12 in (0xC, 0xE):
        tgt = 0x8000 + ((w & 0xFFF) << 1)
        if tgt in sub_set:
            refs.setdefault(tgt, []).append(pos)

# ---------------------------------------------------------------------------
# character names from the glyph tables
# ---------------------------------------------------------------------------

names = {}


def glyph_word(c):
    """CHAR_GLYPH_EMIT (ROM 0x2C3A), transcribed exactly: the glyph word
    the ROM would emit for ASCII code c.
        c >= 'A'  -> table 0x3A2E, index c-'A', clamped at 'Z'
        c == ' '  -> 0x3FD3 index 0x0B
        c == '.'  -> 0x3FCF index 0
        c == ','  -> 0x3FCF index 1
        else      -> 0x3FD3 index c-'/', anything below '/' forced to '0'
    """
    if c >= 0x41:
        return rw(0x3A2E + (min(c, 0x5A) - 0x41) * 2)
    if c == 0x20:
        return rw(0x3FD3 + 0x0B * 2)
    if c == 0x2E:
        return rw(0x3FCF)
    if c == 0x2C:
        return rw(0x3FCF + 2)
    return rw(0x3FD3 + ((c if c >= 0x2F else 0x30) - 0x2F) * 2)


# ascii code -> shape byte address, straight from the routine above
char_map = {}
for c in range(0x20, 0x80):
    tgt = 0x8000 + ((glyph_word(c) & 0xFFF) << 1)
    if tgt in sub_set:
        char_map[c] = tgt


def name_from_word(w, name):
    tgt = 0x8000 + ((w & 0xFFF) << 1)
    if tgt in sub_set and tgt not in names:
        names[tgt] = name


for i in range(26):
    name_from_word(rw(0x3A2E + i * 2), "CHAR_%c" % chr(65 + i))

# digit path in DRAW_TEXT: hl=0x3FD3, index (char-0x2F)*2 -> '/' .. '9'
digit_chars = "/0123456789:;<=>?@"
for i, ch in enumerate(digit_chars):
    w = rw(0x3FD3 + i * 2)
    if 0x3FD3 + i * 2 >= 0x4000:
        break
    label = ("CHAR_%c" % ch) if ch.isalnum() else "CHAR_x%02X" % ord(ch)
    name_from_word(w, label)

# space and period from DRAW_TEXT immediates. 0xCD92 -> 0x9B24 is a
# bare 16-unit blank advance living at the tail of '7'; the glyph table
# also points ':' at it, so the digit loop above would otherwise name it
# CHAR_x3A. The space role is the one text layout needs, so force it.
names.pop(0x8000 + ((0xCD92 & 0xFFF) << 1), None)
name_from_word(0xCD92, "CHAR_SPACE")
name_from_word(0xC00F, "CHAR_PERIOD")

# enemy shapes drawn as text glyphs by the points captions (page script
# at 0x33F6+): ';'=350 photon mine, '>'=500 vapor mine, '@'=1000 droid,
# '?'=1500 command ship
def override_char(ch, name):
    idx = (ord(ch) - 0x2F) * 2
    tgt = 0x8000 + ((rw(0x3FD3 + idx) & 0xFFF) << 1)
    if tgt in sub_set:
        names[tgt] = name

override_char(';', "PHOTON_MINE")
override_char('>', "VAPOR_MINE")
override_char('@', "DROID_00")
override_char('?', "CMDSHIP")

# user-identified shapes (visual pass over shapes_preview.html)
USER_NAMES = {
    0x945C: "DROID_00",
    0x94A2: "DROID_01",
    0x94E8: "DROID_02",
    0x952E: "DEATH_SHIP_00",
    0x9554: "DEATH_SHIP_01",
    0x957C: "DEATH_SHIP_02",
    0x95A4: "DEATH_SHIP_03",
    0x9B7E: "CHAR_SPACE2",
    0x9774: "SHIP_EXPLODE_00",
    0x97B0: "SHIP_EXPLODE_01",
    0x9824: "SHIP_EXPLODE_02",
    0x9408: "CMDSHIP_00",       # census: entered 1827x, wave-enemy rate
    0x9B84: "UNUSED_9B84",      # census: never drawn
    0x9BAE: "UNUSED_9BAE",      # census: never drawn
    # glyph-table words at 0x3FCF/0x3FD1 (CHAR_GLYPH_EMIT punctuation,
    # below the digit table): attract story text period/comma.
    # CHAR_GLYPH_EMIT 0x2C59/0x2C64 routes '.' to the word at 0x3FCF
    # (-> 0x9B54) and ',' to the word at 0x3FD1 (-> 0x9B66). 0x9B54 is
    # a tiny crossed dot; 0x9B66 is that dot with a descender tail.
    0x9B54: "CHAR_PERIOD",
    0x9B66: "CHAR_COMMA",
}
names.update({a: n for a, n in USER_NAMES.items() if a in sub_set})

# ship rotation pairs: the ROM opens with 16 (3-vec flame, 13-vec hull)
# pairs = 16 pre-rotated ship frames (22.5 degree steps)
pairs = [s for s in sorted(sub_set) if s < 0x9406]
for i, s in enumerate(pairs):
    if s in names:
        continue
    names[s] = ("THRUST_%02d" if i % 2 == 0 else "SHIP_%02d") % (i // 2)

# ---------------------------------------------------------------------------
# shape stats + family grouping for the rest
# ---------------------------------------------------------------------------

def shape_stats(start):
    """count vectors / words up to terminator"""
    a = start
    vec = 0
    words = 0
    while a < HI:
        n, op = decode_op(a)
        words += n // 2
        if op <= 9 or op == 0xF:
            vec += 1
        if op in (0xB, 0xD, 0xE):
            break
        a += n
    return vec, words, a + n - start


stats = {s: shape_stats(s) for s in subs}

# the 64 single-vector shapes at 0x95CE-0x974D are the shot/spark line
# at 64 angles
shot_i = 0
for s in sorted(subs):
    if 0x95CE <= s < 0x974E and s not in names and stats[s][0] == 1:
        names[s] = "SHOT_%02d" % shot_i
        shot_i += 1

# 0x9C06-0x9FFF: X-mirrored copies of the ship/thrust set (user-
# identified; exact angle mapping TBD - some reordering vs originals).
# Classify by size: >=8 vectors = hull, else flame.
ms = mt = 0
for s in sorted(subs):
    if s >= 0x9C06 and s not in names:
        if stats[s][0] >= 8:
            names[s] = "SHIP_M%02d" % ms; ms += 1
        else:
            names[s] = "THRUST_M%02d" % mt; mt += 1

fam_id = 0
i = 0
order = sorted(subs)
while i < len(order):
    s = order[i]
    if s in names:
        i += 1
        continue
    # find run of consecutive unnamed subs with same vector count
    j = i
    while (j + 1 < len(order) and order[j + 1] not in names
           and stats[order[j + 1]][0] == stats[s][0]):
        j += 1
    run = j - i + 1
    if run >= 6:
        for k in range(run):
            names[order[i + k]] = "FAM%02d_F%02d" % (fam_id, k)
        fam_id += 1
        i = j + 1
    else:
        i += 1

for s in order:
    if s not in names:
        names[s] = "SHAPE_%04X" % s

# ---------------------------------------------------------------------------
# emit name map for dvgdasm
# ---------------------------------------------------------------------------

# Curated notes that must survive regeneration. 0x9B84 has refs=0 in the
# static JSRL scan because its only reference is built at runtime:
# VG_RESTART_FRAME 0x17AB assembles word 0xCDC2 in place (the 2P
# active-score blink mark -> dlist 0x81A6).
NAME_NOTES = {
    0x9B84: " - misnamed: runtime JSRL from VG_RESTART 0x17AB (2P"
            " active-score blink mark); rename to PLAYER_UP_MARK deferred"
            " while omega_shapes.h uses SHAPE_IDX_UNUSED_9B84",
}

with open("vec_names.py", "w") as f:
    f.write("# generated by vecxref.py - vector ROM shape names\n")
    f.write("VEC_NAMES = {\n")
    for s in order:
        r = len(refs.get(s, []))
        f.write("    0x%04X: %r,  # vec=%d refs=%d%s\n"
                % (s, names[s], stats[s][0], r, NAME_NOTES.get(s, "")))
    f.write("}\n")

# ---------------------------------------------------------------------------
# C export: walk each shape, accumulate line segments
# ---------------------------------------------------------------------------

def walk_shape(start, scale=0, absolute=False, end=None):
    """returns (segs, final_x, final_y); segs = (x0,y0,x1,y1,z) in DVG
    units at global scale 0. absolute=True honors LABS (for fragments
    like the walls list); end bounds the walk for fragments."""
    segs = []
    x = y = 0.0
    a = start
    stack = []
    guard = 4000
    while guard:
        if end is not None and a >= end and not stack:
            break
        if not stack and WALLS_PATCH[0] <= a < WALLS_PATCH[1]:
            a = WALLS_PATCH[1]      # GAME_DLIST_BUILD overwrites this window
            continue
        guard -= 1
        w1 = rw(a)
        op = w1 >> 12
        if op <= 9:
            w2 = rw(a + 2)
            dy = w1 & 0x3FF
            dx = w2 & 0x3FF
            if w1 & 0x400: dy = -dy
            if w2 & 0x400: dx = -dx
            z = w2 >> 12
            temp = (scale + op) & 0xF
            if temp > 9: temp = -1
            f = 2.0 ** -(9 - temp)
            nx, ny = x + dx * f, y + dy * f
            if z: segs.append((x, y, nx, ny, z))
            x, y = nx, ny
            a += 4
        elif op == 0xA:
            if absolute:
                w2 = rw(a + 2)
                x = float(s12(w2 & 0xFFF))
                y = float(s12(w1 & 0xFFF))
                scale = (w2 >> 12) & 0xF
            a += 4
        elif op == 0xB:
            break
        elif op == 0xC:
            stack.append(a + 2)
            a = 0x8000 + ((w1 & 0xFFF) << 1)
        elif op == 0xD:
            if not stack: break
            a = stack.pop()
        elif op == 0xE:
            a = 0x8000 + ((w1 & 0xFFF) << 1)
        else:  # SVEC
            z = (w1 & 0x00F0) >> 4
            dy = w1 & 0x300
            dx = (w1 & 0x003) << 8
            if w1 & 0x400: dy = -dy
            if w1 & 0x004: dx = -dx
            sc = 2 + ((w1 >> 2) & 0x02) + ((w1 >> 11) & 0x01)
            temp = (scale + sc) & 0xF
            if temp > 9: temp = -1
            f = 2.0 ** -(9 - temp)
            nx, ny = x + dx * f, y + dy * f
            if z > 2: segs.append((x, y, nx, ny, z))
            x, y = nx, ny
            a += 2
    return segs, x, y


hdr = open("../c_src/omega_shapes.h", "w")
src = open("../c_src/omega_shapes.c", "w")

hdr.write("""/* omega_shapes.h - generated by vecxref.py
 * All 0x9000-0x9FFF vector-ROM shapes as line segments, plus the
 * playfield WALLS fragment (ROM 0x3109, absolute coordinates).
 * Units: DVG units at global scale 0 (y up); callers scaled these via
 * LABS scale, so multiply as needed. z = DVG intensity 1..15.
 * adv_x/adv_y = net beam displacement of the shape (the real glyph
 * advance for text layout).
 */
#ifndef OMEGA_SHAPES_H
#define OMEGA_SHAPES_H

typedef struct { float x0, y0, x1, y1; unsigned char z; } vec_seg;
typedef struct {
    const char*   name;
    unsigned short rom_addr;   /* byte address 0x9000.. */
    unsigned short word_addr;  /* JSRL operand */
    int            nsegs;
    const vec_seg* segs;
    float          adv_x, adv_y;
} vec_shape;

""")
src.write('#include "omega_shapes.h"\n\n')

# extra non-ROM-shape fragments exported with absolute coordinates
# The walls blob is not drawn as it sits in ROM. GAME_DLIST_BUILD copies
# 0x010E bytes of it to vector RAM 0x81B0 and then immediately overwrites
# sixteen bytes at 0x8234 with a JSRL table from ROM 0x3EE5 (0x1939-0x1942:
# ld bc,$0010 / ld de,$8234 / ld hl,$3EE5 / ldir). In ROM those sixteen
# bytes are eight FFFF placeholders, and FFFF decodes as a full-intensity
# SVEC of dy=-3 dx=-3 at scale+5 - eight collinear 48-unit steps, i.e. a
# long 45-degree line running down-left out of the inner box's bottom-right
# corner. That stray line is a placeholder the machine never draws; the
# real words there call the score/credit/lives sub-lists, which this port
# draws through its own HUD path. Skip the patched window.
WALLS_PATCH = (0x3109 + 0x84, 0x3109 + 0x94)   # -> vector RAM 0x8234-0x8243

FRAGMENTS = [("WALLS", 0x3109, 0x3217)]

def emit_shape(cname, segs):
    src.write("static const vec_seg %s_segs[] = {\n" % cname)
    for (x0, y0, x1, y1, z) in segs:
        src.write("    { %.3ff, %.3ff, %.3ff, %.3ff, %d },\n"
                  % (x0, y0, x1, y1, z))
    if not segs:
        src.write("    { 0, 0, 0, 0, 0 },\n")
    src.write("};\n")

shape_rows = []
for s in order:
    segs, fx, fy = walk_shape(s)
    cname = names[s].lower()
    emit_shape(cname, segs)
    shape_rows.append((names[s], s, (s - 0x8000) >> 1, segs, cname, fx, fy))

for (fname, fstart, fend) in FRAGMENTS:
    segs, fx, fy = walk_shape(fstart, absolute=True, end=fend)
    emit_shape(fname.lower(), segs)
    shape_rows.append((fname, fstart, 0, segs, fname.lower(), 0.0, 0.0))

src.write("\nconst vec_shape omega_shapes[] = {\n")
for (nm, addr, wa, segs, cname, fx, fy) in shape_rows:
    src.write('    { "%s", 0x%04X, 0x%03X, %d, %s_segs, %.3ff, %.3ff },\n'
              % (nm, addr, wa, len(segs), cname, fx, fy))
src.write("};\n")
src.write("const int omega_shape_count = %d;\n" % len(shape_rows))

idx_of = {addr: i for i, (nm, addr, wa, segs, cname, fx, fy)
          in enumerate(shape_rows)}
src.write("\n/* ASCII -> shape index, straight out of the ROM's own glyph\n"
          " * tables (0x3A2E for A-Z, 0x3FD3 for the digit/punctuation path,\n"
          " * plus DRAW_TEXT's space immediate). -1 = no glyph. Note the four\n"
          " * enemy icons the points-caption page draws as ordinary text:\n"
          " * ';' photon mine, '>' vapor mine, '?' command ship, '@' droid. */\n")
src.write("const short omega_char_shape[128] = {\n")
for base in range(0, 128, 8):
    src.write("    %s,\n" % ", ".join(
        "%4d" % idx_of.get(char_map.get(c, -1), -1)
        for c in range(base, base + 8)))
src.write("};\n")

hdr.write("extern const vec_shape omega_shapes[];\n")
hdr.write("extern const int omega_shape_count;\n")
hdr.write("extern const short omega_char_shape[128];  "
          "/* ASCII -> shape index, -1 = none */\n\n")
for i, (nm, addr, wa, segs, cname, fx, fy) in enumerate(shape_rows):
    hdr.write("#define SHAPE_IDX_%-16s %d\n" % (nm, i))
hdr.write("\n#endif\n")
hdr.close()
src.close()

# ---------------------------------------------------------------------------
# HTML shape viewer (visual identification aid)
# ---------------------------------------------------------------------------

with open("shapes_preview.html", "w") as f:
    f.write("<!doctype html><meta charset='utf-8'>"
            "<title>Omega Race shapes</title>"
            "<style>body{background:#000;color:#0f0;font:12px monospace}"
            ".s{display:inline-block;margin:4px;text-align:center}"
            "canvas{border:1px solid #333}</style>\n")
    f.write("<h3>Omega Race vector ROM - %d shapes</h3>\n" % len(order))
    for s in order:
        f.write("<div class='s'><canvas id='c%04X' width='96' "
                "height='96'></canvas><br>%s<br>%04X</div>\n"
                % (s, names[s], s))
    f.write("<script>\nconst S={\n")
    for s in order:
        segs, _fx, _fy = walk_shape(s)
        f.write("'c%04X':%s,\n" % (s,
                [[round(a, 2) for a in seg[:4]] + [seg[4]] for seg in segs]))
    f.write("};\n")
    f.write("""
for (const id in S) {
  const c = document.getElementById(id), g = c.getContext('2d');
  const segs = S[id];
  if (!segs.length) continue;
  let xs = [], ys = [];
  for (const s of segs) { xs.push(s[0], s[2]); ys.push(s[1], s[3]); }
  const minx = Math.min(...xs), maxx = Math.max(...xs);
  const miny = Math.min(...ys), maxy = Math.max(...ys);
  const w = Math.max(maxx - minx, 0.01), h = Math.max(maxy - miny, 0.01);
  const sc = 80 / Math.max(w, h);
  g.strokeStyle = '#0f0'; g.lineWidth = 1.2;
  for (const s of segs) {
    g.globalAlpha = 0.25 + 0.75 * (s[4] / 15);
    g.beginPath();
    g.moveTo(8 + (s[0]-minx)*sc, 88 - (s[1]-miny)*sc);
    g.lineTo(8 + (s[2]-minx)*sc, 88 - (s[3]-miny)*sc);
    g.stroke();
  }
}
</script>\n""")

# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------

named_chars = sum(1 for n in names.values() if n.startswith("CHAR_"))
fams = {}
for s in order:
    if names[s].startswith("FAM"):
        fams.setdefault(names[s][:5], []).append(s)
unref = [s for s in order if s not in refs and not names[s].startswith("CHAR")]

print("shapes: %d  chars named: %d  families: %d  leftover SHAPE_: %d"
      % (len(order), named_chars, len(fams),
         sum(1 for n in names.values() if n.startswith("SHAPE_"))))
for f, members in sorted(fams.items()):
    vc = stats[members[0]][0]
    print("  %s: %2d frames, %2d vectors each, %04X-%04X"
          % (f, len(members), vc, members[0], members[-1]))
print("unreferenced (not chars): %d" % len(unref))
print("  " + " ".join("%04X" % s for s in unref[:40]))
