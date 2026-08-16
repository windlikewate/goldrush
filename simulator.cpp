// simulator.cpp — GoldRush 2.0 模拟器实现
#include "simulator.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <cmath>

// ─── MapGenerator ───

MapGenerator::MapGenerator(const MapConfig& cfg)
    : cfg_(cfg), rng_(cfg.seed) {}

// 判断是否在中心9x9区域 (rows 4-12, cols 4-12)
static bool isCenter(int r, int c) {
    return r >= 4 && r <= 12 && c >= 4 && c <= 12;
}

void MapGenerator::generate(int grid[GRID_SIZE][GRID_SIZE]) {
    std::uniform_int_distribution<int> gold_val(cfg_.gold_min_value, cfg_.gold_max_value);

    // 1. 全部初始化为空地
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            grid[r][c] = 0;

    // 2. 收集中心9x9和外围格子
    std::vector<std::pair<int,int>> center_cells;
    std::vector<std::pair<int,int>> outer_cells;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++) {
            if (isCenter(r, c))
                center_cells.push_back({r, c});
            else
                outer_cells.push_back({r, c});
        }

    // 中心9x9放置障碍和炸弹
    std::shuffle(center_cells.begin(), center_cells.end(), rng_);
    int n_center = (int)center_cells.size();
    int n_obs_center = (int)(n_center * cfg_.obstacle_density);
    int n_bomb_center = (int)(n_center * cfg_.bomb_density);
    int idx = 0;
    for (int i = 0; i < n_obs_center && idx < n_center; i++, idx++)
        grid[center_cells[idx].first][center_cells[idx].second] = -1;
    for (int i = 0; i < n_bomb_center && idx < n_center; i++, idx++)
        grid[center_cells[idx].first][center_cells[idx].second] = -3;

    // 外围放置障碍和炸弹
    std::shuffle(outer_cells.begin(), outer_cells.end(), rng_);
    int n_outer = (int)outer_cells.size();
    int n_obs_outer = (int)(n_outer * cfg_.obstacle_density);
    int n_bomb_outer = (int)(n_outer * cfg_.bomb_density);
    idx = 0;
    for (int i = 0; i < n_obs_outer && idx < n_outer; i++, idx++)
        grid[outer_cells[idx].first][outer_cells[idx].second] = -1;
    for (int i = 0; i < n_bomb_outer && idx < n_outer; i++, idx++)
        grid[outer_cells[idx].first][outer_cells[idx].second] = -3;

    // 3. 中心9x9放置初始金子
    center_cells.clear();
    for (int r = 4; r <= 12; r++)
        for (int c = 4; c <= 12; c++)
            if (grid[r][c] == 0)
                center_cells.push_back({r, c});
    std::shuffle(center_cells.begin(), center_cells.end(), rng_);

    for (int i = 0; i < cfg_.center_gold_initial && i < (int)center_cells.size(); i++) {
        int val = gold_val(rng_);
        grid[center_cells[i].first][center_cells[i].second] = val;
    }

    // 4. 外围放置初始金子
    outer_cells.clear();
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            if (!isCenter(r, c) && grid[r][c] == 0)
                outer_cells.push_back({r, c});
    std::shuffle(outer_cells.begin(), outer_cells.end(), rng_);

    for (int i = 0; i < cfg_.outer_gold_initial && i < (int)outer_cells.size(); i++) {
        int val = gold_val(rng_);
        grid[outer_cells[i].first][outer_cells[i].second] = val;
    }

    // 5. 连通性检测(BFS)：确保所有非障碍格子连通
    fixConnectivity(grid);
}

// BFS 检测从起点出发能到达的所有非障碍格子
static int bfsReachable(const int grid[GRID_SIZE][GRID_SIZE], Position start) {
    bool visited[GRID_SIZE][GRID_SIZE] = {};
    int count = 0;

    static const int dr[] = {-1, 1, 0, 0};
    static const int dc[] = {0, 0, -1, 1};

    std::vector<Position> queue;
    queue.push_back(start);
    visited[start.row][start.col] = true;
    count++;

    size_t head = 0;
    while (head < queue.size()) {
        Position cur = queue[head++];
        for (int d = 0; d < 4; d++) {
            int nr = cur.row + dr[d];
            int nc = cur.col + dc[d];
            if (nr >= 0 && nr < GRID_SIZE && nc >= 0 && nc < GRID_SIZE
                && !visited[nr][nc] && grid[nr][nc] != -1) {
                visited[nr][nc] = true;
                queue.push_back({nr, nc});
                count++;
            }
        }
    }
    return count;
}

