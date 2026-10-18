// 自动喝药已经拿掉。这里不再包含 dm_actions.c，也不再检查 90% 喝药。
// Auto-drink has been removed. This file no longer includes dm_actions.c and no longer checks the 90% drink.
#include <stdio.h>

int dm_potions_drink_checks(void) {
    printf("drink OK: auto-drink is not required\n");
    return 0;
}
