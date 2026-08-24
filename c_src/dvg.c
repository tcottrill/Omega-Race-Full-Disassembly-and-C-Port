/* dvg.c - Omega Race C port: vector RAM + DVG display-list walker.
 *
 * The game builds a real display list exactly like the ROM did, and this
 * walks it: beam position, global scale register, intensity, subroutine
 * calls into the vector ROM, jumps and halt.
 *
 * The walker is a transcription of the rig's dvg_render_list
 * (OmegaraceTest/dvg.cpp), which is the version validated against the
 * real machine. The rig keeps the MAME PROM state machine
 * (mame_late_avgdvg) for TIMING only and draws with this simpler
 * opcode-level walk; the same split applies here. Differences from the
 * rig are deliberate and marked: the beam is kept in DVG coordinates
 * (y up) rather than the rig's screen-down 1024-y, which comes out
 * visually identical under this host's y-up ortho and matches the shape
 * data disasm/vecxref.py exports.
 *
 * Entry format, confirmed against a live dump of the real machine
 * (disasm/frametime.exe --dump-vram, game_mode=03):
 *
 *   8000 (obj  1): E001 A2FA 03D4 8000 0000 C0C5
 *   8054 (obj  8): E030 B000 B000 B000 B000 B000
 *
 *   word0  JMPL: alive -> own body (entry_word+1)
 *                dead  -> next entry (entry_word+6), body left as HALTs
 *   word1  LABS y   (0xA000 | y)
 *   word2  LABS x   (scale in the top nibble)
 *   word3  scale/opcode vector (zero length, invisible)
 *   word4  its second word (intensity/scale nibble)
 *   word5  JSRL shape  (0xC000 | word_addr)
 *
 * Word addresses are relative to 0x8000, so vecram[0] == DVG 0x8000 and
 * the vector ROM image sits at vecram[0x1000] == DVG 0x9000.
 */

#include <string.h>
#include "omega_state.h"

extern const unsigned char omega_vecrom[0x1000];

/* the game-screen wall list (ROM 0x3109), generated into omega_dvgprom.c */
extern const unsigned short omega_walls_words[];
extern const int            omega_walls_word_count;

uint8_t omega_vecram[0x2000];

/* host line sink (platform/omega_platform.h; the headless backend routes
 * it to a capture hook). A call with
 * both endpoints equal is a zero-length vector: the hardware parks the
 * beam and lights a dot, so the host must plot a point, not drop it. */
void plat_video_line(float x0, float y0, float x1, float y1, int z);

/* The object region: 36 entries x 12 bytes at DVG 0x8000-0x81AF. The ROM
 * built these in a RAM shadow at 0x4481 and VG_RESTART_FRAME (0x1750)
 * copied exactly 0x01B0 bytes of it into vector RAM once per frame -
 * which is why the constant below is the same 0x01B0. Only the first 32
 * entries are objects; the last four hold a fixed template (see
 * DLIST_TAIL). */
#define OBJ_ENTRIES     36
#define OBJ_ENTRY_BYTES 12
#define OBJ_REGION      (OBJ_ENTRIES * OBJ_ENTRY_BYTES)   /* 0x01B0 */

static void wrwd(int byte_off, uint16_t w)
{
    omega_vecram[byte_off]     = (uint8_t)(w & 0xFF);
    omega_vecram[byte_off + 1] = (uint8_t)(w >> 8);
}

/* The display-list shadow (RAM 0x4481-0x4630 on hardware). The game
 * builds the 36 object entries AND the tail template's digit runs here;
 * VG_RESTART_FRAME copies all 0x1B0 bytes into vector RAM each frame.
 * The digits must live here and not in vector RAM, or the per-frame
 * copy would erase them - that is why GAME_DLIST_BUILD and SCORE_ADD
 * write their glyphs to 0x4617/0x462B (shadow), not 0x8196/0x81AA. */
uint8_t omega_dlist_shadow[0x1B0];

static void swrwd(int byte_off, uint16_t w)
{
    omega_dlist_shadow[byte_off]     = (uint8_t)(w & 0xFF);
    omega_dlist_shadow[byte_off + 1] = (uint8_t)(w >> 8);
}

static void vgt_prime_idle(void);   /* timing machine -> authentic idle */

void dvg_init(void)
{
    memset(omega_vecram, 0, sizeof omega_vecram);
    memcpy(omega_vecram + 0x1000, omega_vecrom, 0x1000);
    dvg_ship_buffers_build();
    dvg_dlist_reset();
    vgt_prime_idle();
}

