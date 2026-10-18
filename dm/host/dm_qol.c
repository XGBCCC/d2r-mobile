// 便利操作，都是自动触发，不加按钮（dm_qol.c）：
//   · 腰带补药（服务端拍）：红 / 蓝始终是当前难度商人卖的最高档（普通 hp3/mp3、噩梦 hp4/mp4、地狱 hp5/mp5），喝掉立刻补。
//     列的安排：第 1 列红、第 2 列蓝、其余红；地狱且至少已有红、蓝两列时，最后一列是满紫（rvl），只在跟商人交易时补（赌博不算）。
//     已经在腰带里的红 / 蓝，没被喝掉就留着。档位不对的那一瓶，要等同一列里已经有本难度最高档的替换瓶（同一次操作里先放进去），才摘掉旧的。
//     生成失败、地上孤儿瓶、暂停补药、回城、开面板，都不能把还在的那瓶弄没。摘掉和放进都在这一拍里通知客户端。
//     排数不够 4 就只补这一列现有的格子，不拿红 / 蓝的列去凑 4 瓶紫。底格是解毒、卷轴之类的列不动。背包里的药不删。
//     开关沿用自定义页 pot_pick（缺省开）。关着时不补，开背包从背包补药（dm_beltauto.c）照旧。DM_POT_PICK=0 时两边都不动（界面测试用）。
//   · 拾取放不下：diag.c 先按住这一个 0x16，主线程整理背包一次再重发一次；还是放不下就走原来的进方块。
//   · 打开交易商人（不是赌博页）：背包里的白装、低质量、超强、魔法装备逐件卖掉。不卖暗金、套装、稀有、工艺、带孔、无形、
//     护身符、珠宝、戒指、项链、符文、宝石、药水、任务物品，也不卖身上穿着的。会修理的商人（「全部修理」按钮可见）接着逐件修玩家和佣兵身上的装备。
//     卖和修都只在商人页开着时进行，一帧一件；修理必须先进入原版修理态，否则 0x433e50 会把装备卖掉。
//     原版修理一定会弹出「是否修理」（免确认那个参数只覆盖买和卖）。弹出之后立刻走确认回调，不再问。
//     身上金币保持在本级上限（等级 × 1 万），箱内金币保持 250 万。少了补回，多了压回。
//     超过上限时，赌博结算会直接退出进程，卖东西会把身上金币清掉或往下掉。
// Convenience actions, all automatic, no new buttons (dm_qol.c):
//   · Belt refill (server tick): healing / mana stay at the best tier this difficulty's vendors sell (Normal hp3/mp3, Nightmare hp4/mp4, Hell hp5/mp5) and refill as soon as one is drunk.
//     Columns: column 1 healing, column 2 mana, the rest healing; on Hell, once a healing column and a mana column already exist, the last column is full rejuvenation (rvl), refilled only while trading with a vendor (gambling does not count).
//     A healing or mana bottle already in the belt stays until it is drunk, or until a top-tier replacement for that column is already in the belt in the same operation. The old bottle is detached only after that replacement has landed, and the client is told about both changes in that same tick.
//     A failed spawn, an orphan ground bottle, a refill pause, returning to town, or opening a panel does not remove a bottle that is still there. DM_POT_PICK=0 turns both this and the inventory refill off (used by UI tests).
//     With fewer than 4 rows only that column's existing cells are filled; healing / mana columns are not taken to force 4 rejuvenation potions. A column whose bottom is an antidote, a scroll and the like is left alone. Potions in the inventory are not deleted.
//   · A pickup that does not fit: diag.c holds that one 0x16, the main thread sorts the inventory once and resends once; if it still does not fit, the original cube path runs.
//   · Opening a trade vendor (not the gambling page): white, low-quality, superior and magic gear in the inventory is sold one piece at a time. Uniques, sets, rares, crafted, socketed, ethereal,
//     charms, jewels, rings, amulets, runes, gems, potions and quest items are not sold, nor is anything equipped. A vendor that repairs ("Repair All" visible) then repairs the player's and the mercenary's equipped items one at a time.
//     Selling and repairing happen only while the vendor page stays open, one item per frame; repair must enter the original repair mode first, or 0x433e50 sells the item.
//     The original repair always asks "repair?" (the no-confirm argument covers only buy and sell). The confirm callback runs as soon as that dialog is up, so it is not asked.
//     Carried gold stays at this level's cap (level × 10,000) and stash gold stays at 2,500,000. Below the cap is filled back; above the cap is clamped down.
//     Above the cap, the gamble transaction calls exit, and selling clears or drains the gold carried.
#include "dm_actions.h"
#include "dm_cursor_item.h"
#include "dm_custom.h"
#include "hle.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define T_WEAP 45
#define T_ARMO 50
#define F_SOCK 0x800u
#define F_ETH  0x400000u
#define MODE_KEY 0x838e27f92c679132ull
#define STASH_KEEP 2500000   // 箱内金币上限，和原版「金币最大数」一样 | stash gold cap, the same as the original "max gold"

