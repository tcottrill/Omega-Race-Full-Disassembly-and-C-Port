/* pages.c - the page-reference resolver.
 *
 * DRAW_PAGE (0x2AF3) / PAGE_SCRIPT_STEP (0x2B6D) live in dvg_pages.c,
 * which interprets the machine's OWN page bytes out of omega_pagerom.c
 * (generated from the ROM by disasm/gen_pagerom.py) and writes real
 * display-list words - the generated blob cannot drift from the ROM.
 *
 * This file holds the one piece that is not ROM data: the map from a
 * listing address to the live RAM (or table) it names. The page scripts
 * embed pointers like 0x402D "credits_bcd" or 0x43D6 "hiscore row 1",
 * and something has to turn those into a pointer into `g`. dvg_pages.c
 * calls resolve_ref() for exactly that, on both the BCD-number and
 * string-reference payload parts.
 *
 * ADDRESS CONVENTION: a listing address names the LAST (most
 * significant) byte of a multi-byte BCD field, so an n-byte field
 * starts at (addr - n + 1). Every case below is written that way, and
 * dvg_pages.c's escape-pointer decode follows the same rule.
 *
 * g.hiscore_bank is uint8_t[2][6][7] (0x43D3 ctx0 / 0x43FD ctx1): page
 * 0x0F reads the "1 CREDIT GAME" table, page 0x10 the "2 CREDIT GAME"
 * one, and page 0x15's "HIGHEST SCORE" row reuses the same two
 * addresses (0x43D6/0x4400) as a plain BCD number with no initials.
 */
#include "omega_state.h"

/* ROM 0x2FDB: kill-tier point values, 2-byte BCD, used by page 2's
 * droid-icon point list. Duplicates score.c's POINTS_TABLE, which is
 * file-static there; if that table is ever exported, this copy can
 * become an extern. */
static const uint8_t PAGE_POINTS_TABLE[5][2] = {
    { 0x50, 0x03 },   /* 350  */
    { 0x00, 0x05 },   /* 500  */
    { 0x00, 0x10 },   /* 1000 */
    { 0x00, 0x15 },   /* 1500 */
    { 0x00, 0x25 },   /* 2500 */
};

/* resolve_ref: map a listing address to {ptr, is_bcd}. Returns 1 if
 * resolved, 0 (with *ptr = NULL) if not - callers render those blank.
 * See the big header comment for the ADDR-is-last-byte convention used
 * by every PART_NUM_BCD case below. */