/* DLIST_RESET (ROM 0x1823): wait for the vector generator, then fill the
 * display list with HALT words so nothing is drawn until it is built
 * again. Full ROM range 0x8000-0x8BF7 (0x1841: 0x0FFE-0x0408 bytes);
 * a partial clear would leave stale E180 fill words and the border box
 * from earlier attract steps visible under later pages. Note this wipes
 * the 0x87F0 ship frame set exactly like the hardware; GAME_DLIST_BUILD
 * rebuilds it (dvg_ship_buffers_build) before every game screen, same
 * as the ROM's 0x18BC call. */
void dvg_dlist_reset(void)
{
    int i;
    for (i = 0; i < 0x0BF8; i += 2)
        wrwd(i, 0xB000);                      /* HALT */
    /* the ROM's fill covers the shadow too (0x1831: 0x600 bytes from
     * 0x4481) - the tail template is wiped and GAME_DLIST_BUILD lays
     * it down again */
    for (i = 0; i < 0x1B0; i += 2)
        swrwd(i, 0xB000);
}

/* ---- ship frame buffers (WALL_BUFFERS_BUILD 0x18BC) ------------------
 * Only a quarter of the ship's 64 headings is in ROM. SHIP_BASE below
 * selects one of four 0x408-byte frame sets by heading bits 4-5, and two
 * of those four live in vector RAM: the ROM copies the 0x9000-0x9407
 * set into 0x8BF8 and 0x87F0 and then bit-transforms each copy in place.
 *
 * The transform is a single XOR of bit 2 on the byte that carries a
 * component's sign - 0x0004 in an SVEC's low byte is its X sign, 0x0400
 * in the high byte is its Y sign, and for a long VCTR the same bit lives
 * in word0's high byte (Y) and word1's high byte (X):
 *   VWALL_XFORM_A 0x2D56  flip X       -> 0x9BF8 (already in ROM as the
 *                                        SHIP_M* set; the ROM's ldir to
 *                                        it is a write to ROM, a no-op)
 *   VWALL_XFORM_B 0x2D8A  flip X and Y -> 0x8BF8  (180 degrees)
 *   VWALL_XFORM_C 0x2DC8  flip Y       -> 0x87F0
 * Without these two RAM copies, headings 0x20-0x3F would jump into
 * blank vector RAM. */
static void vwall_xform(int off, int len, int flip_x, int flip_y)
{
    while (len > 0) {
        uint8_t hi = omega_vecram[off + 1];
        if (hi >= 0xF0) {                 /* SVEC: 2 bytes            */
            if (flip_y) omega_vecram[off + 1] ^= 0x04;
            if (flip_x) omega_vecram[off + 0] ^= 0x04;
            off += 2; len -= 2;
        } else if (hi >= 0xB0) {          /* HALT/JSRL/RTSL/JMPL      */
            off += 2; len -= 2;
        } else if (hi >= 0xA0) {          /* LABS: 4 bytes, untouched */
            off += 4; len -= 4;
        } else {                          /* long VCTR: 4 bytes       */
            if (flip_y) omega_vecram[off + 1] ^= 0x04;
            if (flip_x) omega_vecram[off + 3] ^= 0x04;
            off += 4; len -= 4;
        }
    }
}

#define SHIP_SET_SRC  0x1000      /* DVG 0x9000, the ROM frame set */
#define SHIP_SET_LEN  0x0408
#define SHIP_SET_180  0x0BF8      /* DVG 0x8BF8 */
#define SHIP_SET_FLIPY 0x07F0     /* DVG 0x87F0 */

void dvg_ship_buffers_build(void)
{
    memcpy(omega_vecram + SHIP_SET_180,   omega_vecram + SHIP_SET_SRC, SHIP_SET_LEN);
    memcpy(omega_vecram + SHIP_SET_FLIPY, omega_vecram + SHIP_SET_SRC, SHIP_SET_LEN);
    vwall_xform(SHIP_SET_180,   SHIP_SET_LEN, 1, 1);   /* XFORM_B */
    vwall_xform(SHIP_SET_FLIPY, SHIP_SET_LEN, 0, 1);   /* XFORM_C */
}

static uint16_t rdwd(int a)
{
    a &= 0x1FFF;
    return (uint16_t)(omega_vecram[a] | (omega_vecram[a + 1] << 8));
}

static int twos12(uint16_t v)
{
    int x = v & 0xFFF;
    return (x & 0x800) ? x - 0x1000 : x;
}

/* Walk the display list from word 0 and emit lines. Structure, masks,
 * scale arithmetic and intensity gating all follow the rig's
 * dvg_render_list one-for-one. */