extern int dm_diag_difficulty(void);
extern int dm_item_quest(uint8_t *b, uint64_t it);
extern uint64_t dm_diag_srv_item(uint32_t id);

static uint64_t rd(uint64_t a) { return a ? *(uint64_t *)(uintptr_t)a : 0; }
static uint32_t pot_code(char a, char b, char c) { char s[4] = { a, b, c, ' ' }; uint32_t v; memcpy(&v, s, 4); return v; }
static int pot_kind(uint32_t c) {   // 1 红 / 2 蓝 / 3 紫 / 0 别的 | 1 healing / 2 mana / 3 rejuvenation / 0 other
    char s[4]; memcpy(s, &c, 4);
    if (s[0] == 'h' && s[1] == 'p' && s[2] >= '1' && s[2] <= '5') return 1;
    if (s[0] == 'm' && s[1] == 'p' && s[2] >= '1' && s[2] <= '5') return 2;
    if (s[0] == 'r' && s[1] == 'v' && (s[2] == 's' || s[2] == 'l')) return 3;
    return 0;
}
// 这一格能不能被补药摘掉。kind 1 红 / 2 蓝 / 3 紫。drunk = 这一瓶已经被人物或佣兵喝掉（游戏自己已经拿走，这里不再摘一次）。
// 档位正确、又没喝掉：留着。档位不对：同一列里已经有本难度最高档的替换瓶才可以摘。生成失败不能当成已经换上。
// May refill detach this cell. kind 1 healing / 2 mana / 3 rejuvenation. drunk = the player or the mercenary already drank it (the game has taken it; do not detach it again).
// A correct-tier bottle that was not drunk stays. A wrong-tier bottle may be detached only when a top-tier replacement is already in that column. A failed spawn is not a replacement.
int dm_belt_may_remove(int kind, uint32_t code, uint32_t want, int replacement_in_column, int drunk) {
    if (drunk || (kind != 1 && kind != 2 && kind != 3)) return 0;
    if (!code || !want || code == want) return 0;
    return replacement_in_column ? 1 : 0;
}
typedef struct { int rows, kind[4], present[4][4], drunk[4][4], removed[4][4], notified; uint32_t code[4][4]; } dm_belt_op;
static uint32_t belt_want(int diff, int kind) {
    if (diff < 0 || diff > 2) return 0;
    if (kind == 1) return pot_code('h', 'p', diff == 0 ? '3' : diff == 1 ? '4' : '5');
    if (kind == 2) return pot_code('m', 'p', diff == 0 ? '3' : diff == 1 ? '4' : '5');
    if (kind == 3) return pot_code('r', 'v', 'l');
    return 0;
}
// 一次补药决定：只按 dm_belt_may_remove 清格子。替换瓶 = 同一列里已经在、没喝掉、码就是这一列最高档的另一格。每摘一格 notified +1（客人路径在同一次操作里通知客户端）。
// One refill decision: a cell is cleared only by dm_belt_may_remove. The replacement is another cell in the same column that is present, not drunk, and already holds that column's top tier. Each removal adds 1 to notified (the guest path tells the client in the same operation).
int dm_belt_refill_decide(dm_belt_op *B, int diff) {
    if (!B) return 0;
    int nrem = 0;
    for (int c = 0; c < 4; c++) {
        uint32_t want = belt_want(diff, B->kind[c]); int repl = 0;
        for (int r = 0; r < B->rows && r < 4; r++) if (B->present[c][r] && !B->drunk[c][r] && want && B->code[c][r] == want) repl = 1;
        for (int r = 0; r < B->rows && r < 4; r++) {
            B->removed[c][r] = 0; if (!B->present[c][r]) continue;
            if (!dm_belt_may_remove(B->kind[c], B->code[c][r], want, repl, B->drunk[c][r])) continue;
            B->present[c][r] = 0; B->removed[c][r] = 1; B->notified++; nrem++;
        }
    }
    return nrem;
}

