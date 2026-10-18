// 极乐世界这一局里的三件小事（dm_qol.c）。决定是纯函数，宿主单测直接调用，不碰游戏映像。
//   · 当前生命、当前法力保持在各自上限（属性 6/7、8/9，游戏自己的取数，定点 ×256，不换算）。
//   · 身上装备的当前耐久（属性 72）掉了就写回掉之前的数。上限是 0 的（不会坏）不动。
//   · 身上金币（14）和箱内金币（15）的合计变少时，写回变少之前的数，花费当作 0。已经不低于上限的，写回上限减 1 并记住，绝不写上限本身：写成或超过上限会让赌博退出进程、卖东西把金币清掉。上限未知时不写，也不改记扣过之后的数。
// 单位是空的、物品栏魔数不是 0x01020304、人物模式是 0 或 17（死亡）时不写。服务端拍和客户端每帧各记各的玩家指针。
// 非极乐世界的局：清掉记着的数，不写生命、法力、耐久、金币，也不补腰带、不喝药、不自动卖杂物、不自动捡金币。
// Three small things during an Elysium run (dm_qol.c). The decision is a pure function; the host test calls it and does not touch the game image.
//   · Current life and current mana stay at their maxima (stats 6/7 and 8/9, the game's own values, fixed point ×256, not shifted).
//   · When current durability (stat 72) of an equipped item falls, write back the value from before the fall. Max 0 (indestructible) is left alone.
//   · When the sum of carried gold (14) and stash gold (15) falls, write back the values from before the fall, so a cost is 0. A total already at or above its cap is written back as cap-1 and remembered; the cap itself is never written: writing the cap or past it makes gambling exit and selling drain gold. An unknown cap is not written, and the reduced total is not remembered.
// Do not write when the unit is null, the inventory magic is not 0x01020304, or the player mode is 0 or 17 (dead). The server tick and the client frame each remember their own player pointer.
// Outside an Elysium run: forget the remembered numbers and do not write life, mana, durability or gold, and do not refill the belt, drink, auto-sell junk, or pick up gold piles.
#include "dm_elysium.h"
#include "hle.h"
#include <stdint.h>
#include <string.h>

int dm_elysium_hold(int on, int current, int maximum) {
    if (!on || maximum <= 0) return current;
    return maximum;
}
int dm_elysium_durability(int on, int current, int loss, int *write) {
    if (write) *write = on && loss > 0;
    return current;
}
static int gold_refund(int remembered, int cap, int *do_set) {
    if (cap <= 0) { *do_set = 0; return remembered; }          // 上限未知：不写，返回值仍是花费前的数 | unknown cap: do not write; the returned total stays pre-cost
    if (remembered >= cap) { *do_set = 1; return cap - 1; }    // 已经在上限上：写回上限减 1，绝不写上限 | already at the cap: restore to cap-1, never write the cap
    *do_set = 1; return remembered;
}
void dm_elysium_gold(int on, int carry, int stash, int cost, int carry_cap, int stash_cap, int *carry_out, int *stash_out, int *cost_out, int *write) {
    int set_carry = 0, set_stash = 0, co = carry, so = stash;
    if (on && cost > 0) { co = gold_refund(carry, carry_cap, &set_carry); so = gold_refund(stash, stash_cap, &set_stash); }
    if (carry_out) *carry_out = co;
    if (stash_out) *stash_out = so;
    if (cost_out) *cost_out = on ? 0 : cost;
    if (write) *write = (set_carry ? 1 : 0) | (set_stash ? 2 : 0);   // bit0 身上，bit1 箱内 | bit0 carry, bit1 stash
}

#define QMEM 2
#define QDURA 16
#define STASH_CAP 2500000
typedef struct { uint64_t pl; int carry, stash, gold_seen; uint32_t id[QDURA]; int dura[QDURA]; int nd; } qmem;
static qmem g_mem[QMEM];

