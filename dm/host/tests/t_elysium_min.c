// 极乐三件事的单测：直接包含真的 dm_qol.c，调用 dm_elysium_hold / dm_elysium_durability / dm_elysium_gold，不另写一份规则。
// Host test for the three Elysium decisions: includes the real dm_qol.c and calls dm_elysium_hold / dm_elysium_durability / dm_elysium_gold, with no second copy of the rule.
#include <stdio.h>
#include "../dm_qol.c"

static int fails;

static void check(const char *name, int ok) {
    if (ok) printf("%s\n", name);
    else { fails++; printf("  x %s\n", name); }
}

int main(void) {
    check("elysium on: life held at max", dm_elysium_hold(1, 40, 100) == 100);
    check("elysium on: mana held at max", dm_elysium_hold(1, 10, 50) == 50);
    check("elysium on: life already at max stays max", dm_elysium_hold(1, 100, 100) == 100);
    check("elysium on: non-positive max leaves life alone", dm_elysium_hold(1, 40, 0) == 40);
    check("elysium off: life left alone", dm_elysium_hold(0, 40, 100) == 40);
    check("elysium off: mana left alone", dm_elysium_hold(0, 10, 50) == 10);

    int wr = 0;
    int keep = dm_elysium_durability(1, 12, 3, &wr);
    check("elysium on: durability loss not applied", keep == 12 && wr == 1);
    wr = 1;
    keep = dm_elysium_durability(1, 12, 0, &wr);
    check("elysium on: no durability loss does not write", keep == 12 && wr == 0);
    wr = 1;
    keep = dm_elysium_durability(0, 12, 3, &wr);
    check("elysium off: durability left alone", keep == 12 && wr == 0);

    int co = -1, so = -1, costo = -1;
    wr = 0;
    dm_elysium_gold(1, 5000, 1000, 400, 100000, 2500000, &co, &so, &costo, &wr);
    check("elysium on: gold cost leaves totals unchanged", co == 5000 && so == 1000 && costo == 0 && wr == 3);
    check("elysium on: gold cap not assigned", co != 100000 && so != 2500000 && co < 100000 && so < 2500000);
    co = so = costo = -1; wr = 0;
    dm_elysium_gold(1, 100000, 1000, 400, 100000, 2500000, &co, &so, &costo, &wr);
    check("elysium on: at-cap carry restores below cap", co == 99999 && so == 1000 && co < 100000 && (wr & 1) && costo == 0);
    co = so = costo = -1; wr = 0;
    dm_elysium_gold(1, 5000, 2500000, 400, 100000, 2500000, &co, &so, &costo, &wr);
    check("elysium on: at-cap stash restores below cap", co == 5000 && so == 2499999 && so < 2500000 && (wr & 2) && costo == 0);
    co = so = costo = -1; wr = 0;
    dm_elysium_gold(1, 100001, 2500001, 400, 100000, 2500000, &co, &so, &costo, &wr);
    check("elysium on: above-cap gold restores below cap", co == 99999 && so == 2499999 && co < 100000 && so < 2500000 && costo == 0);
    co = so = costo = -1; wr = 7;
    dm_elysium_gold(1, 5000, 1000, 400, 0, 2500000, &co, &so, &costo, &wr);
    check("elysium on: unknown carry cap does not write carry", co == 5000 && so == 1000 && (wr & 1) == 0 && (wr & 2) && costo == 0);
    co = so = costo = -1; wr = 1;
    dm_elysium_gold(0, 5000, 1000, 400, 100000, 2500000, &co, &so, &costo, &wr);
    check("elysium off: gold left alone", co == 5000 && so == 1000 && costo == 400 && wr == 0);
    co = so = -1; costo = 7; wr = 1;
    dm_elysium_gold(1, 5000, 1000, 0, 100000, 2500000, &co, &so, &costo, &wr);
    check("elysium on: zero cost does not write gold", co == 5000 && so == 1000 && costo == 0 && wr == 0);

    printf("t_elysium_min %s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