// 修复连通性：移除隔离障碍
void MapGenerator::fixConnectivity(int grid[GRID_SIZE][GRID_SIZE]) const {
    Position start{0, 16};  // 玩家角色0起始位置

    // BFS 统计可达格子数
    int reachable = bfsReachable(grid, start);
    int total_walkable = 0;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            if (grid[r][c] != -1) total_walkable++;

    if (reachable == total_walkable) return;  // 已完全连通

    // 否则：逐个移除孤立障碍
    std::vector<std::pair<int,int>> obstacle_cells;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            if (grid[r][c] == -1)
                obstacle_cells.push_back({r, c});

    // 打乱移除顺序（用局部rng，因为fixConnectivity是const）
    std::mt19937 local_rng(cfg_.seed + 9999);
    std::shuffle(obstacle_cells.begin(), obstacle_cells.end(), local_rng);

    for (const auto& [r, c] : obstacle_cells) {
        grid[r][c] = 0;  // 移除障碍
        if (bfsReachable(grid, start) == total_walkable) break;
    }
}

std::vector<InternalState::Npc> MapGenerator::generateNPCs() {
    std::vector<InternalState::Npc> npcs;
    std::uniform_int_distribution<int> pos(0, GRID_SIZE - 1);
    std::uniform_int_distribution<int> dir(0, 3);

    // 随机生成，不生成在障碍/炸弹上
    for (int i = 0; i < cfg_.npc_count; ) {
        int r = pos(rng_), c = pos(rng_);
        if (r == 0 && c == 16) continue;  // 不和角色0起始位置重叠
        if (r == 16 && c == 0) continue;  // 不和角色1起始位置重叠

        npcs.push_back({
            .id = i + 1,
            .pos = {r, c},
            .move_dir = dir(rng_),
        });
        i++;
    }
    return npcs;
}

Position MapGenerator::enemyStartPos(int unit_index) {
    std::uniform_int_distribution<int> edge(0, 3);
    std::uniform_int_distribution<int> pos(0, GRID_SIZE - 1);
    // 简单策略：从四边生成
    switch (edge(rng_)) {
        case 0: return {0, (int)pos(rng_)};          // 上边
        case 1: return {GRID_SIZE - 1, (int)pos(rng_)}; // 下边
        case 2: return {(int)pos(rng_), 0};            // 左边
        default: return {(int)pos(rng_), GRID_SIZE - 1}; // 右边
    }
}

// ─── GameSimulator ───

GameSimulator::GameSimulator(const MapConfig& cfg)
    : cfg_(cfg), map_gen_(cfg) {}

void GameSimulator::reset() {
    current_round_ = 0;
    internal_.total_gold_spawned = 0;
    internal_.total_gold_collected_p1 = 0;
    internal_.total_gold_collected_p2 = 0;
    internal_.total_gold_collected_enemy = 0;
    internal_.bombs_triggered_p1 = 0;
    internal_.bombs_triggered_p2 = 0;

    // 生成地图
    map_gen_.generate(internal_.true_grid);

    // 生成 NPC
    internal_.npcs = map_gen_.generateNPCs();

    // 玩家初始位置(四角对角)
    visible_.my_units[0] = {0, 16};    // 右上角
    visible_.my_units[1] = {16, 0};    // 左下角
    visible_.my_units_gold[0] = 0;
    visible_.my_units_gold[1] = 0;

    // 对手初始位置(四角对角)
    enemy_.pos[0] = {0, 0};            // 左上角
    enemy_.pos[1] = {16, 16};          // 右下角
    enemy_.gold[0] = 0;
    enemy_.gold[1] = 0;

    visible_.num_visible_enemies = 0;
    visible_.visible_enemies[0] = {-1, -1};
    visible_.visible_enemies[1] = {-1, -1};

    visible_.num_visible_npcs = 0;
    visible_.snapshot_valid = 0;
    visible_.snapshot.window_begin = -1;
    visible_.snapshot.window_end = -1;

    updateFog(cfg_.fog_radius);
}

