// 地上金币自动拾取，并给掉落过滤、增益自动补、放不下时整理重试借一个每帧入口（dm_potpick.c）。
// 不再捡地上的药水：红 / 蓝 / 紫由 dm_qol.c 按难度补进腰带。金币走原版捡物品包 0x16，不标成「药水自动拾取」
// （那个标记会让服务端按「进不了腰带就丢包」处理，金币会被丢掉）。
// 范围跟以前捡药一样，玩家周围 PICK_R 格。手上拿着东西、死了、被过滤器藏着的不捡。每 10 帧最多一包。
// 同一堆发过一次：约 2 秒后还在地上（多半是金币到顶了）就记进 8 个的名单，不再发。
// 开关 pot_pick 不再管这里。它改成「腰带按难度补药」（dm_qol.c；关掉时 dm_beltauto.c 才从背包补）。
// Ground gold auto-pickup, and a per-frame entry borrowed by the loot filter, buff recast, and the sort-and-retry after a pickup that does not fit (dm_potpick.c).
// Potions on the ground are no longer picked up: healing / mana / rejuvenation are refilled into the belt by difficulty in dm_qol.c. Gold uses the original pickup packet 0x16 and is not marked as potion auto-pickup
// (that mark makes the server drop the packet when the belt cannot take it, which would drop the gold).
// The range is the same as the old potion pickup, PICK_R tiles around the player. Nothing is picked up while something is held, while dead, or while the filter hides it. At most one packet per 10 frames.
// A pile is sent once: if it is still on the ground after about 2 seconds (usually the gold cap), it goes on a list of 8 and is not sent again.
// The pot_pick switch no longer applies here. It now means "refill the belt at this difficulty's tier" (dm_qol.c; with it off, dm_beltauto.c refills from the inventory).
#include "hle.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint64_t dm_guest_frame(void);
#define PICK_R 3
#define GOLD 0x20646c67u   // 物品码 gld | item code gld

// diag.c 的收包门还在问「这个 0x16 是不是自动拾取发的」。现在没有人登记，门不会拦金币和玩家自己点的物品。
// The packet gate in diag.c still asks "was this 0x16 sent by auto-pickup". Nothing records into it now, so the gate does not block gold or a packet the player tapped.
static _Atomic uint64_t g_auto[4];
int dm_potpick_auto(uint32_t id) { uint32_t fr = (uint32_t)dm_guest_frame(); for (int i = 0; i < 4; i++) { uint64_t v = atomic_load(&g_auto[i]); if (v && (uint32_t)(v >> 32) == id && fr - (uint32_t)v < 300) return 1; } return 0; }

void dm_potpick_step(uint8_t *b) {
    if (b) { void dm_lootfilter_pump(uint8_t *, uint64_t); void dm_lootfilter_test_dump(uint8_t *); dm_lootfilter_pump(b, ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c))); dm_lootfilter_test_dump(b); }
    if (b) { void dm_autobuff_pump(uint8_t *, uint64_t); void dm_autobuff_test_drop(uint64_t); uint64_t pl = ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c)); dm_autobuff_test_drop(pl); dm_autobuff_pump(b, pl); }
    if (!b) return;
    { void dm_qol_sort_pump(uint8_t *); dm_qol_sort_pump(b); }   // 放不下的那件：整理完再重发一次（dm_qol.c） | the one that did not fit: resend once after sorting (dm_qol.c)
    static uint64_t next; uint64_t fr = dm_guest_frame(); if (fr < next) return;
    uint64_t pl = ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c)); if (!pl) return;
    uint32_t pm = *(uint32_t *)(pl + 0xc); if (pm == 0 || pm == 17) return;
    uint64_t inv = *(uint64_t *)(pl + 0x90); if (!inv || *(uint32_t *)inv != 0x01020304 || *(uint64_t *)(inv + 0x40)) return;
    uint64_t pp = *(uint64_t *)(pl + 0x38); if (!pp) return; int px = *(uint16_t *)(pp + 2), py = *(uint16_t *)(pp + 6);
    static uint32_t sent[8], sfr[8], skip[8]; static int si, ki;
    uint32_t bid = 0; int bx = 0, by = 0, bd = PICK_R + 1;
    for (int k = 0; k < 128; k++) for (uint64_t it = *(uint64_t *)(b + 0x2d603a0 + 4 * 0x400 + k * 8); it; it = *(uint64_t *)(it + 0x158)) {
        if (*(uint32_t *)(it + 0xc) != 3) continue;
        uint64_t ip = *(uint64_t *)(it + 0x38); if (!ip) continue; int x = *(int32_t *)(ip + 0x10), y = *(int32_t *)(ip + 0x14);
        int dx = abs(x - px), dy = abs(y - py); if (dx > PICK_R || dy > PICK_R) continue;
        { int dm_lootfilter_hidden(uint8_t *, uint64_t); if (dm_lootfilter_hidden(b, it)) continue; }
        if (((uint32_t (*)(uint64_t))(b + 0x570260))(it) != GOLD) continue;
        uint32_t id = *(uint32_t *)(it + 8); int block = 0;
        for (int i = 0; i < 8; i++) if (skip[i] == id) block = 1;
        for (int i = 0; i < 8 && !block; i++) if (sent[i] == id) {
            if (fr - sfr[i] >= 120) { skip[ki++ & 7] = id; sent[i] = 0; LOG("金币自动拾取：物品 %u 还在地上，不再捡", id); }
            block = 1; }
        if (block) continue;
        int d = dx > dy ? dx : dy; if (d < bd) { bd = d; bid = id; bx = x; by = y; }
    }
    if (!bid) return;
    uint8_t pk[17] = { 0x16 }; uint32_t one = 1; memcpy(pk + 1, &bid, 4); memcpy(pk + 5, &bx, 4); memcpy(pk + 9, &by, 4); memcpy(pk + 13, &one, 4);
    ((void (*)(const uint8_t *, int))(b + 0x2ceab0))(pk, 17);
    sent[si & 7] = bid; sfr[si & 7] = (uint32_t)fr; si++; next = fr + 10;
    LOG("金币自动拾取：物品 %u 在 %d,%d（玩家 %d,%d）", bid, bx, by, px, py);
}
