// 腰带补药决定的单测：直接包含真的 dm_qol.c，不调任何原版地址。
// 要证明的事：没喝掉的正确档红药和蓝药，补药决定之后还在；错档的瓶子，同一列里还没有本难度最高档的替换瓶时留着。
// Unit test for the belt refill decision: includes the real dm_qol.c and calls no original address.
// What is to be proved: a correct-tier red bottle and a correct-tier blue bottle that were not drunk are still present after a refill decision; a wrong-tier bottle is kept when the replacement is not yet in that column.
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../dm_qol.c"

int dm_potions_drink_checks(void);

static uint32_t pot(char a, char b, char c) { char s[4] = { a, b, c, ' ' }; uint32_t v; memcpy(&v, s, 4); return v; }

int main(void) {
    int fails = 0;
    uint32_t hp5 = pot('h', 'p', '5'), hp3 = pot('h', 'p', '3'), mp5 = pot('m', 'p', '5'), mp3 = pot('m', 'p', '3');
    dm_belt_op B; memset(&B, 0, sizeof B); B.rows = 4;
    B.kind[0] = 1; B.kind[1] = 2; B.kind[2] = 1;
    B.present[0][0] = 1; B.code[0][0] = hp5;
    B.present[1][0] = 1; B.code[1][0] = mp5;
    B.present[2][0] = 1; B.code[2][0] = hp3;
    B.present[2][1] = 1; B.code[2][1] = mp3;
    int n = dm_belt_refill_decide(&B, 2);
    int red = B.present[0][0] && !B.removed[0][0] && !B.drunk[0][0];
    int blue = B.present[1][0] && !B.removed[1][0] && !B.drunk[1][0];
    int wrong = B.present[2][0] && !B.removed[2][0] && B.present[2][1] && !B.removed[2][1];
    if (n != 0 || !red || !blue || !wrong || B.notified != 0) {
        fails++; printf("  x belt keep: removed %d notified %d red %d blue %d wrong-tier %d\n", n, B.notified, red, blue, wrong);
    } else printf("belt OK: correct-tier red and correct-tier blue that were not drunk are still present; wrong-tier bottle kept when the replacement is not yet in that column\n");

    memset(&B, 0, sizeof B); B.rows = 4; B.kind[0] = 1; B.kind[1] = 2;
    B.present[0][0] = 1; B.code[0][0] = hp5;
    B.present[0][1] = 1; B.code[0][1] = hp3;
    B.present[1][0] = 1; B.code[1][0] = mp5;
    B.present[1][1] = 1; B.code[1][1] = mp3;
    n = dm_belt_refill_decide(&B, 2);
    if (n != 2 || !B.present[0][0] || B.present[0][1] || !B.removed[0][1] || !B.present[1][0] || B.present[1][1] || !B.removed[1][1] || B.notified != 2) {
        fails++; printf("  x belt replace: notified %d (want 2) red kept %d blue kept %d\n", B.notified, B.present[0][0], B.present[1][0]);
    } else printf("belt OK: wrong-tier removed only after the replacement is already in that column; client notified in the same operation (%d)\n", B.notified);

    memset(&B, 0, sizeof B); B.rows = 2; B.kind[0] = 1;
    B.present[0][0] = 1; B.code[0][0] = hp5; B.drunk[0][0] = 1;
    B.present[0][1] = 1; B.code[0][1] = hp3; B.drunk[0][1] = 1;
    n = dm_belt_refill_decide(&B, 2);
    if (n != 0 || B.removed[0][0] || B.removed[0][1] || B.notified != 0) {
        fails++; printf("  x belt drunk: removed %d notified %d\n", n, B.notified);
    } else printf("belt OK: a bottle already drunk is not removed again\n");

    fails += dm_potions_drink_checks();
    printf("t_potions %s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