bool GameSimulator::isValidPos(int row, int col) const {
    return row >= 0 && row < GRID_SIZE && col >= 0 && col < GRID_SIZE;
}

bool GameSimulator::isWalkable(int row, int col) const {
    if (!isValidPos(row, col)) return false;
    int tile = internal_.true_grid[row][col];
    return tile != -1;  // -1 = 障碍，不能走
}

Position GameSimulator::applyAction(Position from, int action) const {
    int dr = 0, dc = 0;
    switch (action) {
        case 0: dr = -1; break;  // 上
        case 1: dr =  1; break;  // 下
        case 2: dc = -1; break;  // 左
        case 3: dc =  1; break;  // 右
        case 4: return from;      // 不动
    }
    int nr = from.row + dr;
    int nc = from.col + dc;
    if (isWalkable(nr, nc)) return {nr, nc};
    return from;  // 越界或障碍→不动
}

int GameSimulator::manhattan(Position a, Position b) const {
    return std::abs(a.row - b.row) + std::abs(a.col - b.col);
}

int GameSimulator::getRegionId(int row, int col) const {
    // 将 17x17 分为 5 个区域 (简化: 按象限 + 中心)
    if (row >= 6 && row <= 10 && col >= 6 && col <= 10) return 3;  // 中心
    if (row < 8 && col < 8) return 1;   // 左上
    if (row < 8 && col >= 8) return 2;  // 右上
    if (row >= 8 && col < 8) return 4;  // 左下
    return 5;                            // 右下
}

// ─── 迷雾更新 ───
void GameSimulator::updateFog(int vision_radius) {
    // 初始化为全雾
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            visible_.fog_grid[r][c] = -5;

    // 以每个角色为中心，vision_radius 为半径揭开（方形视野）
    auto reveal = [&](Position center, int radius) {
        for (int r = std::max(0, center.row - radius);
             r <= std::min(GRID_SIZE - 1, center.row + radius); r++) {
            for (int c = std::max(0, center.col - radius);
                 c <= std::min(GRID_SIZE - 1, center.col + radius); c++) {
                visible_.fog_grid[r][c] = internal_.true_grid[r][c];
            }
        }
    };

    reveal(visible_.my_units[0], vision_radius);
    reveal(visible_.my_units[1], vision_radius);

    // 检测可见敌人
    visible_.num_visible_enemies = 0;
    auto checkEnemyVisibility = [&](int idx) {
        Position ep = enemy_.pos[idx];
        if (visible_.fog_grid[ep.row][ep.col] != -5) {
            visible_.visible_enemies[visible_.num_visible_enemies] = ep;
            visible_.num_visible_enemies++;
        }
    };
    checkEnemyVisibility(0);
    checkEnemyVisibility(1);
    // 填充空位
    for (int i = visible_.num_visible_enemies; i < 2; i++)
        visible_.visible_enemies[i] = {-1, -1};

    // 检测可见 NPC
    visible_.num_visible_npcs = 0;
    for (const auto& npc : internal_.npcs) {
        if (visible_.fog_grid[npc.pos.row][npc.pos.col] != -5 &&
            visible_.num_visible_npcs < MAX_NPCS) {
            visible_.visible_npcs[visible_.num_visible_npcs] = {
                .id = npc.id,
                .pos = npc.pos,
            };
            visible_.num_visible_npcs++;
        }
    }
}

// ─── 金子生成 ───
void GameSimulator::spawnGold(int round) {
    std::mt19937 spawn_rng(round * 1000 + cfg_.seed);
    std::uniform_int_distribution<int> center_r(4, 12);
    std::uniform_int_distribution<int> center_c(4, 12);
    std::uniform_int_distribution<int> val(cfg_.gold_min_value, cfg_.gold_max_value);

    // 1. 中心9x9每轮生成金子
    for (int i = 0; i < cfg_.center_gold_per_round; i++) {
        int attempts = 0;
        while (attempts++ < 50) {
            int rr = center_r(spawn_rng), cc = center_c(spawn_rng);
            if (internal_.true_grid[rr][cc] == 0) {
                internal_.true_grid[rr][cc] = val(spawn_rng);
                internal_.total_gold_spawned++;
                break;
            }
        }
    }

    // 2. 外围每N轮生成金子
    if (round > 0 && round % cfg_.outer_gold_interval == 0) {
        std::uniform_int_distribution<int> outer_pos(0, GRID_SIZE - 1);
        for (int i = 0; i < cfg_.outer_gold_count; i++) {
            int attempts = 0;
            while (attempts++ < 50) {
                int rr = outer_pos(spawn_rng), cc = outer_pos(spawn_rng);
                // 只在中心9x9外生成
                if (!isCenter(rr, cc) && internal_.true_grid[rr][cc] == 0) {
                    internal_.true_grid[rr][cc] = val(spawn_rng);
                    internal_.total_gold_spawned++;
                    break;
                }
            }
        }
    }
}