void dvg_render(void)
{
    int pc = 0, sp = 0, scale = 0, done = 0, guard = 20000;
    int stack[8];
    float cx = 0.0f, cy = 0.0f;

    while (!done && guard--) {
        uint16_t w1 = rdwd(pc); pc += 2;
        int op = w1 >> 12;
        uint16_t w2 = 0;
        if (op < 0x0B) { w2 = rdwd(pc); pc += 2; }

        switch (op) {
        case 0: case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: case 9: {
            /* long vector: raw x, y and intensity */
            int z = w2 >> 12;
            int y = w1 & 0x3FF, x = w2 & 0x3FF;
            int temp;
            float dx, dy;
            if (w1 & 0x400) y = -y;
            if (w2 & 0x400) x = -x;
            temp = (scale + op) & 0x0F;
            if (temp > 9) temp = -1;
            /* rig: delta = (v << 16) >> (9 - temp), i.e. v * 2^temp/512 */
            dx = (float)x * (float)(1 << (temp + 1)) / 1024.0f;
            dy = (float)y * (float)(1 << (temp + 1)) / 1024.0f;
            if (temp < 0) { dx = (float)x / 1024.0f; dy = (float)y / 1024.0f; }
            if (z) plat_video_line(cx, cy, cx + dx, cy + dy, z);
            cx += dx; cy += dy;
            break;
        }
        case 0x0A:                      /* LABS: recenter beam + scale */
            cx = (float)twos12(w2);
            cy = (float)twos12(w1);
            scale = (w2 >> 12) & 0x0F;
            break;
        case 0x0B:                      /* HALT */
            done = 1;
            break;
        case 0x0C:                      /* JSRL */
            /* the hardware stack is 4 deep and the generator halts on
             * overflow - matched, so a runaway list ends where it would
             * end on the real machine */
            stack[sp] = pc;
            if (sp == 4) { done = 1; sp = 0; }
            else { sp++; pc = (w1 & 0x0FFF) << 1; }
            break;
        case 0x0D:                      /* RTSL */
            if (sp > 0) pc = stack[--sp]; else done = 1;
            break;
        case 0x0E:                      /* JMPL */
            pc = (w1 & 0x0FFF) << 1;
            break;
        default: {                      /* 0x0F: short vector */
            int z = (w1 & 0x00F0) >> 4;
            int y = w1 & 0x300, x = (w1 & 0x003) << 8;
            int sc, temp;
            float dx, dy;
            if (w1 & 0x400) y = -y;
            if (w1 & 0x004) x = -x;
            sc = 2 + ((w1 >> 2) & 0x02) + ((w1 >> 11) & 0x01);
            temp = (scale + sc) & 0x0F;
            if (temp > 9) temp = -1;
            dx = (float)x * (float)(1 << (temp + 1)) / 1024.0f;
            dy = (float)y * (float)(1 << (temp + 1)) / 1024.0f;
            if (temp < 0) { dx = (float)x / 1024.0f; dy = (float)y / 1024.0f; }
            if (z > 2) plat_video_line(cx, cy, cx + dx, cy + dy, z);
            cx += dx; cy += dy;
            break;
        }
        }
    }
}

/* ---- object display-list build --------------------------------------
 * Transcribed from the ROM's own entry writer (0x1EA2-0x1EE4, reached
 * per staged object out of OBJECTS_UPDATE), which fills the 12-byte
 * entry that VG_RESTART_FRAME then copies into vector RAM:
 *
 *   ld hl,(obj_posy) / rlc h,rlc h / rlc l,rla / rlc l,rla
 *   ld (ix+2),a                       -> low byte of the LABS *Y* word
 *   ld a,h / and 3 / or $A0 / ld (ix+3),a
 *   ld hl,(obj_posx) / ...same...
 *   ld (ix+4),a                       -> low byte of the LABS *X* word
 *   ld a,($400B) / and $F0 / or h / ld (ix+5),a
 *                        -> X word high byte: LABS scale nibble comes
 *                           from the record's byte +11
 *   ld (ix+6),$00 / ld (ix+8),$00 / ld (ix+9),$00
 *
 * Three details that follow from the code above: the screen coordinate
 * is pos >> 6 (a full ten bits), not the high byte scaled by four; Y
 * comes from posy and X from posx; and the LABS scale nibble is per
 * object rather than always zero.
 */

#define VEC_SHAPE_WORD(byte_addr) (uint16_t)(0xC000 | (((byte_addr) - 0x8000) >> 1))

/* Ship hull frame tables: ROM 0x30A1 (4 base words, selected by heading
 * bits 4-5 - see dvg_ship_buffers_build for where sets 2 and 3 come
 * from) and ROM 0x30A9 (16 offsets). ROM 0x1E18-0x1E58 builds the
 * hull JSRL as 0xC000 | ((base + offset) & 0x0FFF) and the thrust flame
 * as that word minus 7, because the vector ROM stores the frames as
 * [THRUST_n][SHIP_n] pairs exactly 7 words apart. Heading bit 4 mirrors
 * the frame index (a ^ 0x0F), which is how 64 headings reach 16 frames. */
