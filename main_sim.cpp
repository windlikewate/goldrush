// main_sim.cpp — GoldRush 2.0 模拟器入口
// 编译: make sim
// 运行: ./sim              随机策略跑500轮
//       ./sim --rounds 100 跑100轮
//       ./sim --seed 123    用指定seed
//       ./sim --demo        逐步可视化演示

#include "simulator.h"
#include "factors.h"
#include "backtest.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <algorithm>

// ─── 内置策略(测试用) ───

// 随机策略(baseline)
extern "C" GameOutput randomStrategy(const GameInput* input) {
    GameOutput out = {};
    std::mt19937 gen(input->round * 100 + time(nullptr));
    std::uniform_int_distribution<int> act(0, 4);
    for (int i = 0; i < S; ++i)
        out.actions[i] = act(gen);
    out.k = 4;
    out.order = 0;
    out.vp = (input->snapshot_valid == 1) ? 1 : 0;
    return out;
}

// 贪心策略: 向最近的可见金子移动
extern "C" GameOutput greedyStrategy(const GameInput* input) {
    GameOutput out = {};
    out.k = 3;  // 角色0走3步，角色1走3步
    out.order = 0;
    out.vp = (input->snapshot_valid == 1) ? 1 : 0;

    // 找最近的有金格子
    auto findNearestGold = [&](Position from) -> std::pair<int,int> {
        int best_dist = 1e9;
        std::pair<int,int> best = {-1, -1};  // 找不到
        for (int r = 0; r < GRID_SIZE; r++)
            for (int c = 0; c < GRID_SIZE; c++) {
                if (input->grid[r][c] >= 1) {
                    int d = std::abs(r - from.row) + std::abs(c - from.col);
                    if (d < best_dist) {
                        best_dist = d;
                        best = {r, c};
                    }
                }
            }
        return best;
    };

    // 为角色0规划3步
    Position cur0 = input->my_units[0];
    auto target0 = findNearestGold(cur0);
    for (int i = 0; i < 3; i++) {
        if (target0.first >= 0) {
            if (cur0.row > target0.first)      { out.actions[i] = 0; cur0.row--; }
            else if (cur0.row < target0.first) { out.actions[i] = 1; cur0.row++; }
            else if (cur0.col > target0.second){ out.actions[i] = 2; cur0.col--; }
            else if (cur0.col < target0.second){ out.actions[i] = 3; cur0.col++; }
            else { out.actions[i] = 4; }
        } else {
            out.actions[i] = 4;
        }
    }

    // 为角色1规划3步
    Position cur1 = input->my_units[1];
    auto target1 = findNearestGold(cur1);
    for (int i = 3; i < 6; i++) {
        if (target1.first >= 0) {
            if (cur1.row > target1.first)      { out.actions[i] = 0; cur1.row--; }
            else if (cur1.row < target1.first) { out.actions[i] = 1; cur1.row++; }
            else if (cur1.col > target1.second){ out.actions[i] = 2; cur1.col--; }
            else if (cur1.col < target1.second){ out.actions[i] = 3; cur1.col++; }
            else { out.actions[i] = 4; }
        } else {
            out.actions[i] = 4;
        }
    }

    return out;
}

