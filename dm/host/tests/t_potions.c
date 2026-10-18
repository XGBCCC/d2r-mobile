// 极乐锁血锁蓝之后，这个单测不再要求 90% 自动喝药，也不再要求腰带补药。
// 它包含真的 dm_qol.c，确认开着时生命和法力停在上限、关着时不动。
// After the Elysium life/mana hold, this test no longer requires the 90% auto-drink or the belt refill.
// It includes the real dm_qol.c and checks that life and mana stay at max while on, and are left alone while off.
#include <stdio.h>
#include "../dm_qol.c"

int dm_potions_drink_checks(void);

int main(void) {
    int fails = 0;
    if (dm_elysium_hold(1, 40, 100) != 100 || dm_elysium_hold(1, 10, 50) != 50) {
        fails++; printf("  x elysium on did not hold life and mana at max\n");
    } else printf("potions OK: elysium on holds life and mana at max\n");
    if (dm_elysium_hold(0, 40, 100) != 40 || dm_elysium_hold(0, 10, 50) != 10) {
        fails++; printf("  x elysium off changed life or mana\n");
    } else printf("potions OK: elysium off leaves life and mana alone\n");
    fails += dm_potions_drink_checks();
    printf("t_potions %s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
