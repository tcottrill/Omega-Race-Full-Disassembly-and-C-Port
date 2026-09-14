// frametime.cpp - headless Omega Race frame-rate derivation harness.
//
// Runs the real ROMs on the rig's Z80 core with a cycle-accurate model of
// the DVG (ported from AAE mame_late_avgdvg.cpp, the MAME 0.111 PROM-driven
// state machine, drawing stripped - only cycle counting).  No graphics, no
// throttling: everything runs on the emulated timebase, so the measured
// kick-to-kick period IS the hardware frame period.
//
//   frame period = Z80 overhead (list copy + polling) + DVG list draw time
//
// Build (VS dev prompt, from disasm\; cpu_z80 and its headers are
// vendored here):
//   cl /O2 /EHsc frametime.cpp cpu_z80.cpp /Fe:frametime.exe
//
// Inputs: omega_dump.bin and dvgprom.bin beside the exe, both produced
// by gen_from_roms.py from a user-supplied MAME omegrace set.
//
// The Z80 runs at 12MHz/4 = 3.0 MHz.  The DVG master clock is the same
// 12 MHz crystal (MAME models it as 12.096 MHz; on the Midway board it is
// 12.000 MHz - only a 0.8% difference).  We use the exact 4:1 ratio:
// one Z80 cycle == 4 DVG master cycles.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cpu_z80.h"

int wrlog(char* format, ...) { return 0; }   // stub for cpu core logging

static cpu_z80* mz80;
static uint8_t* MEM;

//////////////////////// DVG timing simulator ////////////////////////////////

static uint8_t state_prom[0x100];

struct vgdata
{
    uint16_t pc;         // word address
    uint8_t  sp;
    uint16_t stack[4];
    uint16_t dvx, dvy;
    uint8_t  op, halt, intensity, scale;
    uint8_t  state_latch, data;
};
static vgdata vgs;

#define OP0 (vg->op & 1)
#define OP3 (vg->op & 8)
#define ST3 (vg->state_latch & 8)

static void dvg_data(vgdata* vg)
{
    // vector memory: 0x8000-0x9fff, pc is a 12-bit word address
    vg->data = MEM[0x8000 + (((vg->pc << 1) | (vg->state_latch & 1)) & 0x1fff)];
}

static int dvg_dmapush(vgdata* vg)
{
    if (OP0 == 0) { vg->sp = (vg->sp + 1) & 0xf; vg->stack[vg->sp & 3] = vg->pc; }
    return 0;
}
static uint32_t shape_used[0x1000];   // JSRL/JMPL entry census by word addr

static int dvg_dmald(vgdata* vg)
{
    if (OP0) { vg->pc = vg->stack[vg->sp & 3]; vg->sp = (vg->sp - 1) & 0xf; }
    else     { vg->pc = vg->dvy; shape_used[vg->dvy & 0xfff]++; }
    return 0;
}
static int dvg_gostrobe(vgdata* vg)   // timing only, no drawing
{
    int scale;
    if (vg->op == 0xf)
    {
        scale = (vg->scale + (((vg->dvy & 0x800) >> 11)
            | (((vg->dvx & 0x800) ^ 0x800) >> 10)
            | ((vg->dvx & 0x800) >> 9))) & 0xf;
        vg->dvy &= 0xf00;
        vg->dvx &= 0xf00;
    }
    else
        scale = (vg->scale + vg->op) & 0xf;

    int fin = 0xfff - (((2 << scale) & 0x7ff) ^ 0xfff);
    return 8 * fin;
}
static int dvg_haltstrobe(vgdata* vg) { vg->halt = OP0; return 0; }
static int dvg_latch3(vgdata* vg)
{
    vg->dvx = (vg->dvx & 0xff) | ((vg->data & 0xf) << 8);
    vg->intensity = vg->data >> 4;
    return 0;
}
static int dvg_latch2(vgdata* vg)
{
    vg->dvx &= 0xf00;
    if (vg->op != 0xf) vg->dvx = (vg->dvx & 0xf00) | vg->data;
    if ((vg->op & 0xa) == 0xa) vg->scale = vg->intensity;
    vg->pc++;
    return 0;
}
static int dvg_latch1(vgdata* vg)
{
    // NB: latch1 does NOT advance pc (only latch0 and latch2 do)
    vg->dvy = (vg->dvy & 0xff) | ((vg->data & 0xf) << 8);
    vg->op = vg->data >> 4;
    if (vg->op == 0xf) { vg->dvx &= 0xf00; vg->dvy &= 0xf00; }
    return 0;
}
static int dvg_latch0(vgdata* vg)
{
    vg->dvy &= 0xf00;
    if (vg->op == 0xf) dvg_latch3(vg);
    else vg->dvy = (vg->dvy & 0xf00) | vg->data;
    vg->pc++;
    return 0;
}

typedef int (*handler_t)(vgdata*);
static handler_t handlers[8] =
{
    dvg_dmapush, dvg_dmald, dvg_gostrobe, dvg_haltstrobe,
    dvg_latch0, dvg_latch1, dvg_latch2, dvg_latch3
};

static uint8_t dvg_state_addr(vgdata* vg)
{
    uint8_t addr = ((((vg->state_latch >> 4) ^ 1) & 1) << 7) | (vg->state_latch & 0xf);
    if (OP3) addr |= ((vg->op & 7) << 4);
    return addr;
}

// Run the whole list to HALT; returns total DVG master-clock cycles.
// Also counts vector-op stats for the report.
static int stat_vctr, stat_svec, stat_jsrl, stat_labs, stat_words;

static int64_t dvg_run_list(void)
{
    vgdata* vg = &vgs;
    int64_t cycles = 0;
    int guard = 2000000;

    // avgdvg_go semantics: dvg_vggo + vg_set_halt(0). pc restart happens
    // through the PROM's own idle-exit path (dmald with op=0 -> pc=dvy=0).
    // state_latch persists across frames like the hardware latch.
    vg->dvy = 0; vg->op = 0; vg->halt = 0;

    stat_vctr = stat_svec = stat_jsrl = stat_labs = stat_words = 0;

    while (guard--)
    {
        vg->state_latch = (vg->state_latch & 0x10)
            | (state_prom[dvg_state_addr(vg)] & 0xf);

        if (ST3)
        {
            dvg_data(vg);
            int h = vg->state_latch & 7;
            cycles += handlers[h](vg);
            if (h == 2)   // gostrobe = one vector drawn
            {
                if (vg->op == 0xf) stat_svec++; else stat_vctr++;
            }
            if (h == 5) { stat_words++;                       // latch1 = new opcode word
                if (vg->data >> 4 == 0xc) stat_jsrl++;
                if (vg->data >> 4 == 0xa) stat_labs++; }
        }

        if (vg->halt && !(vg->state_latch & 0x10))
        {
            vg->state_latch = (vg->halt << 4) | (vg->state_latch & 0xf);
            cycles += 8;
            break;
        }
        vg->state_latch = (vg->halt << 4) | (vg->state_latch & 0xf);
        cycles += 8;
    }
    return cycles;
}

//////////////////////// emulated timebase / DVG busy ////////////////////////

#define CPU_CLOCK 3000000
// Board IRQ = 244.140625 Hz, read off the schematic (sheet omegaM2):
// 12 MHz /16 (N8 74161 QD) /256 (S5 74393 2QD) /12 (R6 74161, preset 4
// with RCO fed back to LOAD) -> 12 MHz / 49152, i.e. exactly 12288 Z80
// cycles at 3 MHz. The traditional 250 Hz (12000 cycles) is 2.4% fast
// and is not producible by this divider chain at all.
#define CYCLES_PER_INT 12288

static uint64_t cycles_base = 0;
static uint64_t emu_cycles(void) { return cycles_base + mz80->mz80GetElapsedTicks(0); }

