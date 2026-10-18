# 本 fork 的改动

上游是 [jhaizhou-ops/d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile)。本仓库是 [XGBCCC/d2r-mobile](https://github.com/XGBCCC/d2r-mobile)。

这些改动只在本 fork 上。同步上游时，下面列出的文件最容易冲突，合并时保留本文件里的行为。

## 当前基线

- 对齐到上游 `main` 的 `9393b00`（iOS audio mix，#77）。这一提交在 v1.0.18（`120fe55`，4 GB 设备能进游戏）之后。
- 旧基线 `17845634`（Mac shader retarget，#60）和 v1.0.17（个人仓库与 iCloud 合并热修）都还在历史上。同步时保留极乐掉率，以及极乐局里的锁血锁蓝、耐久不掉、花费不扣金币、卖杂物、走近捡金币。不要把已经拿掉的腰带补药、自动喝药、商人自动修理、把金币写成上限、背包放不下先整理再捡再加回来。

## 以后怎么同步上游

```bash
git fetch upstream
git merge upstream/main
```

冲突时以本文件的「改了什么」为准，其余文件跟上上游。不要用 `git push --force`。

本机如果重新生成过着色器链（`dm/gfx/nvn/c1_full.tsv`、`dm/gfx/nvn/dmsh_chain.h`、`dm/gfx/tools/dmsh_chain.lock`），那个指纹只在本机缓存里，不要提交。`dm/ios/local.env` 已被忽略，里面是本机密钥，不要提交。

## 改了什么

### 极乐世界掉率

只影响勾选了极乐世界的角色，进局时改内存里的表，退局还原。已有角色下次进局生效。非极乐世界不变。

| 项 | 倍率 |
|---|---|
| 暗金 | ×8 |
| 套装 | ×5 |
| 符文 | ×12（只加直接引用符文组的表；女伯爵整表仍只清「不掉落」，符文档次不摊平，最高仍是 Ist） |
| 不掉落 | 仍 ÷10，非 0 至少留 1 |
| Boss | 仍永远按首次击败掉落 |

文件：`dm/host/dm_elysium.c`、`dm/host/dm_elysium.h`，文案键 `dmBlissDesc`。

### 极乐局里的锁血、耐久、金币

只在极乐世界这一局里。实现在 `dm/host/dm_qol.c`，服务端拍（`diag.c`）和客户端每帧（`dm_potpick.c`）各走一次。非极乐角色不写这些数。

- **生命和法力。** 当前值保持在当前上限。腰带空了也不会掉。死亡（人物模式 0 或 17）不写。
- **耐久。** 身上穿着的装备，当前耐久变少就写回变少之前的数。不会坏的（上限是 0）不动。不再打开商人自动修理。
- **金币。** 身上和箱内的合计变少时，写回变少之前的数，花费当作 0。某一项已经在上限上时，写回上限减 1 并记住这个数，绝不写成上限本身。上限还不知道（等级不在 1–99）时不写身上金币，也不把扣过之后的数记成新的底。转入箱子、从箱子取出、捡到金币（合计没少）照常记新数。
- **卖杂物。** 极乐局里打开交易商人（赌博页不算）时，背包里的白装、低质量、超强、魔法装备逐件卖掉。不卖暗金、套装、稀有、工艺、带孔、无形和任务物品，也不自动修理。长按空格整理还在。
- **捡金币。** 极乐局里走近地上的金币堆会捡。不捡地上的药。
- **拿掉的。** 腰带按难度补药、90% 自动喝药、佣兵喂药、打开商人自动修理、把金币写成上限、背包放不下先整理再捡、地上藏红蓝紫。打开背包不再从背包往腰带搬。双击腰带空格仍走原来的补药。

### 赌博

只在极乐世界这一局里：普通、噩梦、地狱的每个赌博商人，暗金是 10000/100000（10%），套装是 20000/100000（20%）。稀有不变。哈洛加斯里秘境加成已经更高时不往下压。离开极乐或换局时，写回第一次改表之前记住的原数。货架上原来就有的底材都留，不因为没有低等级暗金拿掉，所以匕首、法杖这类也在。各职业专属底材，赌博表里原来没有的也补上。不把药水、任务物品整表加进货架。品质掷骰跟别的底材一样。非极乐角色用游戏自己的概率和货架。

文件：`dm/host/dm_rift.c`、`dm/host/dm_rift.h`、`dm/host/dm_rift_ui.c`。

普通、噩梦也用轻量掉落过滤，地狱仍是重过滤。地上的药不藏。金币、钥匙、暗金、套装、稀有、符文、宝石、护身符、珠宝、任务和材料仍然显示。低号符文（Lem 以下）和全部宝石仍自动进百宝囊。暗金、套装、稀有、护身符仍手动捡。

### 涉及的文件

- 新增：`dm/host/dm_qol.c`、本文件
- 挂钩：`dm/host/Makefile`、`dm/host/diag.c`、`dm/host/dm_actions.c`、`dm/host/dm_beltauto.c`、`dm/host/dm_lootfilter.c`、`dm/host/dm_potpick.c`、`dm/host/dm_world.c`
- 掉率：`dm/host/dm_elysium.c`、`dm/host/dm_elysium.h`
- 文案：`dm/i18n/ui_text.tsv`、`dm/host/dm_i18n_tab.h`（`dmBlissDesc`、`dmCustomAutoMana`、`dmCustomMercPot`、`dmCustomPotPick`、`dmCustomLootFilter`）
- 说明：`README.md`、`README.zh-CN.md`、`dm/host/README.md`、`dm/host/README.zh-CN.md`
- 单测：`dm/host/tests/t_elysium_min.c`、`dm/host/tests/t_gamble.c`、`dm/host/tests/t_potions.c`

手机上的安装包要整包重新编译才会带上这些改动。源码提交本身不安装到手机。