// 多因子策略: 枚举候选动作, 选Score最高的
extern "C" GameOutput factorStrategy(const GameInput* input) {
    GameOutput best_out = {};
    double best_score = -1e18;

    const int NUM_CANDIDATES = 200;
    std::mt19937 rng(input->round * 1000 + input->my_units[0].row * 100 + input->my_units[0].col);

    for (int c = 0; c < NUM_CANDIDATES; c++) {
        GameOutput cand = {};

        if (c == 0) {
            // 候选0: 贪心baseline
            cand.k = 3;
            cand.order = 0;
            cand.vp = (input->snapshot_valid == 1) ? 1 : 0;

            auto findGold = [&](Position from) -> std::pair<int,int> {
                int best = 1e9;
                std::pair<int,int> target = {-1, -1};
                for (int r = 0; r < GRID_SIZE; r++)
                    for (int col = 0; col < GRID_SIZE; col++)
                        if (input->grid[r][col] >= 1) {
                            int d = std::abs(r - from.row) + std::abs(col - from.col);
                            if (d < best) { best = d; target = {r, col}; }
                        }
                return target;
            };

            Position cur = input->my_units[0];
            auto t0 = findGold(cur);
            for (int i = 0; i < 3; i++) {
                if (t0.first >= 0) {
                    if (cur.row > t0.first)      { cand.actions[i] = 0; cur.row--; }
                    else if (cur.row < t0.first) { cand.actions[i] = 1; cur.row++; }
                    else if (cur.col > t0.second){ cand.actions[i] = 2; cur.col--; }
                    else if (cur.col < t0.second){ cand.actions[i] = 3; cur.col++; }
                    else cand.actions[i] = 4;
                } else cand.actions[i] = 4;
            }
            cur = input->my_units[1];
            auto t1 = findGold(cur);
            for (int i = 3; i < 6; i++) {
                if (t1.first >= 0) {
                    if (cur.row > t1.first)      { cand.actions[i] = 0; cur.row--; }
                    else if (cur.row < t1.first) { cand.actions[i] = 1; cur.row++; }
                    else if (cur.col > t1.second){ cand.actions[i] = 2; cur.col--; }
                    else if (cur.col < t1.second){ cand.actions[i] = 3; cur.col++; }
                    else cand.actions[i] = 4;
                } else cand.actions[i] = 4;
            }
        } else {
            std::uniform_int_distribution<int> act(0, 4);
            std::uniform_int_distribution<int> kk(0, 6);
            cand.k = kk(rng);
            cand.order = rng() % 2;
            cand.vp = rng() % 3;
            for (int i = 0; i < S; i++)
                cand.actions[i] = act(rng);
        }

        FactorValues fv = computeFactors(input, cand.actions, cand.k);
        double score = scoreFactors(fv, DEFAULT_WEIGHTS);

        if (score > best_score) {
            best_score = score;
            best_out = cand;
        }
    }

    return best_out;
}
// ─── 交互式演示 ───
void runDemo() {
    MapConfig cfg;
    cfg.seed = 42;
    cfg.center_gold_initial = 15;
    cfg.center_gold_per_round = 5;
    cfg.outer_gold_interval = 3;
    cfg.outer_gold_count = 3;
    cfg.bomb_density = 0.05;
    cfg.obstacle_density = 0.08;
    cfg.fog_radius = 2;

    GameSimulator sim(cfg);
    sim.reset();

    printf("\n%s╔════════════════════════════════════════╗%s\n", Color::BOLD, Color::RESET);
    printf("%s║     GoldRush 2.0 模拟器 - 逐步演示       ║%s\n", Color::BOLD, Color::RESET);
    printf("%s╚════════════════════════════════════════╝%s\n", Color::BOLD, Color::RESET);
    printf("\n%s[真实地图]%s vs %s[迷雾视图]%s\n\n", Color::GREEN, Color::RESET, Color::CYAN, Color::RESET);

    for (int round = 0; round < 20; round++) {
        printf("%s══════════ 回合 %d ══════════%s\n", Color::BOLD, round, Color::RESET);

        // 回合0先打印初始状态再执行
        if (round == 0) {
            printf("%s[初始状态 - 未执行任何动作]%s\n\n", Color::CYAN, Color::RESET);
            sim.printGrid();
            printf("\n");
            sim.printFogGrid();
            printf("按 Enter 开始第0轮执行...\n");
            getchar();
        }

        GameInput input = sim.buildGameInput(round);
        GameOutput output = greedyStrategy(&input);

        RoundResult result = sim.executeRound(output.actions, output.k, output.order, output.vp);

        // 打印真实地图和迷雾地图
        sim.printGrid();
        printf("\n");
        sim.printFogGrid();
        sim.printStats();

        // 本轮捡到的金
        printf("  本轮: U0捡+%d, U1捡+%d, 炸弹: U0=%d, U1=%d\n\n",
               result.p1_gold_this_round, result.p2_gold_this_round,
               result.p1_bombs, result.p2_bombs);

        printf("按 Enter 继续... (或 Ctrl+C 退出)\n");
        getchar();
    }
}

