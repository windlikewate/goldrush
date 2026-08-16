// simulator.h — GoldRush 2.0 本地模拟器
// 用途: 生成地图 + 执行动作 + 统计结果
// 规则假设见 README / 代码注释，均可修正

#pragma once
#include "game_api.h"
#include <random>
#include <vector>
#include <string>

// ─── 颜色/ANSI ───
namespace Color {
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* RED     = "\033[31m";
    constexpr const char* GREEN   = "\033[32m";
    constexpr const char* YELLOW  = "\033[33m";
    constexpr const char* BLUE    = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN    = "\033[36m";
    constexpr const char* WHITE   = "\033[37m";
    constexpr const char* BOLD    = "\033[1m";
    constexpr const char* DIM     = "\033[2m";
}

// ─── 地图生成参数 ───
struct MapConfig {
    int    seed = 42;              // 随机种子
    double bomb_density = 0.05;    // 炸弹占空地比例
    double obstacle_density = 0.08;// 障碍占空地比例
    int    center_gold_initial = 15; // 中心9x9初始金子数
    int    outer_gold_initial = 10;  // 外围初始金子数
    int    center_gold_per_round = 5; // 中心9x9每轮生成金子数
    int    outer_gold_interval = 3;  // 外围生成间隔(每N轮)
    int    outer_gold_count = 3;     // 外围每次生成金子数
    int    gold_max_value = 5;    // 单个格子最大金子数
    int    gold_min_value = 1;    // 单个格子最小金子数
    int    npc_count = 7;         // NPC 数量(规则:7)
    double fog_radius = 2;        // 初始迷雾半径(方形, 半径2=5x5视野)
    int    vp_cost_7x7 = 1;       // 购买7x7视野费用(金/回合)
    int    vp_cost_9x9 = 2;       // 购买9x9视野费用(金/回合)
    int    vision_7x7_radius = 3; // 7x7视野 = 半径3 (2*3+1=7)
    int    vision_9x9_radius = 4; // 9x9视野 = 半径4 (2*4+1=9)
};

// ─── 游戏内部状态(不暴露给选手) ───
struct InternalState {
    // 全图真实状态(无迷雾)
    int true_grid[GRID_SIZE][GRID_SIZE];

    // 金子生成时间表
    struct GoldSpawn {
        int row, col, value, round;
    };

    // NPC 真实位置和ID
    struct Npc {
        int id;
        Position pos;
        int move_dir; // 当前移动方向 0-3
    };

    std::vector<Npc> npcs;

    // 对手状态
    Position enemy_pos[2];
    int enemy_gold[2];

    // 统计
    int total_gold_spawned = 0;
    int total_gold_collected_p1 = 0;
    int total_gold_collected_p2 = 0;
    int total_gold_collected_enemy = 0;
    int bombs_triggered_p1 = 0;
    int bombs_triggered_p2 = 0;
    int total_vision_cost = 0;     // 视野购买总花费
};

// ─── 选手可见状态(即GameInput的真实来源) ───
struct VisibleState {
    int fog_grid[GRID_SIZE][GRID_SIZE];  // 带迷雾的网格
    Position my_units[2];
    int my_units_gold[2];
    Position visible_enemies[2];
    int num_visible_enemies = 0;
    NpcInfo visible_npcs[MAX_NPCS];
    int num_visible_npcs = 0;
    int snapshot_valid = 0;
    Snapshot snapshot;
};

// ─── 单步执行结果 ───
struct StepResult {
    bool hit_obstacle = false;
    bool hit_bomb = false;
    int gold_collected = 0;
    bool collision = false;  // 与另一角色碰撞
    Position from;
    Position to;
};