static const uint16_t SHIP_BASE[4] = { 0x0800, 0x0DFC, 0x05FC, 0x03F8 };
static const uint16_t SHIP_OFFSET[16] = {
    0x0007, 0x0027, 0x0048, 0x0069, 0x008B, 0x00AB, 0x00C9, 0x00E9,
    0x0109, 0x0128, 0x0148, 0x0168, 0x0186, 0x01A6, 0x01C8, 0x01E9
};
#define SHIP_SUBLIST_WORD 0xC0C5    /* ROM (0x3F65): JSRL 0x818A */

/* vector-RAM offsets of the ship sub-list, from the ROM template */
#define SHIP_HULL_SLOT  0x18A
#define SHIP_FLAME_SLOT 0x18C
#define SHIP_FLAME_COPY 0x190

/* The 24 words at ROM 0x30D9 that GAME_DLIST_BUILD copies into the last
 * 48 bytes of the 0x01B0 region (vector RAM 0x8180-0x81AF). They are not
 * object entries: word 0 jumps straight over the block, and the rest is
 * the ship's hull+flame sub-list, a spare RTSL pair, and the two runs of
 * glyph JSRLs that BCD_TO_DIGITS rewrites with the score (0x8196) and
 * the credit count (0x81AA). 0xCD92 is a JSRL to CHAR_SPACE, the blank
 * digit; 0xCD2F is '0'. */
const uint16_t omega_dlist_tail[24] = {   /* exported: dvg_pages.c's
                                             GAME_DLIST_BUILD lays it
                                             into the shadow at 0x4601 */
    0xE0D8, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xC807, 0xC800, 0xD000,
    0xD000, 0xF000, 0xD000, 0xCD92, 0xCD92, 0xCD92, 0xCD92, 0xCD92,
    0xCD92, 0xCD92, 0xCD2F, 0xD000, 0xD000, 0xCD92, 0xCD2F, 0xD000
};
#define DLIST_TAIL_OFF (32 * OBJ_ENTRY_BYTES)     /* 0x180 */

/* The ship's hull and flame JSRLs, as move_mode_dispatch last wrote
 * them. The ROM lays the 0x30D9 template down once per wave and patches
 * these two words in place; this port re-lays the template on every
 * build (see dvg_build_object_list), so it has to re-apply the patch
 * afterwards. Initial values are the template's own. */
static uint16_t s_ship_hull  = 0xC807;
static uint16_t s_ship_flame = 0xC800;

extern void dvg_player_blink(void);       /* dvg_pages.c, 0x179A-0x17B4 */

/* ROM 0x1E18-0x1E58: pick the hull frame for the ship's heading, write
 * the hull and flame JSRLs into the RAM sub-list, and hand the object
 * entry the fixed JSRL that calls it. */
static uint16_t ship_shape_word(obj_rec* o)
{
    uint8_t  ang   = o->angle;
    uint16_t base  = SHIP_BASE[(ang & 0x30) >> 4];
    uint8_t  idx   = (uint8_t)(((ang & 0x10) ? (ang ^ 0x0F) : ang) & 0x0F);
    uint16_t hull  = (uint16_t)(0xC000 | ((base + SHIP_OFFSET[idx]) & 0x0FFF));
    uint16_t flame = (uint16_t)(hull - 7);

    /* 0x1E1B `ld (aim_angle),a`: the heading this frame is drawn at is
     * also the direction anything fired this pass travels in. It sits
     * ahead of the 0x1E9B spawn gate for exactly that reason - without
     * it SPAWN_MINE_SHOT aims a player shot with whatever angle the
     * last enemy AIM_AT_SHIP happened to leave behind. */
    o->aim = ang;    /* 0x1E1B: into the SHIP's own staged aim byte -
                        per-object state; a shared aim cell would clobber
                        the designated shooter's aim every ship pass */

    s_ship_hull  = hull;
    s_ship_flame = flame;
    swrwd(SHIP_HULL_SLOT,  hull);      /* ROM writes the SHADOW's     */
    swrwd(SHIP_FLAME_COPY, flame);     /* sub-list (0x460A/0x4611)    */
    /* 0x818C itself is written per FRAME by ship_flame_update() below,
     * not per stage-run, because that is what VG_RESTART_FRAME does. */
    return SHIP_SUBLIST_WORD;
}

/* ROM 0x1763-0x1778, the thrust-flame strobe. VG_RESTART_FRAME loads
 * hl with RTSL (0xD000, flame blank), and only when the thrust button is
 * held (0x402F bit 5, active low) does it bump the 0x4071 counter and, on
 * odd counts, load hl from 0x8190 instead - the flame JSRL ship_shape_
 * word parked there. Either way hl lands in 0x818C, the second word of
 * the ship's sub-list. So the exhaust alternates on/off every frame it
 * is held, which is what makes it flicker rather than glow steadily.
 * This runs once per displayed frame, from the list build. */
