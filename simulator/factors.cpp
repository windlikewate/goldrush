// factors.cpp — 多因子计算实现
#include "factors.h"
#include <cmath>
#include <algorithm>
#include <set>

// ─── 工具函数 ───
static int manhattan(Position a, Position b) {
    return std::abs(a.row - b.row) + std::abs(a.col - b.col);
}

static bool isValidPos(int r, int c) {
    return r >= 0 && r < GRID_SIZE && c >= 0 && c < GRID_SIZE;
}

static bool isWalkable(const GameInput* input, int r, int c) {
    if (!isValidPos(r, c)) return false;
    return input->grid[r][c] != -1;  // -1 = 障碍
}

// ─── 单角色模拟 ───
UnitSimulation simulateUnit(const GameInput* input, Position start,
                            const int actions[S], int start_idx, int num_steps) {
    UnitSimulation sim = {};
    sim.start_pos = start;
    sim.end_pos = start;
    sim.gold_collected = 0;
    sim.bombs_hit = 0;
    sim.actual_moves = 0;

    Position cur = start;
    std::set<std::pair<int,int>> visited;
    visited.insert({cur.row, cur.col});

    for (int i = 0; i < num_steps; i++) {
        int action = actions[start_idx + i];
        int dr = 0, dc = 0;
        switch (action) {
            case 0: dr = -1; break;  // 上
            case 1: dr =  1; break;  // 下
            case 2: dc = -1; break;  // 左
            case 3: dc =  1; break;  // 右
            case 4: continue;         // 不动
        }

        int nr = cur.row + dr;
        int nc = cur.col + dc;

        if (!isWalkable(input, nr, nc)) continue;  // 障碍/越界→不动

        cur = {nr, nc};
        sim.actual_moves++;
        visited.insert({cur.row, cur.col});

        // 捡金/炸弹
        int tile = input->grid[cur.row][cur.col];
        if (tile >= 1) {
            sim.gold_collected += tile;
        } else if (tile == -3) {
            sim.bombs_hit++;
        }
    }

    sim.end_pos = cur;
    sim.unique_cells = (int)visited.size();
    return sim;
}

// ─── 因子计算 ───
FactorValues computeFactors(const GameInput* input, const int actions[S], int k) {
    FactorValues f = {};

    // 模拟两个角色
    UnitSimulation sim0 = simulateUnit(input, input->my_units[0], actions, 0, k);
    UnitSimulation sim1 = simulateUnit(input, input->my_units[1], actions, k, S - k);

    // ─── F1: 淘金收益 ───
    f.gold_gain = sim0.gold_collected + sim1.gold_collected;

    // ─── F2: 避雷风险 ───
    f.bomb_risk = sim0.bombs_hit + sim1.bombs_hit;

    // ─── F3: 探索覆盖 ───
    f.exploration = sim0.unique_cells + sim1.unique_cells;

    // ─── F4: 敌人接近度 ───
    // 计算执行前后与最近敌人的距离变化(负向: 越近分越低)
    auto nearestEnemyDist = [&](Position p) -> int {
        int best = 1e9;
        for (int i = 0; i < 2; i++) {
            if (input->visible_enemies[i].row >= 0) {
                int d = manhattan(p, input->visible_enemies[i]);
                if (d < best) best = d;
            }
        }
        return best == 1e9 ? 100 : best;  // 无可见敌人→返回大值
    };

    int dist_before_0 = nearestEnemyDist(input->my_units[0]);
    int dist_before_1 = nearestEnemyDist(input->my_units[1]);
    int dist_after_0  = nearestEnemyDist(sim0.end_pos);
    int dist_after_1  = nearestEnemyDist(sim1.end_pos);

    // 距离增加→正向, 距离减少→负向
    f.enemy_proximity = (dist_after_0 - dist_before_0) + (dist_after_1 - dist_before_1);

    // ─── F5: NPC 遭遇 ───
    // 统计路径上靠近NPC的步数(距离<=2)
    auto isNearNPC = [&](Position p) -> int {
        int count = 0;
        for (int i = 0; i < input->num_visible_npcs; i++) {
            if (manhattan(p, input->visible_npcs[i].pos) <= 2) count++;
        }
        return count;
    };
    f.npc_encounter = isNearNPC(sim0.end_pos) + isNearNPC(sim1.end_pos);

    // ─── F6: 路径效率 ───
    f.path_efficiency = (double)(sim0.actual_moves + sim1.actual_moves) / S;

    // ─── F7: 位置优势 ───
    // 结束位置到中心9x9区域(4-12, 4-12)的距离, 越近越好(取反)
    auto distToCenter = [&](Position p) -> int {
        int r = std::clamp(p.row, 4, 12);
        int c = std::clamp(p.col, 4, 12);
        return manhattan(p, {r, c});
    };
    // 在中心区域内=0(最好), 越远越大
    int center_dist = distToCenter(sim0.end_pos) + distToCenter(sim1.end_pos);
    f.positional_adv = -center_dist;  // 负号: 越近分越高

    // ─── F8: 金币差距因子 ───
    int my_total = input->my_units_gold[0] + input->my_units_gold[1];
    int diff = my_total - input->gold_opp;  // 正=领先, 负=落后
    // 落后→正值(鼓励激进), 领先→负值(鼓励保守)
    f.deficit_factor = -diff / 10.0;  // 归一化

    // ─── F9: 中心靠近因子 ───
    // 结束位置到地图中心(8,8)的曼哈顿距离, 越近越好(取反)
    int center_dist9 = manhattan(sim0.end_pos, {8, 8}) + manhattan(sim1.end_pos, {8, 8});
    f.center_proximity = -center_dist9;

    return f;
}

// ─── 综合评分 ───
double scoreFactors(const FactorValues& f, const double weights[9]) {
    return weights[0] * f.gold_gain
         + weights[1] * f.bomb_risk
         + weights[2] * f.exploration
         + weights[3] * f.enemy_proximity
         + weights[4] * f.npc_encounter
         + weights[5] * f.path_efficiency
         + weights[6] * f.positional_adv
         + weights[7] * f.deficit_factor
         + weights[8] * f.center_proximity;
}