// ─── 回合结果 ───
struct RoundResult {
    int round;
    int p1_gold_this_round;  // 角色0捡到的金
    int p2_gold_this_round;  // 角色1捡到的金
    int p1_bombs;
    int p2_bombs;
    int p1_cum_gold;         // 角色0累计金
    int p2_cum_gold;         // 角色1累计金
    int p1_pos_r, p1_pos_c;  // 角色0回合结束位置
    int p2_pos_r, p2_pos_c;  // 角色1回合结束位置
    int enemy_cum_gold;      // 对手累计金
    int gold_on_map;         // 地上剩余金
    int k;                   // 分割点
    int order;               // 执行顺序
    int vp;                  // 视野购买
    int vision_cost;         // 本轮视野费用
    int cum_vision_cost;     // 累计视野费用
    int snapshot_valid;      // 是否有快照
    int snapshot_window_begin;
    int snapshot_window_end;
    int snapshot_r1_gold_remaining, snapshot_r1_occupants;
    int snapshot_r2_gold_remaining, snapshot_r2_occupants;
    int snapshot_r3_gold_remaining, snapshot_r3_occupants;
    int snapshot_r4_gold_remaining, snapshot_r4_occupants;
    int snapshot_r5_gold_remaining, snapshot_r5_occupants;
    StepResult steps[2][S];  // [角色][步]
};

// ─── 完整回放 ───
struct Replay {
    int seed;
    int total_rounds;
    int p1_final_gold;
    int p2_final_gold;
    int enemy_final_gold;
    std::vector<RoundResult> rounds;
};

// ─── 地图生成器 ───
class MapGenerator {
public:
    MapGenerator(const MapConfig& cfg);

    // 生成全图真实网格
    void generate(int true_grid[GRID_SIZE][GRID_SIZE]);

    // 生成 NPC
    std::vector<InternalState::Npc> generateNPCs();

    // 生成对手初始位置
    Position enemyStartPos(int unit_index);

private:
    MapConfig cfg_;
    std::mt19937 rng_;

    // 连通性修复(BFS)
    void fixConnectivity(int grid[GRID_SIZE][GRID_SIZE]) const;
};

// ─── 游戏模拟器 ───
class GameSimulator {
public:
    GameSimulator(const MapConfig& cfg);

    // 重置到初始状态
    void reset();

    // 执行一轮决策
    // actions: 6步动作序列
    // k: 分割点 (角色0走前k步, 角色1走后6-k步)
    // order: 0=角色0先, 1=角色1先
    RoundResult executeRound(const int actions[S], int k, int order, int vp = 0);

    // 单步执行(用于逐步演示)
    RoundResult executeOneStep(int action, int k, int order);

    // 获取当前选手可见状态
    VisibleState getVisibleState(int round) const;

    // 构造 GameInput (直接对接 moveDecision)
    GameInput buildGameInput(int round) const;

    // 获取真实网格(调试用)
    const int* getTrueGrid() const { return &internal_.true_grid[0][0]; }

    // 统计
    int playerGold(int unit) const { return visible_.my_units_gold[unit]; }
    int enemyGold() const;
    int totalGoldOnMap() const;

    // 完整自动对战(选手策略 vs 对手策略)
    using ActionFunc = GameOutput(*)(const GameInput*);
    Replay runFullGame(ActionFunc playerStrategy, int total_rounds = 500);

    // 可视化
    void printGrid() const;                    // 打印真实网格
    void printFogGrid() const;                 // 打印迷雾网格
    void printStats() const;                   // 打印统计
    void printReplaySummary(const Replay& r) const;

    // 导出回放为CSV
    void replayToCSV(const Replay& r, const std::string& filename) const;

private:
    MapConfig cfg_;
    MapGenerator map_gen_;
    InternalState internal_;
    VisibleState visible_;
    int current_round_ = 0;

    // 内部工具
    bool isValidPos(int row, int col) const;
    bool isWalkable(int row, int col) const;
    Position applyAction(Position from, int action) const;
    int manhattan(Position a, Position b) const;
    void updateFog(int vision_radius);
    void spawnGold(int round);
    void moveNPCs();
    void simulateEnemy();
    int getRegionId(int row, int col) const;

    // 对手简单AI
    struct {
        Position pos[2];
        int gold[2];
    } enemy_;
};