static void ship_flame_update(void)
{
    uint16_t w = 0xD000;                       /* 0x1763: RTSL, no flame */
    if (!(g.in1_state & 0x20)) {               /* 0x1766 bit 5, active low */
        g.thrust_flicker++;                    /* 0x176C inc (iy-15)     */
        if (g.thrust_flicker & 1)              /* 0x176F bit 0           */
            w = s_ship_flame;                  /* 0x1775 ld hl,($4611)   */
    }
    wrwd(SHIP_FLAME_SLOT, w);                  /* 0x1778 ld ($818C),hl   */
}

/* Explosion animation, ROM table 0x3F5D. Four entries, and the first and
 * last are the same frame, so the cycle is 02, 00, 01, 02. */
static const uint16_t SHAPE_EXPLODE[4] = {
    0xCC12, 0xCBBA, 0xCBD8, 0xCC12   /* SHIP_EXPLODE_02, 00, 01, 02 */
};

/* Animation table ROM 0x3F4D, indexed by move_mode_dispatch below.
 * Entries 0-3 are the enemy cycle, 4-7 the death-ship cycle; the ROM
 * picks between the halves with `set 3,a` on the byte offset. */
static const uint16_t SHAPE_ANIM[8] = {
    0xCA04, 0xCA2E, 0xCA51, 0xCA74,   /* CMDSHIP_00, DROID_00/01/02   */
    0xCA97, 0xCAAA, 0xCABE, 0xCAD2    /* DEATH_SHIP_00..03            */
};

/* ROM 0x1DD1 move_mode_dispatch - which of the three shape-word sources
 * an object uses:
 *   flags0 bit 4 clear -> the record's own word (+21/+22), written at
 *                         spawn (SPAWN_MINE_SHOT 0x2742 and friends)
 *   bit 4 set, flags1 bit 6 set -> the heading path, 0x1E18
 *   bit 4 set, bit 6 clear      -> the 4-frame animation table 0x3F4D
 * The flag tests come first: an object whose flags say "animate" must
 * never be drawn from a stale record word.
 *
 * Called from objects.c's move_mode_dispatch, once per stage-run, NOT
 * once per rendered frame: the two arms below advance animation
 * counters in the record and the ship arm publishes aim_angle, so
 * calling at frame rate would tie the explosion and droid cycles to
 * the host's draw rate. The word it returns is cached in
 * g.obj_shape[] and the list builder just copies it, exactly as the
 * hardware entry held the last word written to it. */
uint16_t dvg_obj_shape_select(obj_rec* o)
{
    /* ROM 0x1DB4, the very top of move_mode_dispatch: an object whose
     * flags1 bit 5 is set is dying, and draws the explosion animation
     * instead of anything else. OBJ_DESTROY gets that bit there by
     * stamping the inert template (0x2FAC, flags1 = 0x20) over the
     * record. The frame is angle & 3, and the angle is incremented each
     * pass so it advances - `and 3 / rlca` reads the value BEFORE the
     * `inc (iy-118)` that follows, hence the post-increment here. */
    if (o->flags1 & 0x20)
        return SHAPE_EXPLODE[o->angle++ & 3];

    if (!(o->flags0 & 0x10))                       /* 0x1DD1 */
        return o->shape_word;                      /* 0x1E63 */

    if (o->flags1 & 0x40)                          /* 0x1DD8 */
        return ship_shape_word(o);                 /* 0x1E18 */

    /* 0x1DDE-0x1E16: advance b20 and pick a frame. `rlca / and 6` turns
     * the counter into a byte offset of 0, 2, 4 or 6; bit 3 (i.e. +4
     * entries) selects the death-ship half when flags1 bit 7 is clear. */
    {
        uint8_t a;
        o->b20++;                                  /* 0x1DDE inc (iy-108) */
        a = (uint8_t)(((o->b20 << 1) | (o->b20 >> 7)) & 0x06);   /* rlca */
        if (a == 0) {
            if (o->b14 == 0) {                     /* 0x1DE9 ($400E)  */
                a = 2;
                o->b20 = 1;                        /* 0x1DFD          */
            } else if (o->flags1 & 0x80) {         /* 0x1DF1          */
                o->b20 = 2;                        /* 0x1DF7          */
            }
        }
        if (!(o->flags1 & 0x80))                   /* 0x1E03          */
            a |= 0x08;                             /* set 3,a         */
        return SHAPE_ANIM[(a >> 1) & 7];
    }
}

