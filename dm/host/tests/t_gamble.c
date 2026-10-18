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
static int rates_eq(int diff, int best, int harr, uint32_t eu, uint32_t es) {
    uint32_t u = 0, s = 0; dm_gamble_rates(diff, best, harr, &u, &s);
    if (u == eu && s == es) return 0;
    printf("  x diff %d best %d harrogath %d: unique %u set %u, want %u %u\n", diff, best, harr, u, s, eu, es);
    return 1;
}

int main(void) {
    int fails = 0;
    for (int d = 0; d < 3; d++) fails += rates_eq(d, 0, 0, 10000u, 20000u);
    for (int d = 0; d < 3; d++) fails += rates_eq(d, 100, 0, 10000u, 20000u);
    fails += rates_eq(0, 100, 1, 10000u, 20000u);
    fails += rates_eq(1, 100, 1, 10000u, 20000u);
    fails += rates_eq(2, 0, 1, 10000u, 20000u);
    fails += rates_eq(2, 1, 1, 10000u, 20000u);
    if (!fails) printf("gamble OK: each difficulty unique is 10000 and set is 20000 per 100000 when no higher rift bonus applies\n");

    static const struct { int best; uint32_t u, s; } rift[] = { { 2, 10100u, 20200u }, { 11, 11000u, 22000u }, { 100, 19900u, 39800u } };
    for (unsigned i = 0; i < sizeof rift / sizeof *rift; i++) {
        uint32_t ru = 0, rs = 0, u = 0, s = 0;
        dm_rift_gamble_vals(rift[i].best, &ru, &rs);
        dm_gamble_rates(2, rift[i].best, 1, &u, &s);
        if (ru != rift[i].u || rs != rift[i].s || u != ru || s != rs || u <= 10000u) {
            fails++; printf("  x rift best %d: rates %u/%u vals %u/%u want %u/%u\n", rift[i].best, u, s, ru, rs, rift[i].u, rift[i].s);
        } else printf("gamble OK: rift bonus best %d stays %u/%u, equals dm_rift_gamble_vals, not clamped down\n", rift[i].best, u, s);
    }

    static const char *fam[] = { "abow", "ajav", "phlm", "ashd", "pelt", "head", "orb", "aspe" };
    for (unsigned i = 0; i < sizeof fam / sizeof *fam; i++) {
        if (!dm_gamble_shelf_eligible(code4(fam[i]), 0, 0, 1, 0)) { fails++; printf("  x %s not eligible\n", fam[i]); }
        else printf("gamble OK: %s eligible\n", fam[i]);
    }
    if (!dm_gamble_shelf_eligible(code4("grim"), 0, 0, 1, 1)) { fails++; printf("  x grim with warlock not eligible\n"); }
    else printf("gamble OK: grim eligible with warlock_in_table=1\n");
    if (dm_gamble_shelf_eligible(code4("grim"), 0, 0, 1, 0)) { fails++; printf("  x grim with warlock_in_table=0 and on_vanilla=0 was eligible\n"); }
    else printf("gamble OK: grim with warlock_in_table=0 and on_vanilla=0 is not eligible\n");
    if (!dm_gamble_shelf_eligible(code4("swor"), 1, 1, 1, 0)) { fails++; printf("  x swor dropped\n"); }
    else printf("gamble OK: previously eligible non-class base swor stays\n");
    if (!dm_gamble_shelf_eligible(code4("h2h"), 1, 0, 1, 0)) { fails++; printf("  x h2h dropped\n"); }
    else printf("gamble OK: assassin claw h2h stays eligible\n");
    if (!dm_gamble_shelf_eligible(code4("abow"), 0, 0, 1, 0)) { fails++; printf("  x abow with no low unique dropped\n"); }
    else printf("gamble OK: class-specific abow stays eligible with no qualifying unique at the low item level\n");
    if (dm_gamble_shelf_eligible(code4("swor"), 1, 0, 1, 0)) { fails++; printf("  x swor with no low unique stayed under the filter\n"); }
    else printf("gamble OK: non-class swor with no low unique is not eligible when the filter is on\n");

    printf("t_gamble %s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