static uint64_t qrd(uint64_t a) { return a ? *(uint64_t *)(uintptr_t)a : 0; }
static int unit_live(uint64_t pl) {
    uint32_t pm = *(uint32_t *)(uintptr_t)(pl + 0xc);
    if (pm == 0 || pm == 17) return 0;
    uint64_t inv = qrd(pl + 0x90);
    return inv && *(uint32_t *)(uintptr_t)inv == 0x01020304;
}
static qmem *mem_of(uint64_t pl) {
    int free_i = -1;
    for (int i = 0; i < QMEM; i++) { if (g_mem[i].pl == pl) return &g_mem[i]; if (!g_mem[i].pl && free_i < 0) free_i = i; }
    if (free_i < 0) { free_i = 0; memset(&g_mem[0], 0, sizeof g_mem[0]); }
    g_mem[free_i].pl = pl;
    return &g_mem[free_i];
}
static void apply_one(uint8_t *b, uint64_t pl) {
    int (*get)(uint64_t, int, int) = (int (*)(uint64_t, int, int))(b + 0x5b9290);
    void (*set)(uint64_t, int, int, int) = (void (*)(uint64_t, int, int, int))(b + 0x5bc1f0);
    int life = get(pl, 6, 0), lmax = get(pl, 7, 0), mana = get(pl, 8, 0), mmax = get(pl, 9, 0);
    int hl = dm_elysium_hold(1, life, lmax), hm = dm_elysium_hold(1, mana, mmax);
    if (hl != life) set(pl, 6, hl, 0);
    if (hm != mana) set(pl, 8, hm, 0);
    static int told; if (told < 1 && (hl != life || hm != mana)) { told++; LOG("极乐：生命 %d/%d、法力 %d/%d，保持在上限", life >> 8, lmax >> 8, mana >> 8, mmax >> 8); }

    qmem *M = mem_of(pl);
    uint64_t inv = qrd(pl + 0x90);
    uint32_t seen_id[QDURA]; int ns = 0;
    uint64_t q = qrd(inv + 0x10); int k = 0;
    for (; q && k < 512; k++) {   // 穿着的和箱子、背包、腰带在同一条链上，和 dm_actions.c / dm_lootfilter.c 一样走到 512 | equipped items share the chain with stash, inventory and belt; walk 512, same as dm_actions.c / dm_lootfilter.c
        uint64_t d = qrd(q + 0x10); if (!d) { q = 0; break; }
        if (*(uint32_t *)(uintptr_t)(q + 0xc) == 1) {
            int mx = get(q, 73, 0), cur = get(q, 72, 0);
            if (mx > 0 && cur >= 0) {
                uint32_t id = *(uint32_t *)(uintptr_t)(q + 8); int at = -1;
                for (int i = 0; i < M->nd; i++) if (M->id[i] == id) { at = i; break; }
                if (at < 0) { if (M->nd < QDURA) { M->id[M->nd] = id; M->dura[M->nd] = cur; M->nd++; } }
                else if (cur < M->dura[at]) {
                    int wr = 0, keep = dm_elysium_durability(1, M->dura[at], M->dura[at] - cur, &wr);
                    if (wr) set(q, 72, keep, 0);
                } else M->dura[at] = cur;
                if (ns < QDURA) seen_id[ns++] = id;
            }
        }
        q = qrd(d + 0xb0);
    }
    if (!q) {   // 链走完了才忘掉没再看见的。走到 512 还没完：留下已记住的 id，不因为窗口不够就丢掉 | Forget an id only when the chain ended. If 512 nodes were not enough, keep every tracked id
        int w = 0; for (int i = 0; i < M->nd; i++) { int keep = 0; for (int s = 0; s < ns; s++) if (seen_id[s] == M->id[i]) keep = 1; if (keep) { M->id[w] = M->id[i]; M->dura[w] = M->dura[i]; w++; } } M->nd = w;
    }

    int lv = get(pl, 12, 0), carry_cap = lv >= 1 && lv <= 99 ? lv * 10000 : 0;
    int carry = get(pl, 14, 0), stash = get(pl, 15, 0);
    if (!M->gold_seen) { M->carry = carry; M->stash = stash; M->gold_seen = 1; return; }
    long long was = (long long)M->carry + M->stash, now = (long long)carry + stash;
    if (now >= was) { M->carry = carry; M->stash = stash; return; }   // 转入箱子、从箱子取出、捡到金币：合计没少 | stash deposit, withdraw, or a pickup: the sum did not fall
    int co = 0, so = 0, costo = 0, wr = 0;
    dm_elysium_gold(1, M->carry, M->stash, (int)(was - now), carry_cap, STASH_CAP, &co, &so, &costo, &wr);
    if ((wr & 1) && carry != co) set(pl, 14, co, 0);   // co 已是函数给出的、严格低于上限的数 | co is the function's value, already strictly below the cap
    if ((wr & 2) && stash != so) set(pl, 15, so, 0);
    M->carry = co; M->stash = so;   // 没写的那一项也记花费前的数，不改记扣过之后的 | a pool that was not written still remembers the pre-cost total, not the reduced one
}
static void dm_qol_apply(uint8_t *b, uint64_t pl) {
    if (!dm_elysium_active()) { memset(g_mem, 0, sizeof g_mem); return; }
    if (!b || !pl || !unit_live(pl)) return;
    apply_one(b, pl);
}
void dm_qol_tick(uint8_t *b, uint64_t G, uint64_t pl) { (void)G; dm_qol_apply(b, pl); }
void dm_qol_guard(uint8_t *b) {
    if (!b) return;
    uint64_t pl = ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c));
    dm_qol_apply(b, pl);
}