void dvg_build_object_list(void)
{
    int n, i;

    /* MAINLOOP 0x0FDA: `bit 1,(iy-92)` gates BOTH VG_RESTART_FRAME (the
     * per-frame shadow -> vector RAM copy) and OBJECTS_UPDATE on
     * game_mode bit 1 - only mode 2 (spawn/demo) and mode 3 (play)
     * refresh the object region. In attract mode (1) and high-score
     * entry (4) it stays as DLIST_RESET left it, so the object records
     * alive behind the attract screens - the title ship ATTRACT_INIT
     * spawns, and the wave-4 droid roster the single attract PLAY_FRAME
     * pass creates - are not drawn. */
    if (!(g.game_mode & 0x02)) return;

    /* WAVE_CLEAR_ANIM: the blocking loop never reaches MAINLOOP, so the
     * hardware's per-frame shadow copy cannot run while the bonus pages
     * occupy 0x8000-0x8095 (sealed by the HALTs at 0x8092/0x8094). The
     * port's per-frame anim must skip the build the same way, or the
     * copy would stamp object entries over the page text. */
    if (g.wave_anim_active) return;

    /* POST owns the whole list while it is up. On hardware that needs no
     * guard - POST runs before GAME_INIT and never reaches MAINLOOP - but
     * the host binds F2 to enter the diagnostics from anywhere, including
     * a game in progress, where game_mode is still 3. Without this the
     * per-frame object build would stamp ship and enemy entries straight
     * over the diagnostic screen at 0x8000. */
    if (g.post_active) return;

    /* The tail template (0x4601-0x4630 in the shadow) is laid down by
     * GAME_DLIST_BUILD and persists there - together with the score and
     * credit digit runs the game writes into it. Only the ship's hull
     * and flame words are refreshed here, from what move_mode_dispatch
     * last chose (the ROM patches them in the shadow the same way). */
    swrwd(SHIP_HULL_SLOT,  s_ship_hull);
    swrwd(SHIP_FLAME_COPY, s_ship_flame);

    for (n = 1; n <= OBJ_COUNT; n++) {
        int base = (n - 1) * OBJ_ENTRY_BYTES;
        int word = base >> 1;
        obj_rec* o = &g.obj[n];

        /* Two conditions, not one. An object that has just gone active
         * is not drawn until it has had a stage-run: on the hardware
         * word0 only becomes "jump into my own body" at
         * fsd_slot_init_once (0x1B03), which is also where flags1 bit 2
         * goes up, and until then obj_expire_slot's jump-over stands.
         * Without the second test the entry would go live a frame
         * before move_mode_dispatch has chosen a shape for it. */
        if (!(o->flags0 & 0x80) || !(o->flags1 & 0x04)) {
            /* obj_expire_slot (0x19A9): (entry - 0x8475) >> 1, a JMPL
             * over this entry to the next one. */
            swrwd(base, (uint16_t)(0xE000 | (word + 6)));
            for (i = 1; i < 6; i++) swrwd(base + i * 2, 0xB000);
            continue;
        }
        /* fsd_slot_init_once (0x1B03): (entry - 0x847F) >> 1, a JMPL to
         * this entry's own body. */
        swrwd(base + 0,  (uint16_t)(0xE000 | (word + 1)));
        swrwd(base + 2,  (uint16_t)(0xA000 | (((uint16_t)o->posy >> 6) & 0x3FF)));
        swrwd(base + 4,  (uint16_t)((((uint16_t)(o->b11 & 0xF0)) << 8)
                                    | (((uint16_t)o->posx >> 6) & 0x3FF)));
        /* word3: zero length and zero intensity, so it draws nothing -
         * its opcode nibble is the beam-settling delay obj_proximity_upd
         * computes (ROM 0x1A3C). 0x8000 is the idle value the entry
         * template carries before that runs. */
        swrwd(base + 6,  g.obj_settle[n] ? (uint16_t)(g.obj_settle[n] << 8)
                                         : (uint16_t)0x8000);
        swrwd(base + 8,  0x0000);
        /* word5 is whatever move_mode_dispatch last chose for this
         * object (0x1E5B/0x1E63); the gate above guarantees it has. */
        swrwd(base + 10, g.obj_shape[n]);
    }

    /* VG_RESTART_FRAME 0x1758: copy the whole shadow - entries, tail
     * template, digit runs - into vector RAM, then strobe the flame
     * word in the LIVE list (0x1763-0x1778 writes 0x818C directly) and
     * the 2-player active-score marker (0x179A-0x17B4). */
    memcpy(omega_vecram, omega_dlist_shadow, 0x1B0);
    ship_flame_update();
    dvg_player_blink();

    /* The tail template's first word is 0xE0D8, a JMPL to word 0x0D8 =
     * byte 0x1B0 - straight into the wall list (dvg_walls_install).
     * DLIST_RESET leaves HALTs everywhere, so the walker still stops
     * safely before GAME_DLIST_BUILD has run. */
}

