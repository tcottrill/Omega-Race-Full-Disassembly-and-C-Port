/* probe_hiscore.c - regression test for the mode-4 high-score entry screen.
 *
 * Build (x64, run from c_src\):
 *   cl /nologo /W4 /std:c11 /wd4102 /D_CRT_SECURE_NO_WARNINGS
 *      tests/probe_hiscore.c mainline.c frame.c objects.c enemies.c score.c
 *      irq_coins.c pages.c dvg_pages.c omega_pagerom.c post.c sound_samples.c omega_shapes.c glue.c
 *      dvg.c omega_vecrom.c omega_dvgprom.c
 *      platform/headless/plat_headless.c /Fe:tests/probe_hiscore.exe
 *
 * Ground truth is the 0x7A-byte display list at ROM 0x3A0E that
 * SHIP_RESPAWN copies into the shadow list (0x0EF7-0x0F00), decoded in the
 * geometry comment at the top of score.c - the whole selectable alphabet
 * lives in that blob.
 *
 * Asserted here:
 *  - entering mode 4 lays out A-Z on a 32-unit pitch from x=32, y=512
 *  - ERASE and END are vertical stacks in their own selection columns
 *  - the cursor tracks letter_sel at x = 32 + 32*sel, y = 500
 *  - the cursor's column coincides with ERASE at sel 0x1B and END at 0x1D,
 *    the two indices HISCORE_INITIALS acts on (ROM 0x2A71-0x2A7B)
 *  - fire commits letters, backspaces on ERASE, finalizes on END
 *  - committed initials draw at the LABS block's own x=416 y=256 scale 2
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../omega_state.h"
#include "../omega_shapes.h"

extern void game_init(void);
extern void ship_respawn(void);
extern void hiscore_initials(void);
extern void hiscore_check(void);

/* ---- host stubs: platform/headless/plat_headless.c ------------------
 * Nothing is captured here: the whole letter wheel lives in the display
 * list, so this probe asserts the shadow blob in vector RAM directly
 * (see the checks below) rather than watching host draw calls. */

static int fails;

