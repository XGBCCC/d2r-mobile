// 赌博概率和货架资格的单测：直接包含真的 dm_rift.c，调用 dm_gamble_rates 和 dm_gamble_shelf_eligible，不另写一份公式。
// 期望值手写：没更高秘境加成时暗金 10000、套装 20000；秘境第 2 层 10100/20200，第 11 层 11000/22000，第 100 层 19900/39800。
// Unit test for gamble rates and shelf eligibility: includes the real dm_rift.c and calls dm_gamble_rates and dm_gamble_shelf_eligible, with no second copy of the formula.
// Expected values are written by hand: unique 10000 and set 20000 when no higher rift bonus applies; rift level 2 is 10100/20200, level 11 is 11000/22000, level 100 is 19900/39800.
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../dm_rift.c"

void (*g_dm_pre_commit_hook[5])(const char *);
uint64_t dm_fs_commit_count(void) { return 0; }

static uint32_t code4(const char *s) {
    char b[4] = { ' ', ' ', ' ', ' ' };
    for (int i = 0; i < 4 && s[i]; i++) b[i] = s[i];
    uint32_t v; memcpy(&v, b, 4); return v;
}
static int rates_eq(int elysium, int diff, int best, int harr, uint32_t eu, uint32_t es) {
    uint32_t u = 0, s = 0; int rc = dm_gamble_rates(elysium, diff, best, harr, &u, &s);
    if (rc == 1 && u == eu && s == es) return 0;
    printf("  x elysium %d diff %d best %d harrogath %d: rc %d unique %u set %u, want %u %u\n", elysium, diff, best, harr, rc, u, s, eu, es);
    return 1;
}
static int rates_off(int diff, int best, int harr) {
    uint32_t u = 111, s = 222; int rc = dm_gamble_rates(0, diff, best, harr, &u, &s);
    if (rc == 0 && u == 111 && s == 222) return 0;
    printf("  x elysium off wrote rates: diff %d best %d harrogath %d rc %d unique %u set %u\n", diff, best, harr, rc, u, s);
    return 1;
}

int main(void) {
    int fails = 0;
    for (int d = 0; d < 3; d++) fails += rates_eq(1, d, 0, 0, 10000u, 20000u);
    for (int d = 0; d < 3; d++) fails += rates_eq(1, d, 100, 0, 10000u, 20000u);
    fails += rates_eq(1, 0, 100, 1, 10000u, 20000u);
    fails += rates_eq(1, 1, 100, 1, 10000u, 20000u);
    fails += rates_eq(1, 2, 0, 1, 10000u, 20000u);
    fails += rates_eq(1, 2, 1, 1, 10000u, 20000u);
    if (!fails) printf("gamble OK: elysium on, each difficulty unique is 10000 and set is 20000 per 100000 when no higher rift bonus applies\n");
    int off = 0;
    for (int d = 0; d < 3; d++) off += rates_off(d, 0, 0);
    off += rates_off(2, 11, 1);
    fails += off;
    if (!off) printf("gamble OK: elysium off does not apply the 10000/20000 floor\n");

    static const struct { int best; uint32_t u, s; } rift[] = { { 2, 10100u, 20200u }, { 11, 11000u, 22000u }, { 100, 19900u, 39800u } };
    for (unsigned i = 0; i < sizeof rift / sizeof *rift; i++) {
        uint32_t ru = 0, rs = 0, u = 0, s = 0;
        dm_rift_gamble_vals(rift[i].best, &ru, &rs);
        dm_gamble_rates(1, 2, rift[i].best, 1, &u, &s);
        if (ru != rift[i].u || rs != rift[i].s || u != ru || s != rs || u <= 10000u) {
            fails++; printf("  x rift best %d: rates %u/%u vals %u/%u want %u/%u\n", rift[i].best, u, s, ru, rs, rift[i].u, rift[i].s);
        } else printf("gamble OK: rift bonus best %d stays %u/%u, equals dm_rift_gamble_vals, not clamped down\n", rift[i].best, u, s);
    }

    if (!dm_gamble_shelf_eligible(1, 1, 0, 0, 1)) { fails++; printf("  x class item missing from the vanilla shelf was dropped\n"); }
    else printf("gamble OK: elysium on, any class item stays even with no low unique and even when gambling did not list it\n");
    if (dm_gamble_shelf_eligible(1, 0, 0, 0, 1)) { fails++; printf("  x non-class item absent from the vanilla shelf was added\n"); }
    else printf("gamble OK: a non-class item that gambling did not list is not added\n");
    if (!dm_gamble_shelf_eligible(1, 0, 1, 1, 1)) { fails++; printf("  x non-class item with a low unique was dropped\n"); }
    else printf("gamble OK: a non-class item with a low unique stays\n");
    if (!dm_gamble_shelf_eligible(1, 0, 1, 0, 1)) { fails++; printf("  x vanilla base with no low unique was dropped\n"); }
    else printf("gamble OK: elysium on, a vanilla base stays even with no low unique\n");
    if (!dm_gamble_shelf_eligible(1, 0, 1, 0, 0)) { fails++; printf("  x non-class item dropped while the filter is off\n"); }
    else printf("gamble OK: a non-class item stays when the filter is off\n");
    if (dm_gamble_shelf_eligible(0, 1, 0, 0, 1)) { fails++; printf("  x elysium off inserted a class item\n"); }
    else printf("gamble OK: elysium off does not insert a class-locked base\n");
    if (!dm_gamble_shelf_eligible(0, 0, 1, 0, 1)) { fails++; printf("  x elysium off dropped a vanilla base under the unique filter\n"); }
    else printf("gamble OK: elysium off keeps a base the shelf already had\n");

    printf("t_gamble %s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