/* GAME_DLIST_BUILD's wall list: the ROM copies its own 0x3109 blob into
 * the display list at 0x81B0, and WALL_FLASH_DECAY then rewrites the
 * per-segment intensity nibbles inside it every frame (0x81BB stride 6).
 * Keeping the walls here, in vector RAM, rather than as a host shape is
 * what lets those writes land on the real bytes - the DVG walker then
 * draws whatever the game last wrote, flash included. */
void dvg_walls_install(void)
{
    int i;
    for (i = 0; i < omega_walls_word_count; i++)
        wrwd(OBJ_REGION + i * 2, omega_walls_words[i]);
}

/* ===========================================================================
 * Authentic DVG timing (the PROM-driven state machine).
 *
 * Transcribed from disasm/frametime.cpp (itself the MAME 0.111 dvg
 * machine, drawing stripped), which was validated against the hardware
 * frame-rate derivation. Every state transition costs 8 master-clock
 * cycles; a gostrobe additionally runs the vector's fin counter, which
 * is where actual beam-drawing time comes from. Master clock = 12 MHz,
 * so ms = master_cycles / 12000.0.
 *
 * One costing entry point: dvg_cost_list(), the exact cost of the live
 * vector-RAM list - the object region and everything it JSRLs into,
 * which is the whole frame.
 * =========================================================================*/

#include "omega_shapes.h"

extern const unsigned char omega_dvgprom[0x100];

typedef struct {
    uint16_t pc;                 /* word address                        */
    uint8_t  sp;
    uint16_t stack[4];
    uint16_t dvx, dvy;
    uint8_t  op, halt, intensity, scale;
    uint8_t  state_latch, data;
} vgtime;

#define VGT_OP0 (vg->op & 1)
#define VGT_OP3 (vg->op & 8)
#define VGT_ST3 (vg->state_latch & 8)

static void vgt_data(vgtime* vg)
{
    vg->data = omega_vecram[(((vg->pc << 1) | (vg->state_latch & 1)) & 0x1FFF)];
}

static int vgt_dmapush(vgtime* vg)
{
    if (VGT_OP0 == 0) { vg->sp = (uint8_t)((vg->sp + 1) & 0xF); vg->stack[vg->sp & 3] = vg->pc; }
    return 0;
}
static int vgt_dmald(vgtime* vg)
{
    if (VGT_OP0) { vg->pc = vg->stack[vg->sp & 3]; vg->sp = (uint8_t)((vg->sp - 1) & 0xF); }
    else         { vg->pc = vg->dvy; }
    return 0;
}
static int vgt_gostrobe(vgtime* vg)
{
    int scale, fin;
    if (vg->op == 0xF)
    {
        scale = (vg->scale + (((vg->dvy & 0x800) >> 11)
            | (((vg->dvx & 0x800) ^ 0x800) >> 10)
            | ((vg->dvx & 0x800) >> 9))) & 0xF;
        vg->dvy &= 0xF00;
        vg->dvx &= 0xF00;
    }
    else
        scale = (vg->scale + vg->op) & 0xF;

    fin = 0xFFF - (((2 << scale) & 0x7FF) ^ 0xFFF);
    return 8 * fin;
}
static int vgt_haltstrobe(vgtime* vg) { vg->halt = (uint8_t)VGT_OP0; return 0; }
static int vgt_latch3(vgtime* vg)
{
    vg->dvx = (uint16_t)((vg->dvx & 0xFF) | ((vg->data & 0xF) << 8));
    vg->intensity = (uint8_t)(vg->data >> 4);
    return 0;
}
static int vgt_latch2(vgtime* vg)
{
    vg->dvx &= 0xF00;
    if (vg->op != 0xF) vg->dvx = (uint16_t)((vg->dvx & 0xF00) | vg->data);
    if ((vg->op & 0xA) == 0xA) vg->scale = vg->intensity;
    vg->pc++;
    return 0;
}
static int vgt_latch1(vgtime* vg)
{
    /* NB: latch1 does NOT advance pc (only latch0 and latch2 do) */
    vg->dvy = (uint16_t)((vg->dvy & 0xFF) | ((vg->data & 0xF) << 8));
    vg->op = (uint8_t)(vg->data >> 4);
    if (vg->op == 0xF) { vg->dvx &= 0xF00; vg->dvy &= 0xF00; }
    return 0;
}
static int vgt_latch0(vgtime* vg)
{
    vg->dvy &= 0xF00;
    if (vg->op == 0xF) vgt_latch3(vg);
    else vg->dvy = (uint16_t)((vg->dvy & 0xF00) | vg->data);
    vg->pc++;
    return 0;
}