static void gold_keep(uint8_t *b, uint64_t pl) {   // 身上压在等级×1万，箱内压在 250 万。买卖扣的是服务端这份，界面读的是客户端这份，两边都写 | Carry stays at level×10000, stash at 2,500,000. Purchases deduct the server copy; the UI reads the client copy; both are written
    static int told;
    if (!b || !pl) return;
    int (*get)(uint64_t, int, int) = (int (*)(uint64_t, int, int))(b + 0x5b9290);
    void (*set)(uint64_t, int, int, int) = (void (*)(uint64_t, int, int, int))(b + 0x5bc1f0);
    int lv = get(pl, 12, 0); if (lv < 1) return; if (lv > 99) lv = 99;   // 等级还没读到就别写，免得把金币写成 0 | Do not write before level is readable, or gold would be set to 0
    int cap = lv * 10000, g = get(pl, 14, 0);
    if (g != cap) { set(pl, 14, cap, 0); if (told < 2) { told++; LOG("金币：身上 %d，改到本级上限 %d（多了会崩、会掉，少了补回）", g, cap); } }
    int s = get(pl, 15, 0); if (s != STASH_KEEP) set(pl, 15, STASH_KEEP, 0);
}
int dm_beltfill_on(void) {   // 与 dm_potpick.c / dm_beltauto.c 同一个开关：DM_POT_PICK 优先，否则自定义页 pot_pick | Same switch as dm_potpick.c / dm_beltauto.c: DM_POT_PICK wins, otherwise the Custom page pot_pick
    static int env = -2; if (env == -2) { const char *e = getenv("DM_POT_PICK"); env = e ? e[0] != '0' : -1; }
    return env >= 0 ? env : dm_custom_get("pot_pick");
}

// ---------- 腰带补药（服务端拍）----------
typedef struct { int rows, boxes; uint64_t cell[4][4]; uint32_t code[4][4]; int managed[4], target[4]; } belt;
static int belt_read(uint8_t *b, uint64_t pl, belt *B) {
    memset(B, 0, sizeof *B);
    uint64_t inv = rd(pl + 0x90); if (!inv || *(uint32_t *)(uintptr_t)inv != 0x01020304) return 0;
    uint64_t pages = rd(inv + 0x20); if (!pages || rd(inv + 0x28) < 2) return 0;
    uint64_t pg = rd(pages + 0x18); if (!pg) return 0; uint64_t worn = rd(pg + 0x40);   // rd 只挡地址 0，空页再 +0x40 会去读 0x40 | rd only guards address 0; a null page plus 0x40 reads 0x40
    int type = worn ? ((int (*)(uint64_t))(b + 0x5cc9b0))(worn) : 2;
    uint8_t ver = *(uint8_t *)(uintptr_t)(pl + 0x1bd); if (ver < 1 || ver > 3) return 0;
    uint64_t T = rd((uint64_t)(uintptr_t)(b + 0x2d64a18 + ver * 16)), rows = T ? rd(T + 0x1578) : 0; if (!rows) return 0;
    B->boxes = *(uint8_t *)(uintptr_t)(rows + (uint64_t)type * 0x120 + 4); if (B->boxes > 16) B->boxes = 16;
    B->rows = B->boxes / 4; if (B->rows < 1) return 0; if (B->rows > 4) B->rows = 4;
    uint64_t bg = rd(pages + 0x20 + 0x18); if (!bg) return 0;
    for (int c = 0; c < 4; c++) { int other = 0;
        for (int r = 0; r < B->rows; r++) { int i = r * 4 + c; if (i >= B->boxes) continue; uint64_t it = rd(bg + 8 * (uint64_t)i); B->cell[c][r] = it;
            if (!it) continue; B->code[c][r] = ((uint32_t (*)(uint64_t))(b + 0x570260))(it); if (!pot_kind(B->code[c][r])) other = 1; }
        B->managed[c] = !other; }
    return 1;
}
static int col_count(const belt *B, int c) { int n = 0; for (int r = 0; r < B->rows; r++) n += B->cell[c][r] != 0; return n; }
static int col_gap(const belt *B, int c) { for (int r = 0; r < B->rows; r++) if (!B->cell[c][r]) return 1; return 0; }
static int predict(const belt *B, uint32_t code) {   // 原版找位的次序：底格同码且有空位的最左列，否则最左空列 | Original finder order: leftmost column whose bottom matches and has a gap, else the leftmost empty column
    for (int c = 0; c < 4; c++) if (B->cell[c][0] && B->code[c][0] == code && col_gap(B, c)) return c;
    for (int c = 0; c < 4; c++) if (B->managed[c] && col_count(B, c) == 0) return c;
    return -1;
}
static void belt_remove(uint8_t *b, uint64_t G, uint64_t pl, uint64_t it) {
    if (*(uint32_t *)(uintptr_t)(it + 0xc) != 2) return;   // 只摘腰带上的，背包里的药留下 | Only detach a belt item; potions in the inventory stay
    uint64_t d = rd(it + 0x10), pp = rd(it + 0x38); int page = d ? *(uint8_t *)(uintptr_t)(d + 0x55) : 1;
    int x = pp ? *(int32_t *)(uintptr_t)(pp + 0x10) : 0, y = pp ? *(int32_t *)(uintptr_t)(pp + 0x14) : 0;
    ((void (*)(uint64_t))(b + 0x55e430))(it);
    ((void (*)(uint64_t, uint64_t, uint64_t, int, int, int, int, int, int))(b + 0x642f70))(G, pl, it, 0x20, 4, 0, page, y, x);
    ((void (*)(uint64_t, uint64_t))(b + 0x658be0))(G, it);
}
static int spawn_ok(uint8_t *b, uint64_t G, uint64_t pl, uint32_t code, int want, uint32_t *left) {
    if (left) *left = 0;
    uint64_t u = ((uint64_t (*)(uint64_t, uint64_t, uint32_t, int))(b + 0x7c0b20))(G, pl, code, 2); if (!u) return 0;
    uint64_t inv = rd(pl + 0x90); uint32_t s1 = 0, s0 = 0;
    int a = inv && ((int (*)(uint64_t, uint64_t, uint32_t *, int))(b + 0x561de0))(inv, u, &s1, 1);
    int c = inv && ((int (*)(uint64_t, uint64_t, uint32_t *, int))(b + 0x561de0))(inv, u, &s0, 0);
    if (!a || !c || (int)(s1 % 4) != want || (int)(s0 % 4) != want) {   // 两种找位不一致就不要：发了会进背包。只记住刚生成的这一瓶 | Do not send when the two finder flags disagree: the potion would land in the inventory. Remember only the bottle just spawned
        if (left && *(uint32_t *)(uintptr_t)(u + 0xc) == 3) *left = *(uint32_t *)(uintptr_t)(u + 8); return -1; }
    uint64_t ip = rd(u + 0x38); uint8_t pk[17] = { 0x16 }; uint32_t one = 1; memcpy(pk + 1, (void *)(uintptr_t)(u + 8), 4);
    if (ip) { memcpy(pk + 5, (void *)(uintptr_t)(ip + 0x10), 4); memcpy(pk + 9, (void *)(uintptr_t)(ip + 0x14), 4); } memcpy(pk + 13, &one, 4);
    *(uint32_t *)(uintptr_t)(G + 0x19050) = 1; ((void (*)(uint64_t, uint64_t, uint8_t *, uint32_t))(b + 0x6e5b40))(G, pl, pk, 17); *(uint32_t *)(uintptr_t)(G + 0x19050) = 0;
    if (*(uint32_t *)(uintptr_t)(u + 0xc) == 2) return 1;
    if (left && *(uint32_t *)(uintptr_t)(u + 0xc) == 3) *left = *(uint32_t *)(uintptr_t)(u + 8);
    return -1;
}