// ─── 批量回测(多seed) ───
void runBatch(int rounds, int n_seeds, const char* strategy_name,
              GameSimulator::ActionFunc strategy) {
    printf("\n%s═══ 批量回测: %s ═══%s\n", Color::BOLD, strategy_name, Color::RESET);
    printf("  场景数: %d, 每场景 %d 轮\n\n", n_seeds, rounds);

    int total_gold = 0, wins = 0;
    std::vector<int> gold_list;
    std::vector<double> latencies;

    for (int s = 0; s < n_seeds; s++) {
        MapConfig cfg;
        cfg.seed = s;
        cfg.bomb_density = 0.05;
        cfg.obstacle_density = 0.08;
        cfg.center_gold_initial = 15;
        cfg.center_gold_per_round = 5;
        cfg.outer_gold_initial = 10;
        cfg.outer_gold_interval = 3;
        cfg.outer_gold_count = 3;
        cfg.fog_radius = 2;

        GameSimulator sim(cfg);

        auto start = std::chrono::high_resolution_clock::now();
        Replay replay = sim.runFullGame(strategy, rounds);
        auto end = std::chrono::high_resolution_clock::now();

        double ms = (double)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        latencies.push_back(ms / rounds);

        int gold = replay.p1_final_gold + replay.p2_final_gold;
        total_gold += gold;
        gold_list.push_back(gold);
        if (gold > replay.enemy_final_gold) wins++;

        if ((s + 1) % 20 == 0) {
            printf("  场景 %3d/%d: 金币=%4d, 累计平均=%.1f\n",
                   s + 1, n_seeds, gold, (double)total_gold / (s + 1));
        }
    }

    // 统计
    std::sort(gold_list.begin(), gold_list.end());
    std::sort(latencies.begin(), latencies.end());
    int p90_idx = (int)(latencies.size() * 0.9);

    printf("\n  平均金币: %.1f\n", (double)total_gold / n_seeds);
    printf("  胜率: %.1f%% (%d/%d)\n", (double)wins / n_seeds * 100, wins, n_seeds);
    printf("  金币中位数: %d\n", gold_list[n_seeds / 2]);
    printf("  金币P10/P90: %d / %d\n", gold_list[n_seeds / 10], gold_list[n_seeds * 9 / 10]);
    double avg_latency = 0;
    for (double l : latencies) avg_latency += l;
    printf("  平均延迟: %.2f ms/轮\n", avg_latency / n_seeds);
    printf("  P90延迟: %.2f ms/轮\n", latencies[p90_idx]);
    printf("\n");
}

// ─── 完整对战 ───
void runFullGame(int rounds, int seed) {
    MapConfig cfg;
    cfg.seed = seed;
    cfg.bomb_density = 0.05;
    cfg.obstacle_density = 0.08;
    cfg.center_gold_initial = 15;
    cfg.center_gold_per_round = 5;
    cfg.outer_gold_interval = 3;
    cfg.outer_gold_count = 3;
    cfg.fog_radius = 2;

    GameSimulator sim(cfg);

    auto start = std::chrono::high_resolution_clock::now();
    Replay replay = sim.runFullGame(randomStrategy, rounds);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    sim.printReplaySummary(replay);

    printf("  耗时: %lld ms (%.2f ms/round)\n",
           (long long)duration.count(),
           (double)duration.count() / rounds);
    printf("  P50延迟: ~%.2f ms\n", (double)duration.count() / rounds);
    printf("  P90延迟: ~%.2f ms (估计)\n\n", (double)duration.count() / rounds * 1.5);

    // 导出 CSV
    char csv_name[128];
    snprintf(csv_name, sizeof(csv_name), "replay_seed%d_r%d.csv", seed, rounds);
    sim.replayToCSV(replay, csv_name);
}