static uint64_t dvg_busy_until = 0;
static int dvg_busy(void) { return emu_cycles() < dvg_busy_until; }

// --lock60: the AAE omegrace60 model. "Done" is reported once per 60 Hz frame
// (set at the frame boundary, cleared on go) and the IRQ fires 4x per frame
// (240 Hz) at fixed positions. Frame = 50000 Z80 cycles at 3 MHz.
static int lock60 = 0;
static int lock_done = 1;
#define LOCK_CYCLES_PER_INT 12500

// --passrate-counters: write-event counters (see the write map)
static int64_t ev_pass = 0;     // writes to tick_delta 0x4019 (one per MAINLOOP head pass)
static int64_t ev_dec_407C = 0; // decrements of difficulty countdown (iy-4), ROM 0x1DA0
static int64_t ev_dec_407F = 0; // decrements of special-spawn arm (iy-1), ROM 0x1C72
static int64_t ev_dec_406D = 0; // decrements of 2P turn-banner hold, ROM 0x0D35
static int64_t ev_kicks = 0;    // VG kicks

//////////////////////// frame statistics ////////////////////////////////////

struct frame_rec
{
    uint64_t kick_cycle;      // Z80 cycle of the kick
    int64_t  draw_master;     // DVG master cycles for the list
    uint8_t  game_mode;       // MEM[0x4024]
    int      words;           // opcode words in list
};
#define MAXFRAMES 200000
static frame_rec frames[MAXFRAMES];
static int nframes = 0;
static uint64_t last_done_cycle = 0;   // when DVG went idle

//////////////////////// input scripting /////////////////////////////////////

static uint8_t in_coin = 0xff;     // port 0x11 (bit0 low = coin)
static uint8_t in_start = 0xff;    // port 0x12 (bit6 low = start1)

//////////////////////// port handlers ///////////////////////////////////////

static UINT16 r_watchdog(UINT16, struct z80PortRead*) { return 0; }
static UINT16 r_vg_status(UINT16, struct z80PortRead*)
{
    if (lock60) return lock_done ? 0x00 : 0x80;
    return dvg_busy() ? 0x80 : 0x00;
}

static UINT16 r_vg_go(UINT16, struct z80PortRead*)
{
    if (lock60) lock_done = 0;
    else if (dvg_busy()) return 0;
    ev_kicks++;

    int64_t master = dvg_run_list();
    uint64_t now = emu_cycles();

    if (nframes < MAXFRAMES)
    {
        frames[nframes].kick_cycle = now;
        frames[nframes].draw_master = master;
        frames[nframes].game_mode = MEM[0x4024];
        frames[nframes].words = stat_words;
        nframes++;
    }
    dvg_busy_until = now + (uint64_t)(master / 4);   // exact 4:1 master:Z80
    return 0;
}

static UINT16 r_dsw_c4(UINT16, struct z80PortRead*) { return 0x5f; }
static UINT16 r_dsw_c6(UINT16, struct z80PortRead*) { return 0xbf; }
static UINT16 r_in_p1(UINT16, struct z80PortRead*) { return in_coin; }
static UINT16 r_in_p2(UINT16, struct z80PortRead*) { return in_start; }
static UINT16 r_spin(UINT16, struct z80PortRead*) { return 0x04; }
static void w_ignore(UINT16, UINT8, struct z80PortWrite*) {}
static void w_rom(UINT32, UINT8, struct MemoryWriteByte*) {}

// Event counters for --passrate-counters. The handler must store the byte
// itself (a mapped write bypasses the core's plain RAM store).
static void w_count(UINT32 a, UINT8 d, struct MemoryWriteByte* map)
{
    // FIX (not in the plan text): cpu_z80::mz80PutMemory() calls
    // memoryCall(addr - lowAddr, byte, mapEntry) - the first param is the
    // offset WITHIN the mapped range, not the absolute address (see
    // cpu_z80.cpp mz80PutMemory). Each of these four ranges is a single
    // byte (lowAddr == highAddr), so that offset is always 0 and a plain
    // "switch (a)" on the raw param never matches, leaving every counter
    // at 0. Recover the absolute address from the map entry's lowAddr.
    UINT32 addr = map->lowAddr + a;
    uint8_t old = MEM[addr];
    MEM[addr] = d;
    switch (addr)
    {
    case 0x4019: ev_pass++; break;
    case 0x407C: if (d == (uint8_t)(old - 1)) ev_dec_407C++; break;
    case 0x407F: if (d == (uint8_t)(old - 1)) ev_dec_407F++; break;
    case 0x406D: if (d == (uint8_t)(old - 1)) ev_dec_406D++; break;
    }
}

static struct MemoryWriteByte WriteMap[] =
{
    { 0x0000, 0x3fff, w_rom },
    { 0x4019, 0x4019, w_count },
    { 0x406D, 0x406D, w_count },
    { 0x407C, 0x407C, w_count },
    { 0x407F, 0x407F, w_count },
    { 0x9000, 0x9fff, w_rom },
    { (UINT32)-1, (UINT32)-1, NULL }
};
static struct MemoryReadByte ReadMap[] =
{
    { (UINT32)-1, (UINT32)-1, NULL }
};
static struct z80PortRead PortRead[] =
{
    { 0x08, 0x08, r_vg_go },
    { 0x09, 0x09, r_watchdog },
    { 0x0b, 0x0b, r_vg_status },
    { 0x10, 0x10, r_dsw_c4 },
    { 0x17, 0x17, r_dsw_c6 },
    { 0x11, 0x11, r_in_p1 },
    { 0x12, 0x12, r_in_p2 },
    { 0x15, 0x15, r_spin },
    { 0x16, 0x16, r_spin },
    { (UINT16)-1, (UINT16)-1, NULL }
};
static struct z80PortWrite PortWrite[] =
{
    { 0x0a, 0x0a, w_ignore },
    { 0x13, 0x13, w_ignore },
    { 0x14, 0x14, w_ignore },
    { (UINT16)-1, (UINT16)-1, NULL }
};

//////////////////////// rom loading /////////////////////////////////////////

static int load(const char* path, uint8_t* dst, int size)
{
    FILE* f = fopen(path, "rb");
    if (!f) { printf("cannot open %s\n", path); return 0; }
    fread(dst, 1, size, f);
    fclose(f);
    return 1;
}

//////////////////////// report //////////////////////////////////////////////

static const char* mode_name(int m)
{
    switch (m) {
    case 1: return "attract ";
    case 2: return "spawn/demo";
    case 3: return "play    ";
    case 4: return "hiscore ";
    default: return "boot    ";
    }
}

static void report(const char* tag, int from, int to)
{
    if (to - from < 2) return;

    // aggregate by game mode
    for (int mode = 0; mode <= 4; mode++)
    {
        int64_t sum_period = 0, sum_draw = 0, sum_words = 0;
        int64_t min_period = INT64_MAX, max_period = 0;
        int n = 0;
        for (int i = from + 1; i < to; i++)
        {
            if (frames[i].game_mode != mode) continue;
            int64_t period = (int64_t)(frames[i].kick_cycle - frames[i - 1].kick_cycle);
            if (period <= 0 || period > CPU_CLOCK) continue; // skip stalls >1s
            sum_period += period;
            sum_draw += frames[i].draw_master / 4;
            sum_words += frames[i].words;
            if (period < min_period) min_period = period;
            if (period > max_period) max_period = period;
            n++;
        }
        if (n < 5) continue;
        double avg_p = (double)sum_period / n;              // Z80 cycles
        double avg_d = (double)sum_draw / n;
        printf("%s mode=%s  frames=%5d  fps avg=%6.1f (min=%5.1f max=%6.1f)  "
            "period=%6.2fms  draw=%5.2fms  cpu-gap=%5.2fms  words=%d\n",
            tag, mode_name(mode), n,
            CPU_CLOCK / avg_p, CPU_CLOCK / (double)max_period, CPU_CLOCK / (double)min_period,
            avg_p * 1000.0 / CPU_CLOCK,
            avg_d * 1000.0 / CPU_CLOCK,
            (avg_p - avg_d) * 1000.0 / CPU_CLOCK,
            (int)(sum_words / n));
    }
}