void dm_qol_tick(uint8_t *b, uint64_t G, uint64_t pl) {
    static uint32_t orphan, seen; static uint64_t seen_g; static int pause, logged;
    if (!b || !G || !pl) return;
    gold_keep(b, pl);   // 服务端这份：修理和买东西扣的是它 | server copy: repair and purchases deduct this one
    if (!dm_beltfill_on()) return;
    uint32_t pid = *(uint32_t *)(uintptr_t)(pl + 8); if (pid != seen || G != seen_g) { seen = pid; seen_g = G; orphan = 0; pause = 0; }   // 换局后物品号会重用，上一局地上那瓶不能再挡住这一局 | Item ids are reused in the next game, so a ground bottle from the previous game must not block this one
    if (pause > 0) { pause--; return; }
    uint32_t pm = *(uint32_t *)(uintptr_t)(pl + 0xc); if (pm == 0 || pm == 17) return;
    int diff = dm_diag_difficulty(); if (diff < 0 || diff > 2) return;
    uint64_t inv = rd(pl + 0x90); if (inv && rd(inv + 0x40)) return;
    belt B; if (!belt_read(b, pl, &B)) return;
    int idx[4], n = 0; for (int c = 0; c < 4; c++) if (B.managed[c]) idx[n++] = c; if (!n) return;
    B.target[idx[0]] = 1; if (n >= 2) B.target[idx[1]] = 2; for (int i = 2; i < n; i++) B.target[idx[i]] = 1;
    if (diff == 2 && n >= 3) B.target[idx[n - 1]] = 3;   // 地狱最后一列满紫；只有红、蓝两列时不抢 | On Hell the last column is full rejuvenation; two columns stay healing and mana
    uint64_t pd = rd(pl + 0x10); int vendor = pd && *(uint8_t *)(uintptr_t)(pd + 0x100) == 2;
    uint32_t hp = pot_code('h', 'p', diff == 0 ? '3' : diff == 1 ? '4' : '5'), mp = pot_code('m', 'p', diff == 0 ? '3' : diff == 1 ? '4' : '5'), rv = pot_code('r', 'v', 'l');
    // 不先摘。错档的瓶子留在腰带上，直到这一拍里最高档的替换瓶已经进了同一列（下面 spawn_ok 成功之后才 belt_remove）。
    // Do not detach first. A wrong-tier bottle stays in the belt until this tick has already put a top-tier replacement in the same column (belt_remove runs only after spawn_ok succeeds).
    // 先定这一拍补哪一种，再问原版找位会落进哪一列。不能先指定一条空着的红列：
    // 找位看到别的红列底格已是红药、还有空位，就会落回那一列；指定对不上就整拍跳过，蓝药和其余红列再也不会补。
    // 顺序：还没有任何红药就先补一瓶红；否则先把蓝列补满，再补其余红列；地狱的满紫只在交易时补。
    // Pick the kind for this tick first, then ask the original finder which column it would land in. Do not nominate an empty healing column first:
    // if another healing column already has a healing potion at the bottom and a free cell, the finder lands there; a mismatch skips the tick, and mana plus the other healing columns never fill.
    // Order: one healing potion while none is present; otherwise fill the mana column, then the other healing columns; full rejuvenation on Hell only while trading.
    int have_heal = 0; for (int c = 0; c < 4; c++) if (B.target[c] == 1 && col_count(&B, c)) have_heal = 1;
    uint32_t code = 0; int want = 0;
    if (!have_heal) { code = hp; want = 1; }
    else {
        for (int c = 0; c < 4 && !want; c++) if (B.target[c] == 2 && col_gap(&B, c) && (!B.cell[c][0] || B.code[c][0] == mp)) { code = mp; want = 2; }
        if (!want) for (int c = 0; c < 4; c++) if (B.target[c] == 1 && col_gap(&B, c) && (!B.cell[c][0] || B.code[c][0] == hp)) { code = hp; want = 1; break; }
        if (!want && vendor) for (int c = 0; c < 4; c++) if (B.target[c] == 3 && col_gap(&B, c) && (!B.cell[c][0] || B.code[c][0] == rv)) { code = rv; want = 3; break; }
    }
    if (orphan) { uint64_t o = dm_diag_srv_item(orphan); if (o && *(uint32_t *)(uintptr_t)(o + 0xc) == 3) return; orphan = 0; }   // 上一瓶没进腰带就先不生成。没生成成功也不摘腰带里还在的药 | If the last bottle did not enter the belt, do not spawn another. A bottle still in the belt is not detached when the spawn did not succeed
    int col = want ? predict(&B, code) : -1; if (col < 0 || B.target[col] != want) return;   // 找位落进的列不是这种药，就不生成 | Do not spawn when the finder would land in a column of another kind
    uint32_t left = 0; int rc = spawn_ok(b, G, pl, code, col, &left);
    if (rc == 1) {   // 0x16 已经把新瓶放进腰带并通知了客户端。接着用同一次补药决定摘错档：替换瓶必须已经在这一列里，belt_remove 再通知客户端
        // The 0x16 has placed the new bottle in the belt and told the client. The same refill decision then detaches a wrong-tier bottle only when its replacement is already in that column; belt_remove tells the client
        belt N; if (belt_read(b, pl, &N)) {
            dm_belt_op op; memset(&op, 0, sizeof op); op.rows = N.rows;
            for (int c = 0; c < 4; c++) { op.kind[c] = B.target[c]; for (int r = 0; r < N.rows && r < 4; r++) { op.code[c][r] = N.code[c][r]; op.present[c][r] = N.cell[c][r] != 0; } }
            if (dm_belt_refill_decide(&op, diff)) for (int c = 0; c < 4; c++) for (int r = op.rows - 1; r >= 0; r--) if (op.removed[c][r] && N.cell[c][r]) {
                if (logged < 40) LOG("腰带补药：列 %d 摘掉错档的 %.4s（同一次操作通知客户端，共 %d 格）", c + 1, (const char *)&N.code[c][r], op.notified); logged++;
                belt_remove(b, G, pl, N.cell[c][r]); } }
        if (logged < 40) LOG("腰带补药：补了一瓶 %.4s 到列 %d", (const char *)&code, col + 1); logged++; return; }
    if (left) orphan = left;   // 只停在刚生成、还在地上的那一瓶。地上本来就有的红蓝紫（过滤藏着、不捡）不算 | Only pause on the bottle just spawned that is still on the ground. Potions already on the ground (hidden by the filter, not picked up) do not count
    pause = 125; if (logged < 40) LOG("【红】腰带补药：%.4s 没有进列 %d，暂停补药（地上那瓶不再连着生成）", (const char *)&code, col + 1); logged++;
}

