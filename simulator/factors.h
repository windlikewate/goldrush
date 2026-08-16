// factors.h — 因子评价计算
#pragma once
#include "game_api.h"

// ─── 因子值 ───
struct FactorValues {
    double gold_gain;       // F1: 模拟捡到的金子数
    double bomb_risk;       // F2: 踩炸弹次数(负向)
    double exploration;     // F3: 6步覆盖的不同格子数
    double enemy_proximity; // F4: 与最近敌人的平均距离变化
    double npc_encounter;   // F5: 靠近NPC的次数
    double path_efficiency; // F6: 有效移动步数/6
    double positional_adv;  // F7: 结束位置到中心9x9区域的距离(越近越好)
    double center_proximity;// F8: 结束位置到地图中心(8,8)的距离(越近越好)
};

// ─── 单角色6步模拟结果 ───
struct UnitSimulation {
    Position end_pos;       // 6步结束位置
    int gold_collected;     // 捡到金子(65%向上取整)
    int bombs_hit;          // 踩炸弹
    int unique_cells;       // 访问过的不同格子数
    Position start_pos;     // 起始位置
    int actual_moves;       // 实际移动步数(排除不动)
};

// 模拟单角色执行6步(只读,不改状态)
UnitSimulation simulateUnit(const GameInput* input, Position start,
                            const int actions[S], int start_idx, int num_steps);

// 计算所有因子
FactorValues computeFactors(const GameInput* input,
                            const int actions[S], int k);

// 综合评分
double scoreFactors(const FactorValues& f, const double weights[8]);

// ─── 内置权重(基于贪心策略IC回测) ───
static const double DEFAULT_WEIGHTS[8] = {
    1.0,   // F1 淘金(目标变量)
    -0.08, // F2 避雷(IC=0.08, 弱)
    0.19,  // F3 探索(IC=0.19)
    -0.08, // F4 敌人距离(IC=-0.08, 反向)
    0.08,  // F5 NPC(IC=0.08)
    0.08,  // F6 路径效率(IC=0.08)
    0.08,  // F7 位置优势(IC=0.08)
    0.55,  // F8 中心靠近(IC=0.55, 强!)
};