//////////////////////// main ////////////////////////////////////////////////

static void run_seconds(double secs)
{
    if (lock60)
    {
        // 60 frames/s x 4 IRQ slices of 12500 cycles; done at each frame end.
        // FIX (not in the plan text): a persistent fractional carry and a
        // persistent slice-phase counter, instead of per-call truncation.
        // --passrate-counters times play in 0.02 s steps; 0.02*240 = 4.8,
        // so computing slices fresh per call as (int64_t)(secs*240) drops
        // 0.8 slice on every call (4 executed instead of 4.8), understating
        // elapsed emulated time by ~17% and reporting 50.00 kicks/s instead
        // of the exact 60.00 the 240 Hz tick guarantees. The carry recovers
        // the lost fraction across calls; slice_phase keeps the "done every
        // 4th slice" cadence aligned across call boundaries instead of
        // restarting the 0..3 count at 0 on every run_seconds() call.
        static double carry = 0.0;
        static int64_t slice_phase = 0;
        carry += secs * 240.0;
        int64_t slices = (int64_t)carry;
        carry -= (double)slices;
        for (int64_t i = 0; i < slices; i++)
        {
            mz80->mz80exec(LOCK_CYCLES_PER_INT);
            mz80->mz80int(0xff);
            cycles_base += mz80->mz80GetElapsedTicks(0xff);
            if ((++slice_phase & 3) == 0) lock_done = 1;
        }
        return;
    }
    int64_t slices = (int64_t)(secs * 250);
    for (int64_t i = 0; i < slices; i++)
    {
        mz80->mz80exec(CYCLES_PER_INT);
        mz80->mz80int(0xff);
        cycles_base += mz80->mz80GetElapsedTicks(0xff);
    }
}

static void selftest(void)
{
    // list 1: immediate HALT
    memset(MEM + 0x8000, 0, 0x1000);
    MEM[0x8000] = 0x00; MEM[0x8001] = 0xB0;
    int64_t c = dvg_run_list();
    printf("selftest halt-only:  master=%lld  words=%d (expect tiny)\n",
        (long long)c, stat_words);

    // list 2: LABS(0,0,scale0) + VCTR op6 dx=100 dy=0 z=7 + HALT
    uint8_t l2[] = { 0x00, 0xA0, 0x00, 0x00,   // LABS y=0 | x=0 scale=0
                     0x00, 0x60, 0x64, 0x70,   // VCTR op6: y=0, x=0x064, z=7
                     0x00, 0xB0 };
    memset(MEM + 0x8000, 0, 0x1000);
    memcpy(MEM + 0x8000, l2, sizeof(l2));
    c = dvg_run_list();
    // op6, scale0 -> scale=6, fin=(2<<6)&0x7ff=128, draw=8*128=1024 master
    printf("selftest labs+vctr:  master=%lld  words=%d (expect ~1024+overhead, 3 words)\n",
        (long long)c, stat_words);

    // list 3: JSRL into vector ROM char, then HALT (shape at word 0x800)
    uint8_t l3[] = { 0x00, 0xA0, 0x00, 0x00,
                     0x00, 0xC8,               // JSRL $800
                     0x00, 0xB0 };
    memset(MEM + 0x8000, 0, 0x1000);
    memcpy(MEM + 0x8000, l3, sizeof(l3));
    // need the vector ROM present for this test - caller loads ROMs first
    c = dvg_run_list();
    printf("selftest jsrl-shape: master=%lld  words=%d\n", (long long)c, stat_words);
}

