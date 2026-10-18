// 自动喝药决定的单测：直接包含真的 dm_actions.c。和 t_potions.c 分成两个编译单元，因为两份源里都有同名的 static。
// 要证明的事：生命 89% 喝红药、正好 90% 不喝；法力 89% 喝蓝药、正好 90% 不喝；缺省百分数是 90，改一个数两边一起变；紫药只在低于 30% 时选。
// Unit test for the auto-drink decision: includes the real dm_actions.c. Split from t_potions.c so the two sources' statics do not collide.
// What is to be proved: life 89% selects healing and life 90% does not; mana 89% selects mana and mana 90% does not; the default percent is 90 and one override changes both; rejuvenation is selected only below 30%.
#include <stdio.h>
#include <stdlib.h>
#include "../dm_actions.c"

static int want(const char *name, int got, int exp) {
    if (got == exp) return 0;
    printf("  x %s: got %d want %d\n", name, got, exp);
    return 1;
}

int dm_potions_drink_checks(void) {
    int fails = 0;
    unsetenv("DM_AUTOPOT_PCT"); unsetenv("DM_AUTOPOT_RED"); unsetenv("DM_AUTOPOT_REJUV");
    fails += want("default percent", dm_autopot_pct(), 90);
    fails += want("life 89", dm_autopot_select(0, 89, 100), 1);
    fails += want("life 90", dm_autopot_select(0, 90, 100), 0);
    fails += want("mana 89", dm_autopot_select(1, 89, 100), 2);
    fails += want("mana 90", dm_autopot_select(1, 90, 100), 0);
    fails += want("life 29 rejuvenation", dm_autopot_select(0, 29, 100), 3);
    fails += want("life 30 not rejuvenation", dm_autopot_select(0, 30, 100), 1);
    fails += want("mana 29 is mana", dm_autopot_select(1, 29, 100), 2);
    if (!fails) printf("drink OK: life 89%% selects healing and life 90%% does not; mana 89%% selects mana and mana 90%% does not; the default percent is 90; rejuvenation is selected only below 30%%\n");

    setenv("DM_AUTOPOT_RED", "70", 1);
    setenv("DM_AUTOPOT_PCT", "40", 1);
    fails += want("override percent", dm_autopot_pct(), 40);
    fails += want("override life 39", dm_autopot_select(0, 39, 100), 1);
    fails += want("override life 40", dm_autopot_select(0, 40, 100), 0);
    fails += want("override mana 39", dm_autopot_select(1, 39, 100), 2);
    fails += want("override mana 40", dm_autopot_select(1, 40, 100), 0);
    fails += want("override life 29 still rejuvenation", dm_autopot_select(0, 29, 100), 3);
    if (dm_autopot_pct() == 40 && dm_autopot_select(0, 39, 100) == 1 && dm_autopot_select(1, 39, 100) == 2)
        printf("drink OK: one override changes both life and mana\n");
    unsetenv("DM_AUTOPOT_PCT"); unsetenv("DM_AUTOPOT_RED"); unsetenv("DM_AUTOPOT_REJUV");
    return fails;
}