// ---------- 放不下时整理一次再捡 ----------
static uint8_t g_pk[17]; static uint32_t g_hold_id, g_tried[16]; static int g_phase, g_wait, g_tw;
int dm_qol_sort_busy(void) { return g_phase != 0; }
int dm_qol_sort_tried(uint32_t id) { if (!id) return 1; for (int i = 0; i < 16; i++) if (g_tried[i] == id) return 1; return 0; }
void dm_qol_sort_arm(const uint8_t pk[17], uint32_t id) { if (g_phase || !id) return; memcpy(g_pk, pk, 17); g_hold_id = id; g_phase = 1; }
void dm_qol_sort_pump(uint8_t *b) {
    if (!b) return;
    uint64_t pl = ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c)); if (!pl) { g_phase = 0; if (g_tw) { memset(g_tried, 0, sizeof g_tried); g_tw = 0; } return; }   // 离局清掉重试过的物品号，下一局同号的掉落还整理得了一次 | Leaving the game clears tried ids, so a drop with the same id next game still gets one sort retry
    gold_keep(b, pl);   // 客户端这份：界面上的金币数 | client copy: the gold number on screen
    if (!g_phase) return;
    if (g_phase == 1) { dm_inv_sort(0); g_phase = 2; g_wait = 20; return; }   // 整理是异步的：这一帧只请求，等约 20 帧再重发 | Sort is async: only request it this frame, resend after about 20 frames
    if (--g_wait > 0) return;
    ((void (*)(const uint8_t *, int))(b + 0x2ceab0))(g_pk, 17); g_tried[g_tw++ & 15] = g_hold_id; g_phase = 0;
    LOG("整理：背包放不下，整理后重试拾取物品 %u", g_hold_id);
}