int main(int argc, char** argv)
{
    MEM = (uint8_t*)calloc(1, 0x10000);

    // omega_dump.bin: the 64 KB address-space image (program 0x0000-0x3FFF,
    // vector ROM 0x9000-0x9FFF) that gen_from_roms.py assembles from a
    // MAME omegrace set. RAM regions are zero, same as the calloc state.
    if (!load("omega_dump.bin", MEM, 0x10000)) return 1;

    uint8_t rawprom[0x100];
    if (!load("dvgprom.bin", rawprom, 0x100)) return 1;
    // Omega Race swaps two pairs of PROM outputs; un-swap to standard DVG
    // (AAE init_omega: BITSWAP8(x, 7,6,5,4,1,0,3,2))
    for (int i = 0; i < 0x100; i++)
    {
        uint8_t v = rawprom[i];
        state_prom[i] = (v & 0xf0)
            | (((v >> 1) & 1) << 3)
            | (((v >> 0) & 1) << 2)
            | (((v >> 3) & 1) << 1)
            | (((v >> 2) & 1) << 0);
    }

    if (argc > 1 && !strcmp(argv[1], "--selftest"))
    {
        selftest();
        return 0;
    }

    mz80 = new cpu_z80(MEM, ReadMap, WriteMap, PortRead, PortWrite, 0xffff, 0);
    mz80->mz80reset();

    if (argc > 1 && !strcmp(argv[1], "--test"))
    {
        // hold the test switch (port 0x11 bit7 low): runs the full POST /
        // diagnostic loop; census then shows the diagnostic-mode shapes.
        in_coin = 0x7f;
        run_seconds(30.0);
        // advance through the test pages with the fire button (bit6 low)
        for (int i = 0; i < 6; i++)
        {
            in_coin = 0x3f; run_seconds(0.2);
            in_coin = 0x7f; run_seconds(5.0);
        }
        printf("diagnostic-mode shape census:\n");
        for (int wa = 0x800; wa < 0x1000; wa++)
            if (shape_used[wa])
                printf("  %04X:%u", 0x8000 + wa * 2, shape_used[wa]);
        printf("\n");
        return 0;
    }

    printf("=== Omega Race frame-rate derivation (authentic DVG timing) ===\n");
    printf("Z80 3.000 MHz, DVG master = 4x Z80 clock, PROM-driven state machine\n\n");

    if (argc > 1 && !strcmp(argv[1], "--dump-vram"))
    {
        // coin up, start, play a bit, then dump the live display list so
        // the C port can replicate the exact per-object entry format
        run_seconds(20.0);
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff; run_seconds(0.5);
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        run_seconds(8.0);
        printf("game_mode=%02X  vector RAM 0x8000-0x81BF as DVG words:\n",
            MEM[0x4024]);
        for (int a = 0x8000; a < 0x8380; a += 12)
        {
            printf("%04X (obj %2d):", a, (a - 0x8000) / 12 + 1);
            for (int w = 0; w < 12; w += 2)
                printf(" %04X", MEM[a + w] | (MEM[a + w + 1] << 8));
            printf("\n");
        }
        printf("\nship record 0x40B2: ");
        for (int i = 0; i < 23; i++) printf("%02X ", MEM[0x40B2 + i]);
        printf("\ndroid record 0x40C9: ");
        for (int i = 0; i < 23; i++) printf("%02X ", MEM[0x40C9 + i]);
        printf("\n");
        return 0;
    }

    if (argc > 2 && !strcmp(argv[1], "--vgt-file"))
    {
        // run the reference timing machine over a raw 0x2000-byte
        // vector-RAM image (the C port's live list) - differential
        // check of the port's cost walker
        FILE* fp = fopen(argv[2], "rb");
        if (!fp) { printf("cannot open %s\n", argv[2]); return 1; }
        fread(MEM + 0x8000, 1, 0x2000, fp);
        fclose(fp);
        int64_t c = dvg_run_list();
        printf("reference machine: cycles=%lld (%.3f ms) words=%d "
               "vctr=%d svec=%d jsrl=%d labs=%d\n",
            (long long)c, (double)c / 12000.0, stat_words,
            stat_vctr, stat_svec, stat_jsrl, stat_labs);
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--credit-trace"))
    {
        // what the battery bytes 0x5C56/57 hold around coin and start -
        // the reference for the C port's credit persistence
        auto show = [](const char* tag) {
            printf("%-12s credits_bcd=%02X  nvram 5C56=%02X 5C57=%02X\n",
                tag, MEM[0x402D], MEM[0x5C56], MEM[0x5C57]);
        };
        run_seconds(3.0);  show("boot");
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff; run_seconds(0.8);
        show("after coin");
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        run_seconds(1.0);  show("after start");
        return 0;
    }

    if (argc > 2 && !strcmp(argv[1], "--play-vram"))
    {
        // coin at 3s, start at 4s, then dump at N seconds total -
        // reference grid for the C port's game-HUD display list
        // (compare against test_drive.exe --playvram N).
        run_seconds(3.0);
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff;
        run_seconds(0.8);
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        run_seconds(atof(argv[2]) - 4.2);
        printf("vram @%ss mode=%02X\n", argv[2], MEM[0x4024]);
        for (int a = 0x8000; a < 0xA000; a += 16)
        {
            printf("%04X:", a);
            for (int w = 0; w < 16; w += 2)
                printf(" %04X", MEM[a + w] | (MEM[a + w + 1] << 8));
            printf("\n");
        }
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--passrate-counters"))
    {
        // Per-second rates of the MAINLOOP head pass and of the three
        // pass-denominated counters' decrements, in idle play (mode 3),
        // stock DVG-busy model vs the AAE omegrace60 lock ("lock60" arg).
        // The question: does frame-locking done at 60 Hz change the
        // cadence of per-pass logic? Hardware pacing floats 43-52 fps.
        lock60 = (argc > 2 && !strcmp(argv[2], "lock60"));
        run_seconds(3.0);
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff;
        run_seconds(0.8);
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        run_seconds(1.0);                       // spawn -> play settles
        ev_pass = ev_dec_407C = ev_dec_407F = ev_dec_406D = ev_kicks = 0;
        double secs = 0.0;
        for (int step = 0; step < 30 * 50; step++)
        {
            if (MEM[0x4024] != 3) break;        // ship lost -> stop timing
            run_seconds(0.02);
            secs += 0.02;
        }
        printf("%s: %.2f s in play (mode 3)\n", lock60 ? "lock60" : "stock", secs);
        printf("  kicks/s        = %8.2f\n", ev_kicks / secs);
        printf("  head passes/s  = %8.2f   (tick_delta 0x4019 writes)\n", ev_pass / secs);
        printf("  dec 0x407C /s  = %8.3f   (difficulty countdown, ROM 0x1DA0)\n", ev_dec_407C / secs);
        printf("  dec 0x407F /s  = %8.3f   (special-spawn arm, ROM 0x1C72)\n", ev_dec_407F / secs);
        printf("  dec 0x406D /s  = %8.3f   (2P turn-banner hold, ROM 0x0D35; 0 in 1P play)\n", ev_dec_406D / secs);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--passrate-trace"))
    {
        // Distribution of tick_delta (0x4019, written with B on every
        // MAINLOOP head pass at 0x0F45) sampled per 244 Hz slice during
        // the title reveal - the direct record of how many ticks elapse
        // between head passes, i.e. what 0x0FCC subtracts from
        // fire_cooldown each pass.
        // default: sample during the title reveal (page 1). With
        // "play": coin at 3s, start at 4s, then sample during mode 3 -
        // the frame-locked-or-not question for the play arm.
        int play_mode = (argc > 2 && !strcmp(argv[2], "play"));
        if (play_mode)
        {
            run_seconds(3.0);
            in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff;
            run_seconds(0.8);
            in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        }
        int dhist[32] = { 0 };
        int64_t title_ticks = 0;
        for (int64_t tick = 0; tick < 250 * 25; tick++)
        {
            mz80->mz80exec(CYCLES_PER_INT);
            mz80->mz80int(0xff);
            if (play_mode) { if (MEM[0x4024] != 3) continue; }
            else if (MEM[0x4084] != 1) continue;     // title page only
            title_ticks++;
            uint8_t d = MEM[0x4019];
            if (d < 32) dhist[d]++;
        }
        printf("%s: %lld ticks; tick_delta distribution "
               "(per-slice samples):\n",
               play_mode ? "play (mode 3)" : "title reveal",
               (long long)title_ticks);
        for (int i = 0; i < 32; i++)
            if (dhist[i]) printf("  %2d: %d\n", i, dhist[i]);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--scrawl-trace"))
    {
        // Per-tick sampling of the title (page 1) reveal: one slice =
        // one 244.14 Hz interrupt. Any movement of the 0x4089/0x408A
        // line/glyph pair is one PAGE_SCRIPT_STEP emission; the gaps
        // between emissions, in ticks, are the machine's true scrawl
        // pacing (reference for test_drive --authtime).
        int calls = 0; int64_t t_first = -1, t_last = -1;
        uint8_t p89 = 0xff, p8A = 0xff;
        int gap_hist[64] = { 0 };
        for (int64_t tick = 0; tick < 250 * 25; tick++)
        {
            mz80->mz80exec(CYCLES_PER_INT);
            mz80->mz80int(0xff);
            if (MEM[0x4084] != 1) continue;          // title page only
            uint8_t cur = MEM[0x4089], gly = MEM[0x408A];
            if (cur != p89 || gly != p8A)
            {
                if (t_first < 0) t_first = tick;
                else
                {
                    int gap = (int)(tick - t_last);
                    if (gap >= 0 && gap < 64) gap_hist[gap]++;
                }
                t_last = tick;
                calls++;
                p89 = cur; p8A = gly;
            }
        }
        printf("title reveal: %d emissions, tick %lld..%lld = %.2f s "
               "(%.2f ticks/emission avg)\n", calls,
               (long long)t_first, (long long)t_last,
               (t_last - t_first) / 244.140625,
               calls > 1 ? (double)(t_last - t_first) / (calls - 1) : 0.0);
        printf("inter-emission gap histogram (ticks: count):\n");
        for (int i = 0; i < 64; i++)
            if (gap_hist[i]) printf("  %2d: %d\n", i, gap_hist[i]);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "--credit-vram"))
    {
        // coin at 3s but never start, then dump at N seconds total -
        // the reference for the credit/pricing page ("PRESS ONE OF THE
        // FLASHING BUTTONS" / "BONUS SHIPS AT"), which --play-vram can
        // never capture because it always starts the game at 4.0s.
        // (compare against test_drive.exe --playvram N for N < 4.)
        run_seconds(3.0);
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff;
        run_seconds(atof(argv[2]) - 3.2);
        printf("vram @%ss mode=%02X step=%02X page=%02X\n",
            argv[2], MEM[0x4024], MEM[0x4083], MEM[0x4084]);
        for (int a = 0x8000; a < 0xA000; a += 16)
        {
            printf("%04X:", a);
            for (int w = 0; w < 16; w += 2)
                printf(" %04X", MEM[a + w] | (MEM[a + w + 1] << 8));
            printf("\n");
        }
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "--attract-vram"))
    {
        // run N seconds of attract, then dump vector RAM 0x8000-0x83FF
        // as words - the reference for the C port's DVG page renderer
        // (compare against test_drive.exe --vram N).
        run_seconds(atof(argv[2]));
        printf("vram @%ss mode=%02X step=%02X page=%02X\n",
            argv[2], MEM[0x4024], MEM[0x4083], MEM[0x4084]);
        for (int a = 0x8000; a < 0xA000; a += 16)
        {
            printf("%04X:", a);
            for (int w = 0; w < 16; w += 2)
                printf(" %04X", MEM[a + w] | (MEM[a + w + 1] << 8));
            printf("\n");
        }
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--attract-trace"))
    {
        // authentic attract-state trace: one line per emulated second,
        // with 4 PC samples to show which code owns the phase
        printf("sec: step page 4021 4023 mode | 40B2 40B3 406B | PC samples\n");
        for (int s = 0; s < 40; s++)
        {
            uint16_t pcs[4];
            for (int q = 0; q < 4; q++)
            {
                run_seconds(0.25);
                pcs[q] = mz80->GetPC();
            }
            printf("%3d:  %02X   %02X   %02X   %02X   %02X  |  %02X   %02X"
                "   %02X  | %04X %04X %04X %04X\n",
                s, MEM[0x4083], MEM[0x4084], MEM[0x4021], MEM[0x4023],
                MEM[0x4024], MEM[0x40B2], MEM[0x4077], MEM[0x406B],
                pcs[0], pcs[1], pcs[2], pcs[3]);
        }
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--droid-trace"))
    {
        // An ordinary wall-following droid's real path. No forcing at
        // all: coin, start, and watch droid 2 (and the min/max envelope
        // of all six) for 25 s of play. The question is what circuit the
        // wall-follow steering actually flies on hardware: pressed
        // against the boundary lines, or an inset track.
        run_seconds(10.0);
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff; run_seconds(0.5);
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        run_seconds(2.0);

        const uint16_t REC = 0x40C9;                 // droid 2
        uint8_t ymin = 0xFF, ymax = 0, xmin = 0xFF, xmax = 0;
        printf("t(s)  scrY scrX ang f0 f1 | all-droid envelope y[..] x[..]\n");
        for (int step = 0; step < 25 * 25; step++)   // 40 ms steps
        {
            run_seconds(0.04);
            const uint8_t* r = MEM + REC;
            for (int d = 0; d < 6; d++) {
                const uint8_t* q = MEM + 0x40C9 + d * 0x17;
                if (!(q[0] & 0x80)) continue;
                uint8_t qy = q[7], qx = q[9];
                if (qy < ymin) ymin = qy;  if (qy > ymax) ymax = qy;
                if (qx < xmin) xmin = qx;  if (qx > xmax) xmax = qx;
            }
            if ((step % 12) == 0) {
                printf("%5.1f ", step * 0.04);
                for (int d = 0; d < 6; d++) {
                    const uint8_t* q = MEM + 0x40C9 + d * 0x17;
                    if (q[0] & 0x80) printf(" %02X,%02X", q[7], q[9]);
                    else             printf(" --,--");
                }
                {
                    const uint8_t* q3 = MEM + 0x40C9 + 1 * 0x17;   // droid 3
                    printf("  d2[f0=%02X f1=%02X] d3[f0=%02X f1=%02X"
                           " vel=%02X%02X,%02X%02X b18=%02X tmr=%02X]\n",
                           r[0], r[1], q3[0], q3[1],
                           q3[3], q3[2], q3[5], q3[4], q3[18], q3[19]);
                }
            }
        }
        printf("hardware droid envelope over 25 s: y[%02X..%02X] x[%02X..%02X]\n",
               ymin, ymax, xmin, xmax);
        printf("(walls: outer y/x 04..FC ; screen centre 80)\n");
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--deathship-trace"))
    {
        // Hardware observation of a live death ship. The organic
        // scheduler takes ~45s of uninterrupted play to spawn one, so
        // force the exact scenario instead: coin up, start, let wave 1
        // settle, then plant a death ship mid-field in slot 22 (COMMAND
        // template fields, b13=2, shape 0xCBAF) and designate it
        // shooter_a. Everything after that is the game's own code:
        // OBJECTS_UPDATE stages it, enemy_fire_gate runs its shooter arm,
        // move_mode_dispatch does whatever it does. Trace the record
        // every 20 ms - where velocity comes from, and what happens at
        // the first wall contact, are exactly the open questions.
        run_seconds(10.0);
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff; run_seconds(0.5);
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        run_seconds(3.0);

        // A bare special (planted COMMAND record, designated but
        // unarmed) holds zero velocity - it cannot move. The fast
        // "death ship" is the OTHER thing that draws death-ship frames:
        // move_mode_dispatch's animation arm (0x1E03) picks the
        // death-ship half of table 0x3F4D for an object with flags0
        // bit 4 SET whose wall-follow bit is CLEAR - i.e. a DROID that
        // stopped wall-following. And the fire cycle does exactly that
        // to the designated shooter at 0x1CFA (res 7). A droid has
        // thrust (template flags0 0xF9, bit 0), which is where the
        // speed comes from.
        //
        // So: take the live wave's droid 2, designate it shooter_a with
        // the scheduler ARMED (0x407F nonzero, as the fire cycle leaves
        // it), force the pace timer to 0 so the fire cycle runs at once,
        // and watch the conversion and the first wall contact.
        const uint16_t REC = 0x40B2 + 1 * 0x17;      // object 2: 0x40C9
        MEM[0x407A] = 2;                             // shooter_a
        MEM[0x4077] = 2;                             // shooter_state
        MEM[0x407F] = 0xFA;                          // armed
        MEM[0x4022] = 0x00;                          // pace expired -> fire cycle

        printf("t(s)   f0 f1  velx vely  posx posy  ang b14 b18 tmr | sha 4022 407F\n");
        uint8_t pf0 = 0xFF, pf1 = 0xFF;
        uint16_t pvx = 0xFFFF, pvy = 0xFFFF;
        for (int step = 0; step < 40 * 50; step++)
        {
            run_seconds(0.02);
            const uint8_t* r = MEM + REC;
            uint16_t vx = (uint16_t)(r[2] | (r[3] << 8));
            uint16_t vy = (uint16_t)(r[4] | (r[5] << 8));
            // print on any flag/velocity change, plus a 1 Hz heartbeat
            if (r[0] != pf0 || r[1] != pf1 || vx != pvx || vy != pvy
                || (step % 50) == 0)
            {
                printf("%6.2f %02X %02X  %04X %04X  %02X%02X %02X%02X"
                       "  %02X  %02X  %02X  %02X | %02X  %02X  %02X\n",
                       step * 0.02, r[0], r[1], vx, vy,
                       r[7], r[6], r[9], r[8],
                       r[10], r[14], r[18], r[19],
                       MEM[0x407A], MEM[0x4022], MEM[0x407F]);
                pf0 = r[0]; pf1 = r[1]; pvx = vx; pvy = vy;
            }
            if (!(r[0] & 0x80)) { printf("-- record deactivated --\n"); break; }
            // keep it designated: the scan clears shooter_a when the
            // armed countdown runs out; re-arm so the chase persists
            if (MEM[0x407A] == 0) { MEM[0x407A] = 2; MEM[0x407F] = 0xFA; }
        }
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--firegate-vectors"))
    {
        // enemy_fire_gate (0x1BEB), the routine that decides which object
        // is the designated shooter and - the reason this matters - sets
        // flags0 bit 6 on it at 0x1C12 / 0x1C67. The COMMAND template
        // clears that bit, and OBJ_PHYSICS refuses to integrate position
        // without it, so a special ship cannot move until this routine
        // says so. It ends by falling into move_mode_dispatch at 0x1DB4.
        //
        // Two things are neutralised so the comparison is about the
        // routine and not its environment:
        //   RANDOM_NEXT (0x2906) is stubbed to RET, leaving A untouched;
        //   the C side does the same under OMEGA_WBTEST. The ROM's
        //   version reads the Z80 R register, which has no C equivalent.
        //   All of 0x4000-0x44FF is zeroed per case, so ENEMY_ALIVE_SCAN
        //   and SPAWN_SPECIAL walk the same all-inactive object table the
        //   port's zeroed omega_state gives them.
        const uint16_t TRAMP = 0x4E00;
        uint8_t tramp[] = {
            0xFD, 0x21, 0x80, 0x40,       // ld iy,$4080
            0x31, 0x00, 0x4F,             // ld sp,$4F00
            0xC3, 0xEB, 0x1B              // jp enemy_fire_gate
        };
        for (unsigned i = 0; i < sizeof tramp; i++) MEM[TRAMP + i] = tramp[i];

        const uint16_t DONE = 0x1DB4;                 // move_mode_dispatch
        uint8_t save_mmd[2] = { MEM[DONE], MEM[DONE + 1] };
        MEM[DONE] = 0x18; MEM[DONE + 1] = 0xFE;       // jr $
        uint8_t save_rnd = MEM[0x2906];
        MEM[0x2906] = 0xC9;                           // RANDOM_NEXT -> ret

        int count = (argc > 2) ? atoi(argv[2]) : 400;
        uint32_t lcg = 0x0BADC0DE;

        printf("# enemy_fire_gate differential vectors, %d cases\n", count);
        printf("# in:  flags0 flags1 angle b14 objidx shooter_a shooter_b "
               "state pace diff armed xlife wave_num\n");
        printf("# out: flags0 flags1 angle b14 b18 shooter_a shooter_b "
               "pace diff armed xlife aim\n");

        for (int n = 0; n < count; n++)
        {
            for (int i = 0x4000; i < 0x4500; i++) MEM[i] = 0;

            lcg = lcg * 1664525u + 1013904223u; uint8_t flags0 = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u; uint8_t flags1 = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u; uint8_t angle  = (uint8_t)((lcg >> 24) & 0x3F);
            lcg = lcg * 1664525u + 1013904223u; uint8_t b14    = (uint8_t)((lcg >> 24) & 0x1F);
            lcg = lcg * 1664525u + 1013904223u; uint8_t objidx = (uint8_t)(((lcg >> 24) % 32) + 1);
            lcg = lcg * 1664525u + 1013904223u; uint8_t sha    = (uint8_t)(((lcg >> 24) % 34));
            lcg = lcg * 1664525u + 1013904223u; uint8_t shb    = (uint8_t)(((lcg >> 24) % 34));
            lcg = lcg * 1664525u + 1013904223u; uint8_t state  = (uint8_t)((lcg >> 24) % 3);
            lcg = lcg * 1664525u + 1013904223u; uint8_t pace   = (uint8_t)((lcg >> 24) & 0x1F);
            lcg = lcg * 1664525u + 1013904223u; uint8_t diff   = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u; uint8_t armed  = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u; uint8_t xlife  = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u; uint8_t wnum   = (uint8_t)(((lcg >> 24) % 7) + 6);

            // one in three cases makes this object the designated shooter,
            // which is the branch that hands out flags0 bit 6
            if ((n % 3) == 0) sha = objidx;
            if ((n % 7) == 0) shb = objidx;

            MEM[0x4000] = flags0;
            MEM[0x4001] = flags1;
            MEM[0x400A] = angle;
            MEM[0x400E] = b14;
            MEM[0x4067] = wnum;      // wave_num
            MEM[0x4022] = pace;      // wave_pace_timer
            MEM[0x406A] = xlife;
            MEM[0x407A] = sha;
            MEM[0x407B] = shb;
            MEM[0x407C] = diff;
            MEM[0x407D] = objidx;
            MEM[0x407F] = armed;
            MEM[0x4077] = state;     // shooter_state
            MEM[0x40B2 + 1] = 0;     // ship flags1: the 0x1C33 bit-4 gate

            mz80->SetPC(TRAMP);
            for (int guard = 0; guard < 20000 && mz80->GetPC() != DONE; guard++)
                mz80->mz80exec(32);

            printf("F %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X"
                   " | %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                   flags0, flags1, angle, b14, objidx, sha, shb, state,
                   pace, diff, armed, xlife, wnum,
                   MEM[0x4000], MEM[0x4001], MEM[0x400A], MEM[0x400E],
                   MEM[0x4012], MEM[0x407A], MEM[0x407B], MEM[0x4022],
                   MEM[0x407C], MEM[0x407F], MEM[0x406A], MEM[0x400F]);
        }
        MEM[DONE] = save_mmd[0]; MEM[DONE + 1] = save_mmd[1];
        MEM[0x2906] = save_rnd;
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--objphysics-vectors"))
    {
        // Same differential idea as --wallbounce-vectors, one level up:
        // OBJ_PHYSICS (0x1B16) is thrust, the speed clamp, friction,
        // WALL_BOUNCE and the position integration. It does not RET - it
        // falls through into enemy_fire_gate at 0x1BEB - so the run stops
        // the moment PC reaches that address, which is exactly the
        // boundary the port's obj_physics() covers.
        const uint16_t TRAMP = 0x4E00;
        uint8_t tramp[] = {
            0xFD, 0x21, 0x80, 0x40,       // ld iy,$4080
            0x31, 0x00, 0x4F,             // ld sp,$4F00
            0xC3, 0x16, 0x1B              // jp OBJ_PHYSICS
        };
        for (unsigned i = 0; i < sizeof tramp; i++) MEM[TRAMP + i] = tramp[i];
        // Trap the fall-through exactly. mz80exec runs several
        // instructions per call, so polling for PC == 0x1BEB overshoots
        // it and runs on into enemy_fire_gate, which wanders through
        // ENEMY_ALIVE_SCAN/SPAWN_SPECIAL with no valid ix and scribbles
        // the workspace - which reads back as an all-zero "result". A
        // spin loop written over the first two bytes of enemy_fire_gate
        // parks the CPU there instead; the original bytes go back
        // afterwards.
        const uint16_t DONE = 0x1BEB;     // enemy_fire_gate
        uint8_t save_efg[2] = { MEM[DONE], MEM[DONE + 1] };
        MEM[DONE] = 0x18; MEM[DONE + 1] = 0xFE;   // jr $

        int count = (argc > 2) ? atoi(argv[2]) : 400;
        uint32_t lcg = 0x2468ACE1;

        printf("# OBJ_PHYSICS differential vectors, %d cases\n", count);
        printf("# in:  flags0 flags1 velx vely posx posy angle b11 wave_flags\n");
        printf("# out: flags0 flags1 velx vely posx posy angle 4078\n");

        for (int n = 0; n < count; n++)
        {
            lcg = lcg * 1664525u + 1013904223u; uint16_t posy = (uint16_t)(lcg >> 16);
            lcg = lcg * 1664525u + 1013904223u; uint16_t posx = (uint16_t)(lcg >> 16);
            lcg = lcg * 1664525u + 1013904223u; int16_t  vely = (int16_t)((int)((lcg >> 20) & 0x7FF) - 0x400);
            lcg = lcg * 1664525u + 1013904223u; int16_t  velx = (int16_t)((int)((lcg >> 20) & 0x7FF) - 0x400);
            lcg = lcg * 1664525u + 1013904223u; uint8_t  flags0 = (uint8_t)(lcg >> 24);
            // flags1 bit 0 forced clear: with it set, a bounce sends
            // OBJ_PHYSICS through OBJ_DESTROY (0x1BD0), which stamps the
            // inert template over the workspace and walks outside it via
            // ix - not meaningful to drive synthetically. Excluding that
            // one arm keeps the comparison symmetric, since the port
            // takes the same branch on the same condition.
            lcg = lcg * 1664525u + 1013904223u; uint8_t  flags1 = (uint8_t)((lcg >> 24) & 0xFE);
            lcg = lcg * 1664525u + 1013904223u; uint8_t  angle  = (uint8_t)((lcg >> 24) & 0x3F);
            lcg = lcg * 1664525u + 1013904223u; uint8_t  b11    = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u; uint8_t  wflags = (uint8_t)(lcg >> 24);

            MEM[0x4000] = flags0;
            MEM[0x4001] = flags1;
            MEM[0x4002] = (uint8_t)(velx & 0xFF); MEM[0x4003] = (uint8_t)(velx >> 8);
            MEM[0x4004] = (uint8_t)(vely & 0xFF); MEM[0x4005] = (uint8_t)(vely >> 8);
            MEM[0x4006] = (uint8_t)(posx & 0xFF); MEM[0x4007] = (uint8_t)(posx >> 8);
            MEM[0x4008] = (uint8_t)(posy & 0xFF); MEM[0x4009] = (uint8_t)(posy >> 8);
            MEM[0x400A] = angle;
            MEM[0x400B] = b11;
            for (int i = 0x400C; i <= 0x4016; i++) MEM[i] = 0;
            MEM[0x406B] = wflags;
            MEM[0x4078] = 0;
            MEM[0x409A] = 0;
            MEM[0x407D] = 1;              // obj_index: OBJ_DESTROY reads it
            MEM[0x407E] = 1;              // obj_class

            mz80->SetPC(TRAMP);
            for (int guard = 0; guard < 6000 && mz80->GetPC() != DONE; guard++)
                mz80->mz80exec(32);

            printf("P %02X %02X %04X %04X %04X %04X %02X %02X %02X"
                   " | %02X %02X %04X %04X %04X %04X %02X %02X\n",
                   flags0, flags1, (uint16_t)velx, (uint16_t)vely,
                   posx, posy, angle, b11, wflags,
                   MEM[0x4000], MEM[0x4001],
                   (uint16_t)(MEM[0x4002] | (MEM[0x4003] << 8)),
                   (uint16_t)(MEM[0x4004] | (MEM[0x4005] << 8)),
                   (uint16_t)(MEM[0x4006] | (MEM[0x4007] << 8)),
                   (uint16_t)(MEM[0x4008] | (MEM[0x4009] << 8)),
                   MEM[0x400A], MEM[0x4078]);
        }
        MEM[DONE] = save_efg[0]; MEM[DONE + 1] = save_efg[1];
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--wallbounce-vectors"))
    {
        // Differential test vectors for WALL_BOUNCE (ROM 0x1F2A).
        //
        // Rather than compare trajectories - which drift apart the moment
        // any input differs - this drives the ROM's own routine directly
        // on synthetic object states and prints (input, output) pairs. A
        // C harness runs the port's wall_bounce() on the identical inputs
        // and diffs, so a mismatch names the exact case.
        //
        // The routine is reached the way the game reaches it: the staged
        // object lives in the workspace at 0x4000-0x4016 and IY points at
        // 0x4080. A four-instruction trampoline in RAM sets that up,
        // calls, and halts.
        const uint16_t TRAMP = 0x4E00;
        uint8_t tramp[] = {
            0xFD, 0x21, 0x80, 0x40,       // ld iy,$4080
            0x31, 0x00, 0x4F,             // ld sp,$4F00
            0xCD, 0x2A, 0x1F,             // call WALL_BOUNCE
            0x18, 0xFE                    // jr $ - spin, NOT halt: a
        };                                // halted Z80 stays halted
                                          // without an interrupt, so
                                          // every case after the first
                                          // would silently do nothing
        for (unsigned i = 0; i < sizeof tramp; i++) MEM[TRAMP + i] = tramp[i];
        const uint16_t DONE = TRAMP + 10;   // the halt

        int count = (argc > 2) ? atoi(argv[2]) : 400;
        uint32_t lcg = 0x13579BDF;         // fixed seed: reproducible

        printf("# WALL_BOUNCE differential vectors, %d cases\n", count);
        printf("# in:  flags0 flags1 velx vely posx posy angle b11 wave_flags\n");
        printf("# out: ret flags0 flags1 velx vely posx posy b18 4078\n");

        for (int n = 0; n < count; n++)
        {
            // spread the cases over the interesting geometry: screen
            // edges, the inner island, and the corners, at a range of
            // speeds and both wall-follow states
            lcg = lcg * 1664525u + 1013904223u;
            uint16_t posy = (uint16_t)(lcg >> 16);
            lcg = lcg * 1664525u + 1013904223u;
            uint16_t posx = (uint16_t)(lcg >> 16);
            lcg = lcg * 1664525u + 1013904223u;
            int16_t  vely = (int16_t)((int)((lcg >> 20) & 0x7FF) - 0x400);
            lcg = lcg * 1664525u + 1013904223u;
            int16_t  velx = (int16_t)((int)((lcg >> 20) & 0x7FF) - 0x400);
            lcg = lcg * 1664525u + 1013904223u;
            uint8_t  flags0 = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u;
            uint8_t  flags1 = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u;
            uint8_t  angle  = (uint8_t)((lcg >> 24) & 0x3F);
            lcg = lcg * 1664525u + 1013904223u;
            uint8_t  b11    = (uint8_t)(lcg >> 24);
            lcg = lcg * 1664525u + 1013904223u;
            uint8_t  wflags = (uint8_t)(lcg >> 24);

            // stage the object exactly as OBJ_STAGE_RUN does
            MEM[0x4000] = flags0;
            MEM[0x4001] = flags1;
            MEM[0x4002] = (uint8_t)(velx & 0xFF); MEM[0x4003] = (uint8_t)(velx >> 8);
            MEM[0x4004] = (uint8_t)(vely & 0xFF); MEM[0x4005] = (uint8_t)(vely >> 8);
            MEM[0x4006] = (uint8_t)(posx & 0xFF); MEM[0x4007] = (uint8_t)(posx >> 8);
            MEM[0x4008] = (uint8_t)(posy & 0xFF); MEM[0x4009] = (uint8_t)(posy >> 8);
            MEM[0x400A] = angle;
            MEM[0x400B] = b11;
            for (int i = 0x400C; i <= 0x4016; i++) MEM[i] = 0;
            MEM[0x406B] = wflags;       // wave_flags
            MEM[0x4078] = 0;            // bounce flags
            MEM[0x409A] = 0;            // wall_fx meta

            mz80->SetPC(TRAMP);
            for (int guard = 0; guard < 4000 && mz80->GetPC() != DONE; guard++)
                mz80->mz80exec(32);

            printf("V %02X %02X %04X %04X %04X %04X %02X %02X %02X"
                   " | %02X %02X %04X %04X %04X %04X %02X %02X\n",
                   flags0, flags1, (uint16_t)velx, (uint16_t)vely,
                   posx, posy, angle, b11, wflags,
                   MEM[0x4000], MEM[0x4001],
                   (uint16_t)(MEM[0x4002] | (MEM[0x4003] << 8)),
                   (uint16_t)(MEM[0x4004] | (MEM[0x4005] << 8)),
                   (uint16_t)(MEM[0x4006] | (MEM[0x4007] << 8)),
                   (uint16_t)(MEM[0x4008] | (MEM[0x4009] << 8)),
                   MEM[0x4012], MEM[0x4078]);
        }
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--wall-trace"))
    {
        // Ground truth for the wall display list. Dumps 0x81B0-0x8243 as
        // raw bytes twice: once as GAME_DLIST_BUILD leaves it, and again
        // the moment WALL_FLASH_DECAY has touched a struck segment. Also
        // prints wall_fx (0x409A..0x40AD) so the segment index that was
        // hit is known, and the 19 stride-6 byte positions the ROM
        // writes, so their alignment against real opcode words is
        // visible.
        run_seconds(10.0);
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff; run_seconds(0.5);
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        run_seconds(1.0);

        printf("=== wall list as built (0x81B0-0x8243) ===\n");
        for (int a = 0x81B0; a < 0x8244; a += 16)
        {
            printf("%04X:", a);
            for (int i = 0; i < 16 && a + i < 0x8244; i++) printf(" %02X", MEM[a + i]);
            printf("\n");
        }
        printf("stride-6 slots the ROM writes (0x81BB + n*6):\n");
        for (int n = 0; n < 19; n++)
        {
            int addr = 0x81BB + n * 6;
            printf("  slot %2d @%04X = %02X\n", n + 1, addr, MEM[addr]);
        }

        // wait for a wall hit: any of wall_fx[1..19] going nonzero
        int hit_seg = -1;
        for (int step = 0; step < 120 * 50 && hit_seg < 0; step++)
        {
            run_seconds(0.02);
            for (int i = 1; i <= 19; i++)
                if (MEM[0x409A + i]) { hit_seg = i; break; }
        }
        printf("\n=== after wall hit: segment %d, t~%s ===\n",
               hit_seg, hit_seg < 0 ? "NONE SEEN" : "hit");
        printf("wall_fx 0x409A..0x40AD:");
        for (int i = 0; i <= 19; i++) printf(" %02X", MEM[0x409A + i]);
        printf("\n");

        // let the decay run a few passes so the writes land, sampling as
        // we go - the flash alternates, so print several snapshots
        for (int pass = 0; pass < 6; pass++)
        {
            run_seconds(0.05);
            printf("pass %d  fx:", pass);
            for (int i = 1; i <= 19; i++) printf(" %02X", MEM[0x409A + i]);
            printf("\n        nibbles:");
            for (int n = 0; n < 19; n++)
                printf(" %X", MEM[0x81BB + n * 6] >> 4);
            printf("\n");
        }

        printf("\n=== wall list after hits (0x81B0-0x8243) ===\n");
        for (int a = 0x81B0; a < 0x8244; a += 16)
        {
            printf("%04X:", a);
            for (int i = 0; i < 16 && a + i < 0x8244; i++) printf(" %02X", MEM[a + i]);
            printf("\n");
        }
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--pace-trace"))
    {
        // Wave-pacing ground truth for the C port: log every change of the
        // scheduler cells through 90 s of attract (including the self-play
        // demo, where the demo ship kills designated shooters - the
        // cs_kill_object 0x2307-0x2322 re-pace fires there) and then 60 s
        // of idle play.  4022=pace 407C=diff 407A/B=shooters 4077=state
        // 406A=flags (bit0 = late-game 2/2 re-pace select).
        uint8_t pv[6] = { 0 };
        auto snap = [&](double t) {
            uint8_t cv[6] = { MEM[0x4022], MEM[0x407C], MEM[0x407A],
                              MEM[0x407B], MEM[0x4077], MEM[0x406A] };
            if (!memcmp(pv, cv, sizeof cv)) return;
            printf("%7.2f mode=%02X wave=%02X | 4022=%02X 407C=%02X"
                   " shootA=%02X shootB=%02X 4077=%02X 406A=%02X\n",
                   t, MEM[0x4024], MEM[0x4067],
                   cv[0], cv[1], cv[2], cv[3], cv[4], cv[5]);
            memcpy(pv, cv, sizeof cv);
        };
        for (int step = 0; step < 90 * 50; step++)
        { run_seconds(0.02); snap(step * 0.02); }
        printf("-- coin + start --\n");
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff; run_seconds(0.5);
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        for (int step = 0; step < 60 * 50; step++)
        { run_seconds(0.02); snap(90.0 + step * 0.02); }
        return 0;
    }

    if (argc > 1 && !strcmp(argv[1], "--special-trace"))
    {
        // Ground truth for the C port's SPAWN_SPECIAL scheduling: coin up,
        // start, and watch the special pool records (objects 22-32 at
        // 0x40B2 + (n-1)*0x17) directly.  Logs every activation /
        // deactivation with the scheduler cells, and dumps each live
        // special once a second so its motion (or lack of it) is visible.
        run_seconds(10.0);
        in_coin = 0xfe;  run_seconds(0.2);  in_coin = 0xff; run_seconds(0.5);
        in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
        printf("play starts at t=0; 4022=pace 407C=diff 407F=arm 4077=state\n");
        uint8_t prev_act[33] = { 0 };
        for (int step = 0; step < 420 * 50; step++)     // 20 ms steps
        {
            run_seconds(0.02);
            double t = step * 0.02;
            for (int n = 22; n <= 32; n++)
            {
                const uint8_t* r = MEM + 0x40B2 + (n - 1) * 0x17;
                uint8_t act = r[0] & 0x80;
                if (act == prev_act[n]) continue;
                int droids = 0;
                for (int d = 2; d <= 13; d++)
                    if (MEM[0x40B2 + (d - 1) * 0x17] & 0x80) droids++;
                printf("%7.2f obj %2d %s f0=%02X f1=%02X b13=%02X shape=%02X%02X"
                    " | mode=%02X wave=%02X droids=%2d 4022=%02X 407C=%02X"
                    " 407F=%02X 4077=%02X\n",
                    t, n, act ? "SPAWN" : "gone ",
                    r[0], r[1], r[13], r[22], r[21],
                    MEM[0x4024], MEM[0x4067], droids,
                    MEM[0x4022], MEM[0x407C], MEM[0x407F], MEM[0x4077]);
                prev_act[n] = act;
            }
            if (step % 50 == 0)
            {
                for (int n = 22; n <= 32; n++)
                {
                    const uint8_t* r = MEM + 0x40B2 + (n - 1) * 0x17;
                    if (!(r[0] & 0x80)) continue;
                    printf("  t=%5.1f obj %2d pos=(%02X,%02X)"
                        " vel=(%02X%02X,%02X%02X) ang=%02X f0=%02X f1=%02X"
                        " b13=%02X b18=%02X shape=%02X%02X\n",
                        t, n, r[7], r[9], r[3], r[2], r[5], r[4],
                        r[10], r[0], r[1], r[13], r[18], r[22], r[21]);
                }
                // once a second also give the wave context
                if (step % 250 == 0)
                    printf("  t=%5.1f mode=%02X wave=%02X 4022=%02X 407C=%02X"
                        " 407F=%02X 4077=%02X lives=%02X\n",
                        t, MEM[0x4024], MEM[0x4067], MEM[0x4022],
                        MEM[0x407C], MEM[0x407F], MEM[0x4077], MEM[0x4066]);
            }
        }
        return 0;
    }

    // --- phase 1: attract, 90 emulated seconds (covers all attract pages
    //     and the self-play demo at attract step 7)
    int f0 = nframes;
    run_seconds(90.0);
    report("[attract 90s]", f0, nframes);

    // --- phase 2: insert coin, press start, play 60 seconds
    f0 = nframes;
    in_coin = 0xfe;  run_seconds(0.1);  in_coin = 0xff;  run_seconds(0.5);
    in_start = 0xbf; run_seconds(0.2);  in_start = 0xff;
    run_seconds(60.0);
    printf("\n");
    report("[coin+play  ]", f0, nframes);

    // --- vector-ROM shape usage census (word addrs >= 0x800)
    printf("\nshapes entered during run (byte addr: count):\n");
    for (int wa = 0x800; wa < 0x1000; wa++)
        if (shape_used[wa])
            printf("  %04X:%u", 0x8000 + wa * 2, shape_used[wa]);
    printf("\n");

    // --- global histogram of frame periods (1ms buckets)
    printf("\nframe-period histogram (all %d frames):\n", nframes);
    int hist[40] = { 0 };
    for (int i = 1; i < nframes; i++)
    {
        int64_t p = (int64_t)(frames[i].kick_cycle - frames[i - 1].kick_cycle);
        int ms = (int)(p * 1000 / CPU_CLOCK);
        if (ms >= 0 && ms < 40) hist[ms]++;
    }
    for (int i = 0; i < 40; i++)
        if (hist[i])
            printf("  %2d-%2dms: %6d  (%5.1f-%5.1f fps)\n",
                i, i + 1, hist[i],
                i + 1 > 0 ? 1000.0 / (i + 1) : 999.0, i > 0 ? 1000.0 / i : 999.0);

    return 0;
}