// ─── NPC 移动 ───
void GameSimulator::moveNPCs() {
    std::mt19937 npc_rng(current_round_ * 100 + cfg_.seed);
    std::uniform_int_distribution<int> dir(0, 3);
    static const int dr[] = {-1, 1, 0, 0};  // 上下左右
    static const int dc[] = {0, 0, -1, 1};

    for (auto& npc : internal_.npcs) {
        // 70% 概率沿当前方向, 30% 随机转向
        if (npc_rng() % 10 < 3) {
            npc.move_dir = dir(npc_rng);
        }
        int nr = npc.pos.row + dr[npc.move_dir];
        int nc = npc.pos.col + dc[npc.move_dir];
        if (isWalkable(nr, nc)) {
            npc.pos = {nr, nc};
        } else {
            npc.move_dir = dir(npc_rng);  // 碰壁就转向
        }
    }
}

// ─── 对手简单AI(贪心向最近的可见金子) ───
void GameSimulator::simulateEnemy() {
    // 这里简化：对手每轮随机走6步(后续可替换为更好的策略)
    std::mt19937 enemy_rng(current_round_ * 200 + cfg_.seed);
    std::uniform_int_distribution<int> act(0, 4);

    // 简化：对手每个角色走3步随机
    for (int u = 0; u < 2; u++) {
        Position p = enemy_.pos[u];
        for (int s = 0; s < 3; s++) {
            int a = act(enemy_rng);
            p = applyAction(p, a);

            // 捡金
            int& tile = internal_.true_grid[p.row][p.col];
            if (tile >= 1) {
                enemy_.gold[u] += tile;
                internal_.total_gold_collected_enemy += tile;
                tile = 0;
            }
            // 踩炸弹
            if (tile == -3) {
                enemy_.gold[u] = std::max(0, enemy_.gold[u] - 2);
                tile = 0;
            }
        }
        enemy_.pos[u] = p;
    }
}