typedef int (*vgt_handler)(vgtime*);
static const vgt_handler vgt_handlers[8] =
{
    vgt_dmapush, vgt_dmald, vgt_gostrobe, vgt_haltstrobe,
    vgt_latch0, vgt_latch1, vgt_latch2, vgt_latch3
};

static uint8_t vgt_state_addr(vgtime* vg)
{
    uint8_t addr = (uint8_t)(((((vg->state_latch >> 4) ^ 1) & 1) << 7)
                             | (vg->state_latch & 0xF));
    if (VGT_OP3) addr |= (uint8_t)((vg->op & 7) << 4);
    return addr;
}

/* Run the machine from a freshly kicked state until HALT; master cycles.
 * The kick enters through the PROM idle-exit path - a dmald with op=0 -
 * which loads pc FROM dvy, so the start word address goes in via dvy
 * (the hardware restart at 0 works exactly this way; see frametime.cpp
 * dvg_run_list). Setting pc directly is ignored. */
/* diagnostics: how many state transitions the last walk took and
 * whether it reached a HALT. A walk that hits the guard means the list
 * is malformed (or vector RAM was never initialised) and its cost is
 * meaningless. */
int dvg_dbg_iters, dvg_dbg_halted;

static int32_t vgt_run(vgtime* vg, uint16_t start_word)
{
    int32_t cycles = 0;
    int guard = 400000;
    dvg_dbg_iters = 0; dvg_dbg_halted = 0;

    vg->dvy = start_word; vg->op = 0; vg->halt = 0;

    while (guard--)
    {
        dvg_dbg_iters++;
        vg->state_latch = (uint8_t)((vg->state_latch & 0x10)
            | (omega_dvgprom[vgt_state_addr(vg)] & 0xF));

        if (VGT_ST3)
        {
            vgt_data(vg);
            cycles += vgt_handlers[vg->state_latch & 7](vg);
        }

        if (vg->halt && !(vg->state_latch & 0x10))
        {
            vg->state_latch = (uint8_t)((vg->halt << 4) | (vg->state_latch & 0xF));
            cycles += 8;
            dvg_dbg_halted = 1;
            break;
        }
        vg->state_latch = (uint8_t)((vg->halt << 4) | (vg->state_latch & 0xF));
        cycles += 8;
    }
    return cycles;
}

/* exact cost of the live display list (word 0 onward) */
/* PERSISTENT machine state, like frametime.cpp's static vgs: the
 * hardware's state latch carries across frames - a kick only clears
 * halt and the machine re-enters through the PROM's own idle-exit
 * path. Entering with a zero latch walks simple lists but DESYNCS on
 * the real game list: the op latch never recovers and the walk
 * free-runs to its guard, pricing every play frame at the 50 ms cap.
 * A guard trip leaves mid-op garbage in the latch that would poison
 * every later walk the same way, so prime back to authentic idle by
 * running a one-word HALT list, exactly what the machine has just
 * done after any real frame. */
static vgtime  s_cost_vg;

static void vgt_prime_idle(void)
{
    uint8_t save0 = omega_vecram[0], save1 = omega_vecram[1];
    memset(&s_cost_vg, 0, sizeof s_cost_vg);
    omega_vecram[0] = 0x00; omega_vecram[1] = 0xB0;    /* HALT */
    vgt_run(&s_cost_vg, 0);
    omega_vecram[0] = save0; omega_vecram[1] = save1;
}

int32_t dvg_cost_list(void)
{
    int32_t c = vgt_run(&s_cost_vg, 0);
    if (!dvg_dbg_halted)
        vgt_prime_idle();          /* don't let one bad walk poison all */
    return c;
}

/* Whole-frame period in milliseconds:
 *   DVG time (the exact list walk, master/12000 ms)
 * + the serialized Z80 work the port does not execute. frametime.exe's
 *   hardware derivation measured that cpu-gap at ~5.3 ms in modes 2/3
 *   (the 0x1B0-byte VG_RESTART_FRAME copy + OBJECTS_UPDATE + FRAME_
 *   ENGINE between kicks) and ~0.35 ms in attract, where MAINLOOP skips
 *   all of it. */
double dvg_cost_frame_ms(void)
{
    double master = (double)dvg_cost_list();
    double gap_ms = (g.game_mode & 0x02) ? 5.30 : 0.35;
    double ms = master / 12000.0 + gap_ms;

    /* A malformed list runs the walk to its guard and prices out at
     * seconds per frame; handing that to the host's pacer would stall
     * the game outright. The real machine's worst measured frame is
     * ~36 ms (frametime's histogram), so anything past 50 ms means the
     * list is broken, not slow - pace at the cap and keep running.
     * dvg_render has its own, tighter guard, so the frame still draws
     * (garbled) rather than hanging. */
    if (!(ms >= 0.0) || ms > 50.0) ms = 50.0;
    return ms;
}