// ---------- 打开商人：卖杂物，然后修理 ----------
static int in_ids(const uint32_t *a, int n, uint32_t id) { for (int i = 0; i < n; i++) if (a[i] == id) return 1; return 0; }
static uint64_t wfind(uint64_t w, const char *name, int dep) {
    if (!w || dep > 14) return 0; const char *nm = *(const char **)(uintptr_t)(w + 8); if (nm && !strcmp(nm, name)) return w;
    uint64_t ch = rd(w + 0x58), n = rd(w + 0x60); if (!ch || n > 256) return 0;
    for (uint64_t i = 0; i < n; i++) { uint64_t r = wfind(rd(ch + 8 * i), name, dep + 1); if (r) return r; } return 0;
}
static int repair_vendor(uint8_t *b) {   // 「全部修理」可见 = 这家商人会修（阿卡拉买东西但不修） | "Repair All" visible = this vendor repairs (Akara buys but does not repair)
    uint64_t root = rd((uint64_t)(uintptr_t)b + 0x2f05458); if (!root) return 0;
    uint64_t ch = rd(root + 0x58), n = rd(root + 0x60), vp = 0; if (!ch || n > 256) return 0;
    for (uint64_t i = 0; i < n && !vp; i++) { uint64_t p = rd(ch + 8 * i); const char *nm = p ? *(const char **)(uintptr_t)(p + 8) : 0; if (nm && !strcmp(nm, "VendorPanelLayout")) vp = p; }
    if (!vp || !*(uint8_t *)(uintptr_t)(vp + 0x50)) return 0; uint64_t btn = wfind(vp, "button_repair_all", 0); return btn && *(uint8_t *)(uintptr_t)(btn + 0x50);
}
static int mode_write(uint8_t *b, uint32_t v) {   // 修理光标那一格：商人消息处理 0x408eb0 写 3、0x40a8ec 写 0。不点「全部修理」，免得焦点留在按钮上 | The repair-cursor property: vendor message handler 0x408eb0 writes 3, 0x40a8ec writes 0. Do not tap Repair All, or focus stays on the button
    uint64_t t = rd((uint64_t)(uintptr_t)b + 0x2f05450); if (!t) return 0;   // 表不在就当进不了修理态，不要去读 0x8 | No table means repair mode is unavailable; do not read address 0x8
    uint64_t bk = rd(t + 8), n0 = rd(t); if (!bk || !n0) return 0;
    for (uint64_t n = rd(bk + ((n0 - 1) & 0x0cfa4cd5c8419dedull) * 8); n; n = rd(n)) if (*(uint64_t *)(uintptr_t)(n + 8) == MODE_KEY) {
        uint64_t obj = rd(n + 0x10), vp = obj ? rd(obj + 0x10) : 0; if (!vp) return 0; *(uint32_t *)(uintptr_t)vp = v; return 1; }
    return 0;
}
static int junk(uint8_t *b, uint64_t it) {
    uint64_t d = rd(it + 0x10); if (!d || *(uint32_t *)(uintptr_t)(it + 0xc) != 0 || *(uint8_t *)(uintptr_t)(d + 0x55) != 0) return 0;
    uint32_t q = *(uint32_t *)(uintptr_t)d, fl = *(uint32_t *)(uintptr_t)(d + 0x18); if (q < 1 || q > 4) return 0;
    if (fl & (F_SOCK | F_ETH)) return 0;
    int (*is)(uint64_t, int) = (int (*)(uint64_t, int))(b + 0x574bc0); if (!(is(it, T_WEAP) || is(it, T_ARMO))) return 0;
    if (((int (*)(uint64_t, int, int))(b + 0x5b9290))(it, 194, 0) > 0) return 0;
    if (dm_item_quest(b, it)) return 0;
    return ((int (*)(uint64_t))(b + 0x576c60))(it) != 0;
}
static uint64_t client_merc(uint8_t *b, uint64_t pl) {
    uint32_t mid = 0xffffffffu;
    for (uint64_t e = rd((uint64_t)(uintptr_t)b + 0x2d33528), k = 0; e && k < 256; e = rd(e + 0x30), k++) if (*(uint32_t *)(uintptr_t)(e + 4) == 7 && *(uint32_t *)(uintptr_t)(e + 0xc) == *(uint32_t *)(uintptr_t)(pl + 8) && !*(uint32_t *)(uintptr_t)(e + 0x20)) { mid = *(uint32_t *)(uintptr_t)(e + 8); break; }
    if (mid == 0xffffffffu) return 0;
    uint64_t u = rd((uint64_t)(uintptr_t)b + 0x2d603a0 + 0x400 + (mid & 0x7f) * 8);
    for (int k = 0; u && k < 4096; k++) { if (*(uint32_t *)(uintptr_t)(u + 8) == mid) return u; u = rd(u + 0x158); } return 0;
}
static uint64_t merc_inv(uint64_t m) {   // 没雇佣兵时 m 是 0。rd 只挡地址 0，m+0xa0 会去读 0xa0，打开商人就崩 | When there is no mercenary m is 0. rd only guards address 0, so m+0xa0 reads 0xa0 and opening a vendor crashes
    if (!m) return 0;
    uint64_t a0 = rd(m + 0xa0), iv = a0 && *(int32_t *)(uintptr_t)(a0 + 8) && rd(m + 0x98) ? rd(m + 0x98) : rd(m + 0x90);
    return iv && *(uint32_t *)(uintptr_t)iv == 0x01020304 ? iv : 0; }