// ─── 执行一轮 ───
RoundResult GameSimulator::executeRound(const int actions[S], int k, int order, int vp) {
    RoundResult result = {};
    result.round = current_round_;
    result.k = k;
    result.order = order;
    result.vp = vp;

    // 金子生成(轮开始时)
    spawnGold(current_round_);

    // 确定两个角色的动作序列和起始位置
    Position unit_pos[2] = {visible_.my_units[0], visible_.my_units[1]};
    int unit_steps[2] = {k, S - k};  // 角色0走k步, 角色1走6-k步

    // 确定执行顺序
    int exec_order[2] = {0, 1};
    if (order == 1) { exec_order[0] = 1; exec_order[1] = 0; }

    // 执行两个角色的动作
    for (int ei = 0; ei < 2; ei++) {
        int unit = exec_order[ei];
        int steps = unit_steps[unit];
        int start_idx = (unit == 0) ? 0 : k;

        for (int s = 0; s < steps; s++) {
            int action = actions[start_idx + s];
            StepResult& sr = result.steps[unit][s];
            sr.from = unit_pos[unit];

            Position next = applyAction(unit_pos[unit], action);

            // 碰撞检测：如果和另一个角色目标位置相同
            int other = 1 - unit;
            if (next.row == unit_pos[other].row && next.col == unit_pos[other].col) {
                sr.collision = true;
                next = unit_pos[unit];  // 不动
            }

            unit_pos[unit] = next;
            sr.to = next;

            // 检查踩障碍(不应该发生)
            if (!isWalkable(next.row, next.col)) {
                sr.hit_obstacle = true;
                unit_pos[unit] = sr.from;
                continue;
            }

            // 检查炸弹
            int& tile = internal_.true_grid[next.row][next.col];
            if (tile == -3) {
                sr.hit_bomb = true;
                visible_.my_units_gold[unit] = std::max(0, visible_.my_units_gold[unit] - 2);
                if (unit == 0) internal_.bombs_triggered_p1++;
                else internal_.bombs_triggered_p2++;
                tile = 0;  // 炸弹触发后消失
            }

            // 捡金
            if (tile >= 1) {
                sr.gold_collected = tile;
                visible_.my_units_gold[unit] += tile;
                if (unit == 0) internal_.total_gold_collected_p1 += tile;
                else internal_.total_gold_collected_p2 += tile;
                tile = 0;
            }
        }
    }

    // 更新位置
    visible_.my_units[0] = unit_pos[0];
    visible_.my_units[1] = unit_pos[1];

    // 结果统计
    for (int s = 0; s < unit_steps[0]; s++)
        result.p1_gold_this_round += result.steps[0][s].gold_collected;
    for (int s = 0; s < unit_steps[1]; s++)
        result.p2_gold_this_round += result.steps[1][s].gold_collected;
    for (int s = 0; s < unit_steps[0]; s++)
        if (result.steps[0][s].hit_bomb) result.p1_bombs++;
    for (int s = 0; s < unit_steps[1]; s++)
        if (result.steps[1][s].hit_bomb) result.p2_bombs++;

    // NPC 移动
    moveNPCs();

    // 对手移动
    simulateEnemy();

    // 视野购买: 计算半径和费用
    int vision_radius = cfg_.fog_radius;  // 默认基础视野
    int vision_cost = 0;
    if (vp == 1) {
        vision_radius = cfg_.vision_7x7_radius;
        vision_cost = cfg_.vp_cost_7x7;
    } else if (vp == 2) {
        vision_radius = cfg_.vision_9x9_radius;
        vision_cost = cfg_.vp_cost_9x9;
    }

    // 更新迷雾
    updateFog(vision_radius);

    // 扣除视野费用
    if (vision_cost > 0) {
        int total_gold = visible_.my_units_gold[0] + visible_.my_units_gold[1];
        if (total_gold >= vision_cost) {
            visible_.my_units_gold[0] = std::max(0, visible_.my_units_gold[0] - vision_cost);
        }
        internal_.total_vision_cost += vision_cost;
    }

    // 保存当前回合数（在递增之前）
    int round_for_snap = current_round_;
    current_round_++;

    // 填充累计统计
    result.p1_cum_gold = visible_.my_units_gold[0];
    result.p2_cum_gold = visible_.my_units_gold[1];
    result.p1_pos_r = visible_.my_units[0].row;
    result.p1_pos_c = visible_.my_units[0].col;
    result.p2_pos_r = visible_.my_units[1].row;
    result.p2_pos_c = visible_.my_units[1].col;
    result.enemy_cum_gold = enemy_.gold[0] + enemy_.gold[1];
    result.gold_on_map = totalGoldOnMap();
    result.vision_cost = vision_cost;
    result.cum_vision_cost = internal_.total_vision_cost;

    // 记录快照数据（与 buildGameInput 同步判断）
    bool has_snap = (round_for_snap % 10 == 0 && round_for_snap > 0);
    result.snapshot_valid = has_snap ? 1 : 0;
    if (has_snap) {
        result.snapshot_window_begin = round_for_snap - 10;
        result.snapshot_window_end = round_for_snap;
        // 统计5个区域
        for (int i = 0; i < REGION_COUNT; i++) {
            int gold_rem = 0, occ = 0;
            for (int r = 0; r < GRID_SIZE; r++)
                for (int c = 0; c < GRID_SIZE; c++) {
                    if (getRegionId(r, c) == i + 1) {
                        if (internal_.true_grid[r][c] >= 1) gold_rem += internal_.true_grid[r][c];
                    }
                }
            switch (i) {
                case 0: result.snapshot_r1_gold_remaining = gold_rem; result.snapshot_r1_occupants = occ; break;
                case 1: result.snapshot_r2_gold_remaining = gold_rem; result.snapshot_r2_occupants = occ; break;
                case 2: result.snapshot_r3_gold_remaining = gold_rem; result.snapshot_r3_occupants = occ; break;
                case 3: result.snapshot_r4_gold_remaining = gold_rem; result.snapshot_r4_occupants = occ; break;
                case 4: result.snapshot_r5_gold_remaining = gold_rem; result.snapshot_r5_occupants = occ; break;
            }
        }
    }

    return result;
}