int resolve_ref(uint16_t rom_addr, uint8_t** ptr, int* is_bcd) {
    *ptr = NULL;
    *is_bcd = 0;
    switch (rom_addr) {
    /* 0x402D credits_bcd (page 4 "CREDIT", page 0x15 "CURRENT CREDIT") */
    case 0x402D: *ptr = &g.credits_bcd; *is_bcd = 1; return 1;

    /* 0x2FDB-0x2FE4 points table (page 2); listing addr = last byte of
     * each 2-byte entry, base = addr-1 */
    case 0x2FDC: *ptr = (uint8_t*)&PAGE_POINTS_TABLE[0][0]; *is_bcd = 1; return 1;
    case 0x2FDE: *ptr = (uint8_t*)&PAGE_POINTS_TABLE[1][0]; *is_bcd = 1; return 1;
    case 0x2FE0: *ptr = (uint8_t*)&PAGE_POINTS_TABLE[2][0]; *is_bcd = 1; return 1;
    case 0x2FE2: *ptr = (uint8_t*)&PAGE_POINTS_TABLE[3][0]; *is_bcd = 1; return 1;
    case 0x2FE4: *ptr = (uint8_t*)&PAGE_POINTS_TABLE[4][0]; *is_bcd = 1; return 1;

    /* 0x4036-0x4039 score_cur (no page record currently cites it;
     * mapped for completeness) */
    case 0x4039: *ptr = &g.score_cur[0]; *is_bcd = 1; return 1;

    /* 0x43D3-0x43D9.. g.hiscore_bank[0] (1-credit table), 6 rows stride 7:
     * score (4 BCD, listing addr = last byte) then 3 initials (forward) */
    case 0x43D6: *ptr = &g.hiscore_bank[0][0][0]; *is_bcd = 1; return 1;
    case 0x43D7: *ptr = &g.hiscore_bank[0][0][4]; *is_bcd = 0; return 1;
    case 0x43DD: *ptr = &g.hiscore_bank[0][1][0]; *is_bcd = 1; return 1;
    case 0x43DE: *ptr = &g.hiscore_bank[0][1][4]; *is_bcd = 0; return 1;
    case 0x43E4: *ptr = &g.hiscore_bank[0][2][0]; *is_bcd = 1; return 1;
    case 0x43E5: *ptr = &g.hiscore_bank[0][2][4]; *is_bcd = 0; return 1;
    case 0x43EB: *ptr = &g.hiscore_bank[0][3][0]; *is_bcd = 1; return 1;
    case 0x43EC: *ptr = &g.hiscore_bank[0][3][4]; *is_bcd = 0; return 1;
    case 0x43F2: *ptr = &g.hiscore_bank[0][4][0]; *is_bcd = 1; return 1;
    case 0x43F3: *ptr = &g.hiscore_bank[0][4][4]; *is_bcd = 0; return 1;
    case 0x43F9: *ptr = &g.hiscore_bank[0][5][0]; *is_bcd = 1; return 1;
    case 0x43FA: *ptr = &g.hiscore_bank[0][5][4]; *is_bcd = 0; return 1;

    /* 0x43FD-0x4403.. g.hiscore_bank[1] (2-credit table), same layout */
    case 0x4400: *ptr = &g.hiscore_bank[1][0][0]; *is_bcd = 1; return 1;
    case 0x4401: *ptr = &g.hiscore_bank[1][0][4]; *is_bcd = 0; return 1;
    case 0x4407: *ptr = &g.hiscore_bank[1][1][0]; *is_bcd = 1; return 1;
    case 0x4408: *ptr = &g.hiscore_bank[1][1][4]; *is_bcd = 0; return 1;
    case 0x440E: *ptr = &g.hiscore_bank[1][2][0]; *is_bcd = 1; return 1;
    case 0x440F: *ptr = &g.hiscore_bank[1][2][4]; *is_bcd = 0; return 1;
    case 0x4415: *ptr = &g.hiscore_bank[1][3][0]; *is_bcd = 1; return 1;
    case 0x4416: *ptr = &g.hiscore_bank[1][3][4]; *is_bcd = 0; return 1;
    case 0x441C: *ptr = &g.hiscore_bank[1][4][0]; *is_bcd = 1; return 1;
    case 0x441D: *ptr = &g.hiscore_bank[1][4][4]; *is_bcd = 0; return 1;
    case 0x4423: *ptr = &g.hiscore_bank[1][5][0]; *is_bcd = 1; return 1;
    case 0x4424: *ptr = &g.hiscore_bank[1][5][4]; *is_bcd = 0; return 1;

    /* 0x4427+ bookkeeping ledger (page 0x15 operator menu), 4-byte BCD
     * entries per BOOKKEEP_PTR (index*4); indices confirmed against
     * score.c's bookkeep_ptr()/audit_snapshot() index usage:
     *   0 coin chute 1, 1 coin chute 2, 2 test credits,
     *   4/5/6   1-credit-context xlife-award-1/2/3 counts ("FIRST/SECOND/
     *           THIRD FREE SHIP" - award *counts*, not DIP thresholds),
     *   9       1-credit-context time_played highwater ("MAXIMUM SEC/GAME"),
     *   10/11/12 2-credit-context xlife-award-1/2/3 counts,
     *   15      2-credit-context time_played highwater */
    case 0x442A: *ptr = &g.bookkeep[0];  *is_bcd = 1; return 1; /* idx0 */
    case 0x442E: *ptr = &g.bookkeep[4];  *is_bcd = 1; return 1; /* idx1 */
    case 0x4432: *ptr = &g.bookkeep[8];  *is_bcd = 1; return 1; /* idx2 */
    case 0x443A: *ptr = &g.bookkeep[16]; *is_bcd = 1; return 1; /* idx4 */
    case 0x443E: *ptr = &g.bookkeep[20]; *is_bcd = 1; return 1; /* idx5 */
    case 0x4442: *ptr = &g.bookkeep[24]; *is_bcd = 1; return 1; /* idx6 */
    case 0x4452: *ptr = &g.bookkeep[40]; *is_bcd = 1; return 1; /* idx10 */
    case 0x4456: *ptr = &g.bookkeep[44]; *is_bcd = 1; return 1; /* idx11 */
    case 0x445A: *ptr = &g.bookkeep[48]; *is_bcd = 1; return 1; /* idx12 */
    case 0x444E: *ptr = &g.bookkeep[36]; *is_bcd = 1; return 1; /* idx9  */
    case 0x4466: *ptr = &g.bookkeep[60]; *is_bcd = 1; return 1; /* idx15 */

    /* 0x4471-0x4480 operator-menu pricing/audit display buffers (page 0x15).
     * 0x4471 = opmenu_disp1[0] (1-credit context), 0x4479 = opmenu_disp2[0]
     * (2-credit context); listing addr = last byte of each BCD run. */
    case 0x4474: *ptr = &g.opmenu_disp1[0]; *is_bcd = 1; return 1; /* avg score 1cr */
    case 0x4478: *ptr = &g.opmenu_disp1[5]; *is_bcd = 1; return 1; /* avg sec/game 1cr */
    case 0x447C: *ptr = &g.opmenu_disp2[0]; *is_bcd = 1; return 1; /* avg score 2cr */
    case 0x4480: *ptr = &g.opmenu_disp2[5]; *is_bcd = 1; return 1; /* avg sec/game 2cr */

    /* 0x4045: last byte of score_disp_shadow[4] (0x4042-45) */
    case 0x4045: *ptr = &g.score_disp_shadow[3]; *is_bcd = 1; return 1;

    /* 0x4061/0x4063/0x4065: xlife_thr[0..2], 2 BCD bytes each (fields
     * 0x4060-61/62-63/64-65). Encoding contract, from the ROM:
     * ATTRACT_INIT 0x0B16-0x0B38 copies the DIP table words (0x2FE5
     * bonus / 0x2FED difficulty) into these cells VERBATIM via
     * `ld (xlife_thrN),de`, and SCORE_ADD 0x236D compares the same
     * 16-bit image against the score's top BCD word at 0x4038 - the
     * value is BCD end to end, never converted to binary. The port's
     * DIP decode (irq_coins.c 0x0B16-0x0B38) stores the same table
     * words into uint16_t, so on this little-endian host the struct
     * bytes ARE the 0x4060-65 RAM image: byte[0]=LSB=0x4060 etc.,
     * which is the least-significant-first base bcd_digits() wants.
     * The page prints them MSB-first then a literal "0000" - table
     * word 0x0004 renders as "4" + "0000" = the 40000 the DIP means. */
    case 0x4061: *ptr = (uint8_t*)&g.xlife_thr[0]; *is_bcd = 1; return 1;
    case 0x4063: *ptr = (uint8_t*)&g.xlife_thr[1]; *is_bcd = 1; return 1;
    case 0x4065: *ptr = (uint8_t*)&g.xlife_thr[2]; *is_bcd = 1; return 1;

    default:
        return 0;
    }
}
