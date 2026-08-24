/* probe_nvram.c - regression test for credit persistence across a restart.
 *
 * Build (x64, run from c_src\):
 *   cl /nologo /W4 /std:c11 /wd4102 /D_CRT_SECURE_NO_WARNINGS
 *      tests/probe_nvram.c mainline.c frame.c objects.c enemies.c score.c
 *      irq_coins.c pages.c dvg_pages.c omega_pagerom.c post.c sound_samples.c omega_shapes.c glue.c
 *      dvg.c omega_vecrom.c omega_dvgprom.c
 *      platform/headless/plat_headless.c /Fe:tests/probe_nvram.exe
 *
 * 0x5C56/0x5C57 are battery-backed RAM holding the credit count as two
 * nibbles: CREDITS_NVRAM_SYNC (0x16E3) writes credits_bcd plain to 0x5C56
 * and nibble-swapped to 0x5C57, and GAME_INIT reads them back through
 * NVRAM_RD_BYTE (0x0A0A-0x0A13), which rejects either low nibble >= 0x0A
 * and `rrd`s the pair into credits_bcd as (hi << 4) | lo.
 *
 * Asserted here: credits banked or spent survive a power cycle, the
 * shadow holds the ROM's nibble pair, no-op syncs cause no writes, and
 * a corrupt shadow wipes the way the ROM does.
 *
 * The host hooks here keep the "NVRAM chip" in a static buffer, which is
 * exactly the round-trip the file-backed host performs.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../omega_state.h"
#include "../omega_shapes.h"
#include "../platform/headless/plat_headless.h"

extern void game_init(void);
extern void play_begin(uint8_t is_demo);   /* PLAY_BEGIN 0x0BD1 (frame.c) */
extern int  credits_nvram_sync(void);

/* ---- the "NVRAM chip": persists across our simulated power cycles ---- */
static struct {
    uint8_t hiscore_bank[2][6][7];
    uint8_t nvram_template[32];
    uint8_t bookkeep[0x40];
    uint8_t nvram_credits[2];
    int     valid;
    int     writes;
} chip;

static void chip_save(void)
{
    memcpy(chip.hiscore_bank,  g.hiscore_bank,  sizeof chip.hiscore_bank);
    memcpy(chip.nvram_template,g.nvram_template,sizeof chip.nvram_template);
    memcpy(chip.bookkeep,      g.bookkeep,      sizeof chip.bookkeep);
    memcpy(chip.nvram_credits, g.nvram_credits, sizeof chip.nvram_credits);
    chip.valid = 1;
    chip.writes++;
}
static void chip_load(void)
{
    if (!chip.valid) return;             /* first ever boot: nothing stored */
    memcpy(g.hiscore_bank,  chip.hiscore_bank,  sizeof chip.hiscore_bank);
    memcpy(g.nvram_template,chip.nvram_template,sizeof chip.nvram_template);
    memcpy(g.bookkeep,      chip.bookkeep,      sizeof chip.bookkeep);
    memcpy(g.nvram_credits, chip.nvram_credits, sizeof chip.nvram_credits);
}

/* remaining host stubs: platform/headless/plat_headless.c
 * (default DSW C6 0xbf: bit6 clear = not free play) */

static int fails;
static void chk(const char *what, int got, int want)
{
    printf("  %-34s got %3d  want %3d  %s\n", what, got, want,
           got == want ? "ok" : "FAIL");
    if (got != want) fails++;
}

/* power cycle: wipe all volatile state, boot again off the same chip */
static void power_cycle(void)
{
    memset(&g, 0, sizeof g);
    game_init();
}

/* bank `n` credits the way the coin path does, then let the sync run */
static void bank_credits(uint8_t bcd)
{
    g.credits_bcd = bcd;
    if (credits_nvram_sync()) omega_hw_nvram_save();
}

int main(void)
{
    static const uint8_t CASES[] = { 0x01, 0x02, 0x05, 0x09, 0x10, 0x17, 0x20 };
    size_t i;

    hl_nvram_save_hook = chip_save;
    hl_nvram_load_hook = chip_load;

    puts("=== credits survive a power cycle (ROM 0x16E3 / 0x0A0A) ===");

    /* first boot with a blank chip: no credits, and the wipe path leaves a
     * chip that validates next time */
    power_cycle();
    chk("cold boot on blank NVRAM", g.credits_bcd, 0);

    for (i = 0; i < sizeof CASES; i++) {
        uint8_t want = CASES[i];
        char label[64];

        bank_credits(want);
        power_cycle();
        sprintf(label, "banked 0x%02X, after restart", want);
        chk(label, g.credits_bcd, want);
    }

    puts("");
    puts("=== the shadow is the ROM's nibble pair ===");
    bank_credits(0x17);
    chk("0x5C56 (plain)",         g.nvram_credits[0], 0x17);
    chk("0x5C57 (nibble-swapped)", g.nvram_credits[1], 0x71);

    puts("");
    puts("=== spending credits persists too ===");
    bank_credits(0x03);
    bank_credits(0x01);            /* two games played */
    power_cycle();
    chk("after spending down to 1", g.credits_bcd, 0x01);
    bank_credits(0x00);
    power_cycle();
    chk("after spending the last",  g.credits_bcd, 0x00);

    puts("");
    puts("=== the reported case: boot WITH a banked credit, start, restart ===");
    /* A credit banked in a PREVIOUS session sets no dirty flag at boot,
     * so when PLAY_BEGIN deducts it, a raw host flush without re-staging
     * would write the boot-loaded credit shadow back out and the credit
     * would resurrect on restart. PLAY_BEGIN must save via
     * NVRAM_SAVE_ALL (0x0A68), whose 0x0A86 CREDITS_NVRAM_SYNC re-stages. */
    bank_credits(0x01);
    power_cycle();                       /* fresh boot: credit from "chip" */
    chk("boot with 1 banked credit", g.credits_bcd, 0x01);
    g.start_credits = 1;                 /* 1P start (START_BTN_CHECK 0x1230) */
    play_begin(0);                       /* deducts + NVRAM_SAVE_ALL */
    chk("deducted in game", g.credits_bcd, 0x00);
    power_cycle();
    chk("still spent after restart", g.credits_bcd, 0x00);

    puts("");
    puts("=== no file churn when nothing changed ===");
    {
        int before;
        bank_credits(0x04);
        before = chip.writes;
        credits_nvram_sync();      /* MAINLOOP reaches this every frame */
        credits_nvram_sync();
        credits_nvram_sync();
        chk("writes from 3 no-op syncs", chip.writes - before, 0);
    }

    puts("");
    puts("=== corrupt shadow wipes, like the ROM ===");
    bank_credits(0x05);
    chip.nvram_credits[0] = 0x0F;  /* low nibble 0x0F >= 0x0A -> invalid BCD */
    power_cycle();
    chk("bad nibble -> credits reset", g.credits_bcd, 0);

    printf("\n%s\n", fails ? "PROBE FAILED" : "PROBE PASSED");
    return fails ? 1 : 0;
}