// ─── 构建 GameInput ───
GameInput GameSimulator::buildGameInput(int round) const {
    GameInput input = {};
    input.round = round;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            input.grid[r][c] = visible_.fog_grid[r][c];

    input.my_units[0] = visible_.my_units[0];
    input.my_units[1] = visible_.my_units[1];
    input.my_units_gold[0] = visible_.my_units_gold[0];
    input.my_units_gold[1] = visible_.my_units_gold[1];
    input.gold_opp = enemy_.gold[0] + enemy_.gold[1];

    input.visible_enemies[0] = visible_.visible_enemies[0];
    input.visible_enemies[1] = visible_.visible_enemies[1];

    input.num_visible_npcs = visible_.num_visible_npcs;
    for (int i = 0; i < visible_.num_visible_npcs; i++) {
        input.visible_npcs[i] = visible_.visible_npcs[i];
    }

    // 快照判断: 每5轮一次（在buildGameInput中判断，确保策略调用时可见）
    bool has_snapshot = (round % 5 == 0 && round > 0);
    input.snapshot_valid = has_snapshot ? 1 : 0;
    if (has_snapshot) {
        input.snapshot.window_begin = round - 10;
        input.snapshot.window_end = round;
        for (int i = 0; i < REGION_COUNT; i++) {
            input.snapshot.regions[i].id = i + 1;
            input.snapshot.regions[i].gold_remaining = 0;
            input.snapshot.regions[i].occupants = 0;
            int expected_center = cfg_.center_gold_per_round * 10;
            int expected_outer = (10 / cfg_.outer_gold_interval) * cfg_.outer_gold_count;
            input.snapshot.regions[i].gold_generated = expected_center + expected_outer;
            input.snapshot.regions[i].enter = 0;
            input.snapshot.regions[i].leave = 0;
            input.snapshot.regions[i].gold_collected = 0;
            for (int r = 0; r < GRID_SIZE; r++)
                for (int c = 0; c < GRID_SIZE; c++) {
                    if (getRegionId(r, c) == i + 1) {
                        if (internal_.true_grid[r][c] >= 1)
                            input.snapshot.regions[i].gold_remaining += internal_.true_grid[r][c];
                    }
                }
        }
    } else {
        input.snapshot.window_begin = -1;
        input.snapshot.window_end = -1;
    }

    return input;
}

int GameSimulator::enemyGold() const {
    return enemy_.gold[0] + enemy_.gold[1];
}

int GameSimulator::totalGoldOnMap() const {
    int total = 0;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            if (internal_.true_grid[r][c] >= 1)
                total += internal_.true_grid[r][c];
    return total;
}

// ─── 完整对战 ───
Replay GameSimulator::runFullGame(ActionFunc playerStrategy, int total_rounds) {
    reset();
    Replay replay;
    replay.seed = cfg_.seed;
    replay.total_rounds = total_rounds;

    for (int r = 0; r < total_rounds; r++) {
        GameInput input = buildGameInput(r);
        GameOutput output = playerStrategy(&input);

        RoundResult rr = executeRound(output.actions, output.k, output.order, output.vp);
        replay.rounds.push_back(rr);
    }

    replay.p1_final_gold = visible_.my_units_gold[0];
    replay.p2_final_gold = visible_.my_units_gold[1];
    replay.enemy_final_gold = enemy_.gold[0] + enemy_.gold[1];

    return replay;
}

// ─── 可视化 ───