// 极乐、交易商人开着（赌博页不算）：背包里的白装、低质量、超强、魔法装备逐件卖掉。不卖暗金、套装、稀有、工艺、带孔、无形、
// 护身符、珠宝、戒指、项链、符文、宝石、药水、任务物品，也不卖身上穿着的。不进修理态。玩家自己点了修理时这一帧不卖。
// Elysium, while a trade vendor is open (not the gambling page): sell white, low-quality, superior and magic gear in the inventory, one piece a frame.
// Uniques, sets, rares, crafted, socketed, ethereal, charms, jewels, rings, amulets, runes, gems, potions and quest items are not sold, nor is anything equipped. Repair mode is not entered. A frame the player is already repairing does not sell.
#define T_WEAP 45
#define T_ARMO 50
#define F_SOCK 0x800u
#define F_ETH  0x400000u
extern int dm_shop_open(void);
extern int dm_vendor_repair_mode(void);
extern int dm_item_quest(uint8_t *b, uint64_t it);
static int in_ids(const uint32_t *a, int n, uint32_t id) { for (int i = 0; i < n; i++) if (a[i] == id) return 1; return 0; }
static int junk(uint8_t *b, uint64_t it) {
    uint64_t d = qrd(it + 0x10); if (!d || *(uint32_t *)(uintptr_t)(it + 0xc) != 0 || *(uint8_t *)(uintptr_t)(d + 0x55) != 0) return 0;
    uint32_t q = *(uint32_t *)(uintptr_t)d, fl = *(uint32_t *)(uintptr_t)(d + 0x18); if (q < 1 || q > 4) return 0;
    if (fl & (F_SOCK | F_ETH)) return 0;
    int (*is)(uint64_t, int) = (int (*)(uint64_t, int))(b + 0x574bc0); if (!(is(it, T_WEAP) || is(it, T_ARMO))) return 0;
    if (((int (*)(uint64_t, int, int))(b + 0x5b9290))(it, 194, 0) > 0) return 0;
    if (dm_item_quest(b, it)) return 0;
    return ((int (*)(uint64_t))(b + 0x576c60))(it) != 0;
}
void dm_qol_vendor_pump(uint8_t *b) {
    static int open_was, phase, cool, tries, nskip, logged;
    static uint32_t cur, skip[40];
    if (!b || !dm_elysium_active()) { phase = 0; open_was = 0; return; }
    int shop = dm_shop_open(), gamble = *(uint32_t *)(b + 0x2d5eca8) != 0, trade = shop && !gamble;
    if (!trade) { if (phase && logged < 20) LOG("自动卖杂物：商人关了，停下"); phase = 0; open_was = 0; return; }
    if (!open_was) { open_was = 1; phase = 1; cool = tries = nskip = 0; cur = 0; if (logged < 20) LOG("自动卖杂物：商人开了，卖掉背包里的白装、低质量、超强和魔法"); logged++; }
    if (dm_vendor_repair_mode()) return;
    if (cool > 0) { cool--; return; }
    if (!phase) return;
    uint64_t pl = ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c)); if (!pl) return;
    uint64_t inv = qrd(pl + 0x90); if (!inv || *(uint32_t *)(uintptr_t)inv != 0x01020304 || qrd(inv + 0x40)) return;
    uint64_t it = 0;
    if (cur) { uint64_t f = ((uint64_t (*)(uint64_t, uint32_t, uint32_t, uint32_t))(b + 0x45f5f0))((uint64_t)(uintptr_t)(b + 0x2d603a0 + 4 * 0x400), cur & 0x7f, cur, 4); if (f && junk(b, f)) it = f; else cur = 0; }
    if (!it) for (uint64_t q = qrd(inv + 0x10), k = 0; q && k < 512 && !it; k++) { uint64_t d = qrd(q + 0x10); if (!d) break; uint32_t id = *(uint32_t *)(uintptr_t)(q + 8); if (!in_ids(skip, nskip, id) && junk(b, q)) { it = q; cur = id; } q = qrd(d + 0xb0); }
    if (!it) { phase = 0; cur = 0; if (logged < 20) LOG("自动卖杂物：处理完"); return; }
    int r = ((int (*)(uint64_t, uint64_t, int, int, int, int))(b + 0x433e50))(pl, it, 1, 0, 1, 0); cool = 32;
    if (r == 1) { if (nskip < 40) skip[nskip++] = cur; if (logged < 40) { uint32_t cd = ((uint32_t (*)(uint64_t))(b + 0x570260))(it); LOG("自动卖杂物：卖掉 %.4s（id %u）", (const char *)&cd, cur); } cur = 0; tries = 0; logged++; }
    else if (++tries >= 2) { if (nskip < 40) skip[nskip++] = cur; cur = 0; tries = 0; }
}
