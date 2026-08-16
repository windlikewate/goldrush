# GoldRush 2.0 模拟器使用指南

## 目录结构

```
game/
├── src/                    # 题目代码
│   ├── game_api.h          # C++ 接口定义
│   └── solutions/          # 示例策略
├── simulator/              # 模拟器 + 回测系统
│   ├── simulator.h/cpp     # 游戏模拟
│   ├── factors.h/cpp       # 因子评价
│   ├── backtest.h/cpp      # 回测分析
│   └── main_sim.cpp        # 入口
├── docs/                   # 文档
│   ├── SIMULATOR.md        # 本文档
│   └── strategy.md         # 因子策略文档
└── data/                   # 输出数据
    └── *.csv               # 回放 + 分析报告
```

---

## 编译

```bash
cd simulator/
make          # 编译模拟器
make clean    # 清理
```

---

## 运行模式

### 1. 单场对战

```bash
./sim                          # 随机策略, 500轮
./sim --seed 42 --rounds 100   # 自定义种子和轮数
```

输出：回放摘要 + CSV

### 2. 策略对比

```bash
./sim --compare --seed 42 --rounds 500
```

同时跑 3 种策略并对比：

| 策略 | 说明 |
|------|------|
| 随机策略 | 每步随机 0~4 |
| 贪心策略 | 向最近的金子移动 |
| 多因子策略 | 200个候选评分选最优 |

输出：各策略金币对比 + 各自 CSV

### 3. 交互式可视化演示

```bash
./sim --demo
```

**会提示选择策略**：
```
选择策略: 1=贪心策略, 2=随机策略, 3=多因子策略
>
```

逐步显示每轮 6 步的执行过程，按 Enter 继续。

### 4. 批量回测

```bash
./sim --batch --n-seeds 50 --rounds 500
```

对 50 个不同地图跑 3 种策略，输出平均金币、胜率、P90 延迟。

### 5. 因子分析

```bash
./sim --analyze data/replay_seed42_r500.csv
```

计算 CSV 中每轮数据的因子 IC（秩相关系数），输出分析报告 + `*_factor_report.csv`。

### 6. 策略 CSV 对比

```bash
./sim --compare-csv data/replay_贪心_seed42.csv data/replay_多因子_seed42.csv
```

对比两个策略的因子 IC 差异。

---

## 命令行参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--seed N` | 42 | 随机种子 |
| `--rounds N` | 500 | 回合数 |
| `--compare` | off | 策略对比 |
| `--batch` | off | 批量回测 |
| `--n-seeds N` | 50 | 批量场景数 |
| `--demo` | off | 可视化演示 |
| `--analyze FILE` | - | 因子分析 |
| `--compare-csv A B` | - | 策略CSV对比 |

---

## 地图可视化

```
   0 1 2 3 4 5 6 7 8 910111213141516
 0 E  2                       💣 U0   ← U0在(0,16), E在(0,0)
 1    💣             ▓             5
 ...
 8        4   4   N                 ← N=7个NPC在中心
 ...
16 U1 ▓   ▓   ▓                   E ← U1在(16,0), E在(16,16)
```

| 符号 | 含义 | 颜色 |
|------|------|------|
| `U0` / `U1` | 我方角色 | 绿色 |
| `E` | 敌方角色 | 红色 |
| `N` | NPC | 粉色 |
| `💣` | 炸弹 | 红色 |
| `▓` | 障碍 | 灰色 |
| 数字 | 金子数量 | 黄色 |

---

## 游戏规则（模拟器实现）

| 规则 | 值 |
|------|-----|
| 地图大小 | 17×17 |
| 角色起始位置 | 对角线两端 (0,16) 和 (16,0) |
| 每轮步数 | 6 步（两个角色分配） |
| 视野 | 5×5 方形（半径2） |
| 金子生成 | 中心9×9每轮生成5个，外围每3轮生成3个 |
| 捡金比例 | 65%（向上取整） |
| 炸弹伤害 | 当前金币 10%（向上取整） |
| NPC | 7个，出生於中心(8,8)，每轮最多3步 |
| 快照 | 每 5 轮发布 |

---

## CSV 输出

所有输出文件在 `data/` 目录下：

| 命令 | 输出 |
|------|------|
| `./sim` | `data/replay_seed{N}_r{M}.csv` |
| `./sim --compare` | `data/replay_{策略名}_seed{N}.csv` |
| `./sim --analyze` | `data/*_factor_report.csv` |

### CSV 列定义

| 列名 | 说明 |
|------|------|
| `round` | 回合数 |
| `p1_gold_this`, `p2_gold_this` | 本回合角色捡金 |
| `p1_cum`, `p2_cum`, `total_cum` | 累计金币 |
| `p1_pos_r/c`, `p2_pos_r/c` | 角色位置 |
| `p1_bombs`, `p2_bombs` | 踩炸弹数 |
| `enemy_cum`, `gold_on_map` | 对手金/地上金 |
| `k`, `order`, `vp` | 决策参数 |
| `p1_steps`, `p2_steps` | 各角色步数 |
| `p1_collisions`, `p2_collisions` | 碰撞次数 |
| `vision_cost`, `cum_vision_cost` | 视野费用 |
| `snap_valid`, `snap_win_begin/end` | 快照信息 |
| `snap_r1~r5_gold`, `snap_r1~r5_occ` | 5个区域金子/人数 |

---

## 因子评价

8 个因子用于评价候选动作：

| 编号 | 因子 | 含义 | IC |
|------|------|------|-----|
| F1 | 淘金收益 | 模拟捡金数 | 1.0 |
| F2 | 避雷风险 | 踩炸弹数 | 0.08 |
| F3 | 探索覆盖 | 不同格子数 | 0.19 |
| F4 | 敌人距离 | 距敌人变化 | -0.08 |
| F5 | NPC遭遇 | 附近NPC数 | 0.08 |
| F6 | 路径效率 | 有效移动/6 | 0.08 |
| F7 | 碰撞次数 | 碰撞数 | 0.08 |
| F8 | 中心靠近 | 到中心距离 | **0.55** |

---

## 添加自定义策略

在 `main_sim.cpp` 中定义新策略：

```cpp
extern "C" GameOutput myStrategy(const GameInput* input) {
    GameOutput out = {};
    out.k = 3;
    out.order = 0;
    out.vp = 0;
    for (int i = 0; i < S; i++) {
        out.actions[i] = /* 你的动作 */;
    }
    return out;
}
```

然后在对比中添加：

```cpp
testStrategy("我的策略", myStrategy, seed);
```

在 demo 中添加：

```cpp
else if (choice == '4') { strategy = myStrategy; sname = "我的策略"; }
```