void GameSimulator::printGrid() const {
    printf("\n");
    printf("  ");
    for (int c = 0; c < GRID_SIZE; c++) printf("%2d", c);
    printf("\n");

    for (int r = 0; r < GRID_SIZE; r++) {
        printf("%2d", r);
        for (int c = 0; c < GRID_SIZE; c++) {
            int tile = internal_.true_grid[r][c];
            bool is_unit0 = (visible_.my_units[0].row == r && visible_.my_units[0].col == c);
            bool is_unit1 = (visible_.my_units[1].row == r && visible_.my_units[1].col == c);
            bool is_enemy0 = (enemy_.pos[0].row == r && enemy_.pos[0].col == c);
            bool is_enemy1 = (enemy_.pos[1].row == r && enemy_.pos[1].col == c);

            if (is_unit0) {
                printf("%s%s%2s%s", Color::BOLD, Color::GREEN, "U0", Color::RESET);
            } else if (is_unit1) {
                printf("%s%s%2s%s", Color::BOLD, Color::GREEN, "U1", Color::RESET);
            } else if (is_enemy0 || is_enemy1) {
                printf("%s%s%2s%s", Color::BOLD, Color::RED, "E", Color::RESET);
            } else if (tile == -5) {
                printf(" %s·%s", Color::DIM, Color::RESET);
            } else if (tile == -3) {
                printf(" %s💣%s", Color::RED, Color::RESET);
            } else if (tile == -1) {
                printf(" %s▓%s", Color::DIM, Color::RESET);
            } else if (tile == 0) {
                printf("  ");
            } else if (tile >= 1) {
                printf(" %s%2d%s", Color::YELLOW, tile, Color::RESET);
            }
        }
        printf("\n");
    }
    printf("\n");
}

void GameSimulator::printFogGrid() const {
    printf("\n");
    printf("  ");
    for (int c = 0; c < GRID_SIZE; c++) printf("%2d", c);
    printf("\n");

    for (int r = 0; r < GRID_SIZE; r++) {
        printf("%2d", r);
        for (int c = 0; c < GRID_SIZE; c++) {
            int tile = visible_.fog_grid[r][c];
            bool is_unit0 = (visible_.my_units[0].row == r && visible_.my_units[0].col == c);
            bool is_unit1 = (visible_.my_units[1].row == r && visible_.my_units[1].col == c);

            if (is_unit0) {
                printf("%s%s%2s%s", Color::BOLD, Color::GREEN, "U0", Color::RESET);
            } else if (is_unit1) {
                printf("%s%s%2s%s", Color::BOLD, Color::GREEN, "U1", Color::RESET);
            } else if (tile == -5) {
                printf(" %s██%s", Color::DIM, Color::RESET);
            } else if (tile == -3) {
                printf(" %s💣%s", Color::RED, Color::RESET);
            } else if (tile == -1) {
                printf(" %s▓%s", Color::DIM, Color::RESET);
            } else if (tile == 0) {
                printf("  ");
            } else if (tile >= 1) {
                printf(" %s%2d%s", Color::YELLOW, tile, Color::RESET);
            }
        }
        printf("\n");
    }
    printf("\n");
}

void GameSimulator::printStats() const {
    printf("\n%s═══ 游戏统计 ═══%s\n", Color::BOLD, Color::RESET);
    printf("  角色0 金币: %s%d%s\n", Color::GREEN, visible_.my_units_gold[0], Color::RESET);
    printf("  角色1 金币: %s%d%s\n", Color::GREEN, visible_.my_units_gold[1], Color::RESET);
    printf("  角色0 捡金: %s%d%s\n", Color::YELLOW, internal_.total_gold_collected_p1, Color::RESET);
    printf("  角色1 捡金: %s%d%s\n", Color::YELLOW, internal_.total_gold_collected_p2, Color::RESET);
    printf("  炸弹触发: U0=%d, U1=%d\n", internal_.bombs_triggered_p1, internal_.bombs_triggered_p2);
    printf("  对手 金币: %s%d%s\n", Color::RED, enemyGold(), Color::RESET);
    printf("  地上剩余: %s%d%s\n", Color::CYAN, totalGoldOnMap(), Color::RESET);
    printf("  总生成:   %d\n", internal_.total_gold_spawned);
    printf("  视野花费: %s%d%s\n", Color::CYAN, internal_.total_vision_cost, Color::RESET);
    printf("  当前回合: %d\n", current_round_);
    printf("  角色0 位置: (%d, %d)\n", visible_.my_units[0].row, visible_.my_units[0].col);
    printf("  角色1 位置: (%d, %d)\n", visible_.my_units[1].row, visible_.my_units[1].col);
    printf("%s═══════════════%s\n\n", Color::BOLD, Color::RESET);
}