void dm_qol_vendor_pump(uint8_t *b) {
    static int open_was, phase, cool, tries, nskip, nrep, we_mode, logged;
    static uint32_t cur, skip[40], rep[24];   // 背包 10×4，卖掉的和卖失败的都记在 skip 里；32 装不下，后面的失败件会每帧重试、修理永远轮不到 | Inventory is 10×4. Successes and failures both go into skip; 32 is too small, so a later failure retries forever and repair never runs
    if (!b) return;
    { uint64_t gp = ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c)); gold_keep(b, gp); }   // 修理之前补上客户端金币。购买在这一帧更早，靠上一帧已经补过 | top up client gold before repair. A purchase earlier this frame uses the top-up from the previous frame
    int shop = dm_shop_open(), gamble = *(uint32_t *)(b + 0x2d5eca8) != 0, trade = shop && !gamble;
    if (!trade) { if (we_mode) { mode_write(b, 0); we_mode = 0; } if (phase && logged < 20) LOG("自动卖修：商人关了，停下"); phase = 0; open_was = 0; return; }
    if (!open_was) { open_was = 1; phase = 1; cool = tries = nskip = nrep = 0; cur = 0; if (logged < 20) LOG("自动卖修：商人开了，先卖掉背包里的杂物"); logged++; }
    if (dm_vendor_repair_mode() && !we_mode) return;   // 玩家自己进了修理态：别卖（这一态下卖出会变成修） | The player entered repair mode: do not sell (a sell in this mode repairs instead)
    if (cool > 0) { cool--; return; }
    uint64_t pl = ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c)); if (!pl) return;
    uint64_t inv = rd(pl + 0x90); if (!inv || *(uint32_t *)(uintptr_t)inv != 0x01020304) return;
    if (rd(inv + 0x40)) return;
    if (phase == 1) {
        uint64_t it = 0; if (cur) { uint64_t f = ((uint64_t (*)(uint64_t, uint32_t, uint32_t, uint32_t))(b + 0x45f5f0))((uint64_t)(uintptr_t)(b + 0x2d603a0 + 4 * 0x400), cur & 0x7f, cur, 4); if (f && junk(b, f)) it = f; else cur = 0; }
        if (!it) for (uint64_t q = rd(inv + 0x10), k = 0; q && k < 512 && !it; k++) { uint64_t d = rd(q + 0x10); if (!d) break; uint32_t id = *(uint32_t *)(uintptr_t)(q + 8); if (!in_ids(skip, nskip, id) && junk(b, q)) { it = q; cur = id; } q = rd(d + 0xb0); }
        if (!it) { phase = repair_vendor(b) ? 2 : 0; tries = 0; cur = 0; if (phase == 2) { if (!mode_write(b, 3) || !dm_vendor_repair_mode()) { mode_write(b, 0); phase = 0; LOG("【红】自动修理：进不了修理态，不修"); } else we_mode = 1; } else if (logged < 20) LOG("自动卖修：杂物处理完（这家不修理）"); return; }   // 写了 3 却没读回修理态，要写回 0，不然光标停在修理、后面的卖被当成玩家自己进的修理态 | If 3 was written but repair mode does not read back, write 0 back; otherwise the cursor stays on repair and later sells look like the player entered repair mode
        int r = ((int (*)(uint64_t, uint64_t, int, int, int, int))(b + 0x433e50))(pl, it, 1, 0, 1, 0); cool = 32;
        if (r == 1) { if (nskip < 40) skip[nskip++] = cur; if (logged < 40) { uint32_t cd = ((uint32_t (*)(uint64_t))(b + 0x570260))(it); LOG("自动卖修：卖掉 %.4s（id %u）", (const char *)&cd, cur); } cur = 0; tries = 0; logged++; }
        else if (++tries >= 2) { if (nskip < 40) skip[nskip++] = cur; cur = 0; tries = 0; }
        return;
    }
    if (phase != 2 || !we_mode) return;
    if (!dm_vendor_repair_mode() && (!mode_write(b, 3) || !dm_vendor_repair_mode())) { mode_write(b, 0); we_mode = 0; phase = 0; LOG("【红】自动修理：修理态丢了，停下"); return; }
    uint64_t merc = client_merc(b, pl), it = 0;   // 先看有没有佣兵。没有就只修玩家，不能先调用 merc_inv | Look up the mercenary first. With none, repair only the player; do not call merc_inv first
    for (int o = 0; o < 2 && !it; o++) { uint64_t own = o ? merc : pl; if (!own) continue; uint64_t iv = o ? merc_inv(own) : inv; if (!iv) continue;
        for (uint64_t q = rd(iv + 0x10), k = 0; q && k < 64 && !it; k++) { uint64_t d = rd(q + 0x10); if (!d) break; uint32_t id = *(uint32_t *)(uintptr_t)(q + 8);
            if (*(uint32_t *)(uintptr_t)(q + 0xc) == 1 && !in_ids(rep, nrep, id) && (((int (*)(uint64_t))(b + 0x435b00))(q) & 1) && !(((int (*)(uint64_t))(b + 0x474990))(q) & 0xff)) it = q; q = rd(d + 0xb0); } }
    if (!it) { mode_write(b, 0); we_mode = 0; phase = 0; if (logged < 20) LOG("自动修理：身上和佣兵都修过了"); logged++; return; }
    uint32_t id = *(uint32_t *)(uintptr_t)(it + 8);
    int r = ((int (*)(uint64_t, uint64_t, int, int, int, int))(b + 0x433e50))(pl, it, 1, 0, 1, 0);   // 必须还在修理态。第 5 参的免确认只覆盖买和卖，修理仍会问 | Must still be in repair mode. The no-confirm 5th argument covers only buy and sell; repair still asks
    if (r == 1 && *(uint32_t *)(b + 0x2d5eb44) == 3) ((void (*)(uint32_t))(b + 0x43a7e0))(0);   // 原版「是否修理」点「是」的回调（0x43a7e0 在状态 3 时收掉提示并成交） | The original "repair?" Yes callback (0x43a7e0, while state is 3, dismisses the prompt and completes the deal)
    cool = 32;
    if (r == 1) { if (nrep < 24) rep[nrep++] = id; tries = 0; if (logged < 40) { uint32_t cd = ((uint32_t (*)(uint64_t))(b + 0x570260))(it); LOG("自动修理：修 %.4s（id %u），已确认", (const char *)&cd, id); } logged++; }
    else if (++tries >= 2) { if (nrep < 24) rep[nrep++] = id; tries = 0; }
}