// ─── 策略对比 ───
void runComparison(int rounds, int seed) {
    printf("\n%s═══ 策略对比 ═══%s\n\n", Color::BOLD, Color::RESET);

    struct StrategyResult {
        const char* name;
        int total_gold;
        int enemy_gold;
        double time_ms;
        Replay replay;
    };

    std::vector<StrategyResult> results;

    auto testStrategy = [&](const char* name, GameSimulator::ActionFunc strategy, int s) {
        MapConfig cfg;
        cfg.seed = s;
        cfg.bomb_density = 0.05;
        cfg.obstacle_density = 0.08;
        cfg.center_gold_initial = 15;
        cfg.center_gold_per_round = 5;
        cfg.outer_gold_interval = 3;
        cfg.outer_gold_count = 3;
        cfg.fog_radius = 2;

        GameSimulator sim(cfg);

        auto start = std::chrono::high_resolution_clock::now();
        Replay replay = sim.runFullGame(strategy, rounds);
        auto end = std::chrono::high_resolution_clock::now();

        results.push_back({
            name,
            replay.p1_final_gold + replay.p2_final_gold,
            replay.enemy_final_gold,
            (double)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(),
            std::move(replay),
        });

        printf("  %s%s%s: %s%d%s 金 (对手%s%d%s), 耗时%.0fms\n",
               Color::BOLD, name, Color::RESET,
               Color::GREEN, results.back().total_gold, Color::RESET,
               Color::RED, results.back().enemy_gold, Color::RESET,
               results.back().time_ms);
    };

    testStrategy("随机策略", randomStrategy, seed);
    testStrategy("贪心策略", greedyStrategy, seed);
    testStrategy("多因子策略", factorStrategy, seed);

    printf("\n%s═══════════════%s\n", Color::BOLD, Color::RESET);
    if (results.size() >= 2) {
        int diff = results[1].total_gold - results[0].total_gold;
        printf("  贪心 vs 随机: %s%+d%s 金\n",
               diff >= 0 ? Color::GREEN : Color::RED, diff, Color::RESET);
    }
    if (results.size() >= 3) {
        int diff = results[2].total_gold - results[1].total_gold;
        printf("  多因子 vs 贪心: %s%+d%s 金\n",
               diff >= 0 ? Color::GREEN : Color::RED, diff, Color::RESET);
    }

    // 导出每个策略的CSV
    printf("\n");
    for (const auto& r : results) {
        char csv_name[128];
        snprintf(csv_name, sizeof(csv_name), "replay_%s_seed%d.csv", r.name, seed);
        // 用临时simulator导出(配置和上面一致)
        MapConfig tmp_cfg;
        tmp_cfg.seed = seed;
        tmp_cfg.bomb_density = 0.05;
        tmp_cfg.obstacle_density = 0.08;
        tmp_cfg.center_gold_initial = 15;
        tmp_cfg.center_gold_per_round = 5;
        tmp_cfg.outer_gold_interval = 3;
        tmp_cfg.outer_gold_count = 3;
        tmp_cfg.fog_radius = 2;
        tmp_cfg.obstacle_density = 0.08;
        tmp_cfg.fog_radius = 2;
        GameSimulator tmp(tmp_cfg);
        tmp.replayToCSV(r.replay, csv_name);
    }
    printf("\n");
}

// ─── 主函数 ───
int main(int argc, char* argv[]) {
    int rounds = 500;
    int seed = 42;
    bool demo = false;
    bool compare = false;
    bool analyze = false;
    bool compareCsv = false;
    bool batch = false;
    const char* csvA = nullptr;
    const char* csvB = nullptr;
    int n_seeds = 50;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc)
            rounds = atoi(argv[++i]);
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = atoi(argv[++i]);
        else if (strcmp(argv[i], "--demo") == 0)
            demo = true;
        else if (strcmp(argv[i], "--compare") == 0)
            compare = true;
        else if (strcmp(argv[i], "--batch") == 0)
            batch = true;
        else if (strcmp(argv[i], "--n-seeds") == 0 && i + 1 < argc)
            n_seeds = atoi(argv[++i]);
        else if (strcmp(argv[i], "--analyze") == 0 && i + 1 < argc) {
            analyze = true;
            csvA = argv[++i];
        }
        else if (strcmp(argv[i], "--compare-csv") == 0 && i + 2 < argc) {
            compareCsv = true;
            csvA = argv[++i];
            csvB = argv[++i];
        }
    }

    if (analyze) {
        BacktestAnalyzer ba;
        if (ba.loadCSV(csvA)) {
            auto analysis = ba.computeIC();
            ba.printReport(analysis);
            // 导出
            std::string out(csvA);
            size_t dot = out.find(".csv");
            if (dot != std::string::npos) out = out.substr(0, dot);
            ba.exportReport(analysis, out + "_factor_report.csv");
        }
    } else if (compareCsv) {
        BacktestAnalyzer::compareStrategies(csvA, csvA, csvB, csvB);
    } else if (batch) {
        runBatch(rounds, n_seeds, "随机策略", randomStrategy);
        runBatch(rounds, n_seeds, "贪心策略", greedyStrategy);
        runBatch(rounds, n_seeds, "多因子策略", factorStrategy);
    } else if (demo) {
        runDemo();
    } else if (compare) {
        runComparison(rounds, seed);
    } else {
        runFullGame(rounds, seed);
    }

    return 0;
}