void GameSimulator::printReplaySummary(const Replay& r) const {
    printf("\n%s══════ 回放摘要 ══════%s\n", Color::BOLD, Color::RESET);
    printf("  Seed: %d, 回合: %d\n", r.seed, r.total_rounds);
    printf("  角色0: %s%4d%s\n", Color::GREEN, r.p1_final_gold, Color::RESET);
    printf("  角色1: %s%4d%s\n", Color::GREEN, r.p2_final_gold, Color::RESET);
    printf("  总计:  %s%4d%s\n", Color::BOLD, r.p1_final_gold + r.p2_final_gold, Color::RESET);
    printf("  对手:  %s%4d%s\n", Color::RED, r.enemy_final_gold, Color::RESET);
    int diff = (r.p1_final_gold + r.p2_final_gold) - r.enemy_final_gold;
    printf("  差距:  %s%+d%s\n", diff >= 0 ? Color::GREEN : Color::RED, diff, Color::RESET);

    // 每回合金币变化
    // 每回合金币累计（简化）
    printf("\n  每回合金币累计:\n");
    printf("  %6s %8s %8s\n", "Round", "U0", "U1");
    int p1_cum = 0, p2_cum = 0;
    int step = std::max(1, (int)r.rounds.size() / 20);
    for (int i = 0; i < (int)r.rounds.size(); i += step) {
        p1_cum += r.rounds[i].p1_gold_this_round;
        p2_cum += r.rounds[i].p2_gold_this_round;
        printf("  %6d %8d %8d\n", r.rounds[i].round, p1_cum, p2_cum);
    }
    printf("%s══════════════════════%s\n\n", Color::BOLD, Color::RESET);
}

void GameSimulator::replayToCSV(const Replay& r, const std::string& filename) const {
    FILE* f = fopen(filename.c_str(), "w");
    if (!f) {
        printf("  %s无法写入文件: %s%s\n", Color::RED, filename.c_str(), Color::RESET);
        return;
    }

    // CSV 表头
    fprintf(f, "round,"
              "p1_gold_this,p2_gold_this,p1_cum,p2_cum,total_cum,"
              "p1_pos_r,p1_pos_c,p2_pos_r,p2_pos_c,"
              "p1_bombs,p2_bombs,"
              "enemy_cum,gold_on_map,"
              "k,order,vp,"
              "p1_steps,p2_steps,"
              "p1_collisions,p2_collisions,"
              "vision_cost,cum_vision_cost,"
              "snap_valid,snap_win_begin,snap_win_end,"
              "snap_r1_gold,snap_r1_occ,"
              "snap_r2_gold,snap_r2_occ,"
              "snap_r3_gold,snap_r3_occ,"
              "snap_r4_gold,snap_r4_occ,"
              "snap_r5_gold,snap_r5_occ\n");

    for (const auto& rd : r.rounds) {
        // 统计碰撞
        int p1_coll = 0, p2_coll = 0;
        for (int s = 0; s < S; s++) {
            if (rd.steps[0][s].collision) p1_coll++;
            if (rd.steps[1][s].collision) p2_coll++;
        }

        fprintf(f, "%d,"
                  "%d,%d,%d,%d,%d,"
                  "%d,%d,%d,%d,"
                  "%d,%d,"
                  "%d,%d,"
                  "%d,%d,%d,"
                  "%d,%d,"
                  "%d,%d,"
                  "%d,%d,"
                  "%d,%d,%d,"
                  "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            rd.round,
            rd.p1_gold_this_round, rd.p2_gold_this_round,
            rd.p1_cum_gold, rd.p2_cum_gold,
            rd.p1_cum_gold + rd.p2_cum_gold,
            rd.p1_pos_r, rd.p1_pos_c,
            rd.p2_pos_r, rd.p2_pos_c,
            rd.p1_bombs, rd.p2_bombs,
            rd.enemy_cum_gold, rd.gold_on_map,
            rd.k, rd.order, rd.vp,
            rd.k, (S - rd.k),
            p1_coll, p2_coll,
            rd.vision_cost, rd.cum_vision_cost,
            rd.snapshot_valid,
            rd.snapshot_window_begin, rd.snapshot_window_end,
            rd.snapshot_r1_gold_remaining, rd.snapshot_r1_occupants,
            rd.snapshot_r2_gold_remaining, rd.snapshot_r2_occupants,
            rd.snapshot_r3_gold_remaining, rd.snapshot_r3_occupants,
            rd.snapshot_r4_gold_remaining, rd.snapshot_r4_occupants,
            rd.snapshot_r5_gold_remaining, rd.snapshot_r5_occupants);
    }

    fclose(f);
    printf("  %sCSV已导出: %s%s\n", Color::CYAN, filename.c_str(), Color::RESET);
}