static void chk(const char *what, int got, int want)
{
    printf("  %-30s got %5d  want %5d  %s\n", what, got, want,
           got == want ? "ok" : "FAIL");
    if (got != want) fails++;
}
static void chk_near(const char *what, float got, float want, float tol)
{
    int ok = (got - want < tol) && (want - got < tol);
    printf("  %-30s got %7.2f  want %7.2f  %s\n", what, got, want,
           ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

static void enter_mode4(void)
{
    memset(&g, 0, sizeof g);
    game_init();
    g.game_flags |= 0x80;                 /* game in progress */
    g.score_cur[2] = 0x99; g.score_cur[3] = 0x99;   /* beats any table entry */
    hiscore_check();
    ship_respawn();
}

/* ---- 1. the wheel is on screen -------------------------------------- */

extern uint8_t omega_dlist_shadow[0x1B0];

static uint16_t shadow_wd(int off)
{
    return (uint16_t)(omega_dlist_shadow[off]
                      | (omega_dlist_shadow[off + 1] << 8));
}

static void wheel_tests(void)
{
    /* The wheel is the ROM's 0x3A0E display list, installed verbatim in
     * the shadow by ship_respawn (dvg_letterwheel_install). Assert the
     * byte-verified blob structure rather than host draw calls. Letter
     * JSRLs per the 0x3A2E table: 'A'=CC4D..'Z'=CD27. */
    static const uint16_t LETTERS[26] = {
        0xCC4D,0xCC58,0xCC64,0xCC6C,0xCC75,0xCC77,0xCC7F,0xCC88,
        0xCC91,0xCC9B,0xCCA3,0xCCAB,0xCCB2,0xCCBA,0xCCC2,0xCCCA,
        0xCCD2,0xCCDC,0xCCE6,0xCCF0,0xCCF8,0xCD02,0xCD0B,0xCD16,
        0xCD1D,0xCD27
    };
    static const uint16_t ERASE[9] = {   /* E R A S E, CDBF spacers */
        0xCC75,0xCDBF,0xCCDC,0xCDBF,0xCC4D,0xCDBF,0xCCE6,0xCDBF,0xCC75
    };
    static const uint16_t END[5] = { 0xCC75,0xCDBF,0xCCBA,0xCDBF,0xCC6C };
    int i, ok;

    puts("=== mode-4 wheel layout (ROM 0x3A0E blob in the shadow) ===");
    enter_mode4();
    chk("game_mode", g.game_mode, 4);

    chk("wheel LABS y word (+18)", shadow_wd(0x18), 0xA200);
    chk("wheel LABS x word (+1A)", shadow_wd(0x1A), 0x1020);
    ok = 1;
    for (i = 0; i < 26; i++)
        if (shadow_wd(0x20 + 2 * i) != LETTERS[i]) ok = 0;
    chk("A-Z JSRLs at +20", ok, 1);
    chk("space after Z", shadow_wd(0x54), 0xCD92);
    ok = 1;
    for (i = 0; i < 9; i++)
        if (shadow_wd(0x56 + 2 * i) != ERASE[i]) ok = 0;
    chk("ERASE stack at +56", ok, 1);
    chk("END LABS y (+68)", shadow_wd(0x68), 0xA200);
    chk("END LABS x (+6A)", shadow_wd(0x6A), 0x13C0);
    ok = 1;
    for (i = 0; i < 5; i++)
        if (shadow_wd(0x70 + 2 * i) != END[i]) ok = 0;
    chk("END stack at +70", ok, 1);
    chk("cursor SVEC (+16)", shadow_wd(0x16), 0xF0FA);
    printf("\n");
}

/* ---- 2. the cursor tracks the spinner ------------------------------- */

static void cursor_tests(void)
{
    /* ROM 0x2A42-0x2A51: the cursor is the LABS X word at shadow +0x10
     * (vram 0x8010), set to (letter_sel+1)*0x20; the SVEC dash after it
     * is static. sel 0x1B -> 0x380 (=896, the ERASE column) and 0x1D ->
     * 0x3C0 (=960, END). */
    int i, ok_x = 1;
    uint16_t erase_w = 0, end_w = 0;

    puts("=== cursor tracks letter_sel (ROM 0x2A42-0x2A51) ===");
    enter_mode4();

    for (i = 0; i < 32; i++) {
        g.obj[OBJ_SHIP].angle = (uint8_t)(i * 2);   /* (angle & 0x3E) >> 1 == i */
        g.seconds_ctr = 0;
        hiscore_initials();
        if (g.letter_sel != (uint8_t)i) ok_x = 0;
        if (shadow_wd(0x10) != (uint16_t)((i + 1) * 0x20)) ok_x = 0;
        if (i == 0x1B) erase_w = shadow_wd(0x10);
        if (i == 0x1D) end_w = shadow_wd(0x10);
    }
    chk("cursor LABS X == (sel+1)*0x20", ok_x, 1);
    chk("sel 0x1B is the ERASE column (x=896)", erase_w, 0x0380);
    chk("sel 0x1D is the END column (x=960)",   end_w,   0x03C0);
    printf("\n");
}

/* ---- 3. typing --------------------------------------------------- */

static void press_fire(int sel)
{
    g.obj[OBJ_SHIP].angle = (uint8_t)(sel * 2);
    g.obj[OBJ_SHIP].flags0 |= 0x04;    /* ROM 0x2A63: bit 2 of (iy+50) */
    g.seconds_ctr = 0;
    hiscore_initials();
}

static void typing_tests(void)
{
    puts("=== typing (ROM 0x2AB8 commit / 0x2A83 erase / 0x2A77 end) ===");
    enter_mode4();

    press_fire(0);           /* 'A' */
    press_fire(1);           /* 'B' */
    chk("initials_cnt after 2", g.initials_cnt, 2);
    chk("initials_ptr[0]", g.initials_ptr[0], 'A');
    chk("initials_ptr[1]", g.initials_ptr[1], 'B');

    /* The committed field is real display-list words in the shadow
     * (dvg_initials_redraw): glyph JSRLs at shadow +0x08/+0x0A over the
     * blob's space words, positioned by the blob's own LABS at +0 -
     * y=0x100 (256), X word 0x21A0 = scale 2, x=0x1A0 (416). */
    {
        extern uint8_t omega_dlist_shadow[0x1B0];
        uint16_t labs_y = (uint16_t)(omega_dlist_shadow[0]
                                     | (omega_dlist_shadow[1] << 8));
        uint16_t labs_x = (uint16_t)(omega_dlist_shadow[2]
                                     | (omega_dlist_shadow[3] << 8));
        uint16_t gA = (uint16_t)(omega_dlist_shadow[8]
                                 | (omega_dlist_shadow[9] << 8));
        uint16_t gB = (uint16_t)(omega_dlist_shadow[10]
                                 | (omega_dlist_shadow[11] << 8));
        chk("initials LABS y word", labs_y, 0xA100);
        chk("initials LABS x word", labs_x, 0x21A0);
        chk("initial 1 glyph = 'A' JSRL", gA, 0xCC4D);
        chk("initial 2 glyph = 'B' JSRL", gB, 0xCC58);
    }

    press_fire(0x1B);        /* ERASE */
    chk("initials_cnt after erase", g.initials_cnt, 1);
    chk("erased cell is a space", g.initials_ptr[1], 0x20);

    press_fire(2);           /* 'C' */
    press_fire(3);           /* 'D' */
    chk("initials_cnt back to 3", g.initials_cnt, 3);
    chk("field reads A C D", (g.initials_ptr[0] == 'A' &&
                              g.initials_ptr[1] == 'C' &&
                              g.initials_ptr[2] == 'D'), 1);

    press_fire(0x1D);        /* END */
    chk("END leaves mode 4", g.game_mode, 1);
    printf("\n");
}

/* ---- 4. 2-player handoff (ROM 0x128E-0x12CF via 0x1478) -------------- */

extern void hiscore_entry_frame(void);
extern void player_state_save(int slot);

/* One pass of the real mode-4 wrapper with fire held on `sel`. Unlike
 * press_fire (which calls hiscore_initials directly), this goes through
 * HISCORE_ENTRY_FRAME, so an END press falls into the commit tail on
 * the same pass - exactly how the machine reaches 0x128E. */
static void fire_frame(int sel)
{
    g.obj[OBJ_SHIP].angle = (uint8_t)(sel * 2);
    g.obj[OBJ_SHIP].flags0 |= 0x04;
    g.seconds_ctr = 0;
    hiscore_entry_frame();
}

static void twoplayer_tests(void)
{
    puts("=== 2P mode-4 handoff (ROM 0x128E-0x12CF via 0x1478) ===");

    /* A finished 2-player game: P2 outscored P1, so frame.c's BCD
     * compare (0x145D) already picked P2 to enter first and set
     * game_flags bit 3. Stage that state and let P2's END press drive
     * the commit tail. */
    memset(&g, 0, sizeof g);
    game_init();
    g.game_flags |= 0x80;                     /* game in progress */

    g.score_cur[3] = 0x10;                    /* P1's (lower) score */
    player_state_save(0);
    g.score_cur[3] = 0x90;                    /* P2's, still loaded */
    player_state_save(1);

    g.game_flags |= 0x40 | 0x08;              /* 2P; P2 went first */
    g.player_sel = 2;                         /* two entries owed */
    g.cur_player_num = 2;

    hiscore_check();
    chk("P2 enters mode 4", g.game_mode, 4);
    ship_respawn();

    press_fire(1);  press_fire(2);            /* 'B' 'C' */
    fire_frame(0x1D);                         /* END -> commit tail */

    chk("wheel handed to P1 (mode 4)", g.game_mode, 4);
    chk("player_sel counted down", g.player_sel, 1);
    chk("current player is P1", g.cur_player_num, 1);
    chk("game_flags bit4 latched", g.game_flags & 0x10, 0x10);
    chk("P1 score restored", g.score_cur[3], 0x10);
    chk("fresh initials cursor", g.initials_cnt, 0);

    press_fire(0);  press_fire(3);            /* 'A' 'D' */
    fire_frame(0x1D);                         /* END -> both done */

    chk("mode back to attract", g.game_mode, 1);
    chk("player_sel exhausted", g.player_sel, 0);
    chk("score page 0x0F entered", g.page_id, 0x0F);
    chk("attract_step is 0x0C", g.attract_step, 0x0C);

    chk("table slot 0 = P2 score", g.hiscore[0][3], 0x90);
    chk("slot 0 initials BC",
        (g.hiscore[0][4] == 'B' && g.hiscore[0][5] == 'C'), 1);
    chk("table slot 1 = P1 score", g.hiscore[1][3], 0x10);
    chk("slot 1 initials AD",
        (g.hiscore[1][4] == 'A' && g.hiscore[1][5] == 'D'), 1);
    chk("bank 0 synced with slot 0", g.hiscore_bank[0][0][3], 0x90);

    /* The second player does NOT qualify: prefill the table above P1's
     * score. The handoff must fall out through fe_next_player (0x148E,
     * attract_step left at 9) to the score page - not re-enter the
     * wheel, and not do a second table commit. */
    memset(&g, 0, sizeof g);
    game_init();
    g.game_flags |= 0x80;
    { int i, j;
      for (i = 0; i < 6; i++) for (j = 0; j < 4; j++) g.hiscore[i][j] = 0x50; }

    memset(g.score_cur, 0, 4);                /* P1: qualifies nowhere */
    player_state_save(0);
    g.score_cur[3] = 0x99;                    /* P2 beats the table */
    player_state_save(1);

    g.game_flags |= 0x40 | 0x08;
    g.player_sel = 2;
    g.cur_player_num = 2;

    hiscore_check();
    chk("P2 enters mode 4 (case B)", g.game_mode, 4);
    ship_respawn();
    fire_frame(0x1D);                         /* END with blank initials */

    chk("P1 skipped (mode 1)", g.game_mode, 1);
    chk("fell out via fe_next_player", g.attract_step, 9);
    chk("score page shown anyway", g.page_id, 0x0F);
    printf("\n");
}

int main(void)
{
    wheel_tests();
    cursor_tests();
    typing_tests();
    twoplayer_tests();
    printf("%s\n", fails ? "PROBE FAILED" : "PROBE PASSED");
    return fails ? 1 : 0;
}
