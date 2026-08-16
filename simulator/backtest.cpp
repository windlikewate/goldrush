// backtest.cpp — 回测分析实现
#include "backtest.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <cmath>

// ─── 加载CSV ───
bool BacktestAnalyzer::loadCSV(const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "r");
    if (!f) return false;

    char line[4096];
    // 跳过表头
    fgets(line, sizeof(line), f);

    rounds_.clear();

    while (fgets(line, sizeof(line), f)) {
        RoundData rd = {};
        int n = sscanf(line, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &rd.round,
            &rd.p1_gold_this, &rd.p2_gold_this, &rd.p1_cum, &rd.p2_cum, &rd.total_cum,
            &rd.p1_pos_r, &rd.p1_pos_c, &rd.p2_pos_r, &rd.p2_pos_c,
            &rd.p1_bombs, &rd.p2_bombs,
            &rd.enemy_cum, &rd.gold_on_map,
            &rd.k, &rd.order, &rd.vp,
            &rd.p1_steps, &rd.p2_steps,
            &rd.p1_collisions, &rd.p2_collisions,
            &rd.vision_cost, &rd.cum_vision_cost,
            &rd.snap_valid, &rd.snap_win_begin, &rd.snap_win_end,
            &rd.snap_r1_gold, &rd.snap_r1_occ,
            &rd.snap_r2_gold, &rd.snap_r2_occ,
            &rd.snap_r3_gold, &rd.snap_r3_occ,
            &rd.snap_r4_gold, &rd.snap_r4_occ,
            &rd.snap_r5_gold, &rd.snap_r5_occ);

        if (n >= 5) {
            rounds_.push_back(rd);
        }
    }

    fclose(f);

    // 构建场景结果
    if (!rounds_.empty()) {
        ScenarioResult sr = {};
        sr.rounds = (int)rounds_.size();
        sr.player_gold = rounds_.back().p1_cum + rounds_.back().p2_cum;
        sr.enemy_gold = rounds_.back().enemy_cum;
        sr.avg_gold_per_round = (double)sr.player_gold / sr.rounds;
        results_.push_back(sr);
    }

    return !rounds_.empty();
}

// ─── Spearman 秩相关 ───
double BacktestAnalyzer::spearmanCorrelation(const std::vector<double>& x,
                                              const std::vector<double>& y) {
    int n = (int)x.size();
    if (n < 3) return 0.0;

    // 转秩
    auto toRank = [&](const std::vector<double>& v) -> std::vector<double> {
        std::vector<int> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            return v[a] < v[b];
        });
        std::vector<double> rank(n);
        for (int i = 0; i < n; i++)
            rank[idx[i]] = i + 1.0;
        return rank;
    };

    auto rx = toRank(x);
    auto ry = toRank(y);

    // Pearson on ranks
    double mx = 0, my = 0;
    for (double v : rx) mx += v;
    for (double v : ry) my += v;
    mx /= n; my /= n;

    double num = 0, dx = 0, dy = 0;
    for (int i = 0; i < n; i++) {
        double xi = rx[i] - mx, yi = ry[i] - my;
        num += xi * yi;
        dx += xi * xi;
        dy += yi * yi;
    }

    double den = std::sqrt(dx * dy);
    return den > 1e-10 ? num / den : 0.0;
}

// ─── 计算IC ───
std::vector<FactorAnalysis> BacktestAnalyzer::computeIC() {
    std::vector<FactorAnalysis> result;

    // 因子定义: (名称, 取值函数)
    struct FactorDef {
        const char* name;
        std::vector<double>(*extract)(const std::vector<RoundData>&);
    };

    static const FactorDef factors[] = {
        {"F1_淘金", [](const auto& r) {
            std::vector<double> v;
            for (auto& d : r) v.push_back(d.p1_gold_this + d.p2_gold_this);
            return v;
        }},
        {"F2_踩炸弹", [](const auto& r) {
            std::vector<double> v;
            for (auto& d : r) v.push_back(-(d.p1_bombs + d.p2_bombs));  // 负向
            return v;
        }},
        {"F3_位置变化", [](const auto& r) {
            std::vector<double> v;
            for (size_t i = 0; i < r.size(); i++) {
                int moved = (r[i].p1_pos_r != (i > 0 ? r[i-1].p1_pos_r : r[i].p1_pos_r)) +
                            (r[i].p2_pos_r != (i > 0 ? r[i-1].p2_pos_r : r[i].p2_pos_r));
                v.push_back(moved);
            }
            return v;
        }},
        {"F4_敌人距离", [](const auto& r) {
            // 简化: 用地上金子数作为代理(金子少说明敌人可能靠近)
            std::vector<double> v;
            for (auto& d : r) v.push_back(d.gold_on_map);
            return v;
        }},
        {"F5_k分配", [](const auto& r) {
            std::vector<double> v;
            for (auto& d : r) v.push_back(d.k);  // k越大角色0分越多步
            return v;
        }},
        {"F6_视野购买", [](const auto& r) {
            std::vector<double> v;
            for (auto& d : r) v.push_back(d.vp);
            return v;
        }},
        {"F7_碰撞", [](const auto& r) {
            std::vector<double> v;
            for (auto& d : r) v.push_back(-(d.p1_collisions + d.p2_collisions));
            return v;
        }},
        {"F8_路径效率", [](const auto& r) {
            std::vector<double> v;
            for (auto& d : r) v.push_back((double)(d.p1_steps + d.p2_steps) / 6.0);
            return v;
        }},
        {"F8_中心靠近", [](const auto& r) {
            std::vector<double> v;
            for (auto& d : r) {
                // 到中心(8,8)的曼哈顿距离, 越近越好(取反)
                int d1 = std::abs(d.p1_pos_r - 8) + std::abs(d.p1_pos_c - 8);
                int d2 = std::abs(d.p2_pos_r - 8) + std::abs(d.p2_pos_c - 8);
                v.push_back(-(d1 + d2));
            }
            return v;
        }},
    };

    for (const auto& def : factors) {
        FactorAnalysis fa = {};
        fa.name = def.name;

        std::vector<double> factor_vals = def.extract(rounds_);
        std::vector<double> returns;
        for (auto& d : rounds_)
            returns.push_back(d.p1_gold_this + d.p2_gold_this);

        fa.ic = spearmanCorrelation(factor_vals, returns);

        // 分4组分析
        std::vector<std::pair<double, double>> pairs;
        for (size_t i = 0; i < factor_vals.size(); i++)
            pairs.push_back({factor_vals[i], returns[i]});

        std::sort(pairs.begin(), pairs.end());
        int qsize = (int)pairs.size() / 4;
        for (int q = 0; q < 4; q++) {
            double sum = 0;
            int start = q * qsize;
            int end = (q == 3) ? (int)pairs.size() : (q + 1) * qsize;
            for (int i = start; i < end; i++)
                sum += pairs[i].second;
            fa.mean_by_quartile[q] = sum / (end - start);
        }

        result.push_back(fa);
    }

    return result;
}

// ─── 打印报告 ───
void BacktestAnalyzer::printReport(const std::vector<FactorAnalysis>& analysis) const {
    printf("\n══════ 因子分析报告 ══════\n");

    if (!results_.empty()) {
        printf("  场景数: %d\n", totalScenarios());
        printf("  总回合: %d\n", (int)rounds_.size());
        printf("  总金币: %d\n", results_[0].player_gold);
        printf("  平均每轮: %.2f\n", results_[0].avg_gold_per_round);
        printf("  对手金币: %d\n", results_[0].enemy_gold);
    }

    printf("\n  %-12s %8s %8s %8s\n", "因子", "IC", "IR", "Q1→Q4");
    printf("  %-12s %8s %8s %8s\n", "--------", "------", "------", "------");

    for (const auto& fa : analysis) {
        printf("  %-12s %8.3f %8.3f %8.1f→%.0f\n",
               fa.name.c_str(),
               fa.ic,
               fa.ic_std > 0.001 ? fa.ic / fa.ic_std : 0.0,
               fa.mean_by_quartile[0],
               fa.mean_by_quartile[3]);
    }

    printf("\n  解读: IC>0.3 强有效 | 0.1~0.3 弱有效 | <0.1 噪音\n");
    printf("        Q1→Q4: 因子最低组→最高组的平均收益\n\n");
}

// ─── 导出报告 ───
void BacktestAnalyzer::exportReport(const std::vector<FactorAnalysis>& analysis,
                                     const std::string& filename) const {
    FILE* f = fopen(filename.c_str(), "w");
    if (!f) return;

    fprintf(f, "factor,ic,ir,q1_mean,q2_mean,q3_mean,q4_mean\n");
    for (const auto& fa : analysis) {
        fprintf(f, "%s,%.4f,%.4f,%.2f,%.2f,%.2f,%.2f\n",
               fa.name.c_str(), fa.ic,
               fa.ic_std > 0.001 ? fa.ic / fa.ic_std : 0.0,
               fa.mean_by_quartile[0], fa.mean_by_quartile[1],
               fa.mean_by_quartile[2], fa.mean_by_quartile[3]);
    }
    fclose(f);
}

// ─── 策略对比 ───
void BacktestAnalyzer::compareStrategies(const std::string& fileA,
                                          const std::string& nameA,
                                          const std::string& fileB,
                                          const std::string& nameB) {
    BacktestAnalyzer a, b;
    bool okA = a.loadCSV(fileA);
    bool okB = b.loadCSV(fileB);

    if (!okA || !okB) {
        printf("  无法加载CSV文件\n");
        return;
    }

    printf("\n══════ 策略对比 ══════\n");
    printf("  %-20s %10s %10s\n", "", nameA.c_str(), nameB.c_str());
    printf("  %-20s %10d %10d\n", "总金币",
           a.results_[0].player_gold, b.results_[0].player_gold);
    printf("  %-20s %10d %10d\n", "对手金币",
           a.results_[0].enemy_gold, b.results_[0].enemy_gold);
    printf("  %-20s %10.2f %10.2f\n", "平均每轮金",
           a.results_[0].avg_gold_per_round, b.results_[0].avg_gold_per_round);
    printf("  %-20s %+11d\n", "差距",
           a.results_[0].player_gold - b.results_[0].player_gold);
    printf("\n");

    // 对比因子IC
    auto icA = a.computeIC();
    auto icB = b.computeIC();

    printf("  因子IC对比:\n");
    printf("  %-12s %8s %8s %8s\n", "因子", nameA.c_str(), nameB.c_str(), "差异");
    for (size_t i = 0; i < icA.size() && i < icB.size(); i++) {
        printf("  %-12s %8.3f %8.3f %+8.3f\n",
               icA[i].name.c_str(), icA[i].ic, icB[i].ic,
               icA[i].ic - icB[i].ic);
    }
    printf("\n");
}

double BacktestAnalyzer::avgGold() const {
    if (results_.empty()) return 0;
    double sum = 0;
    for (auto& r : results_) sum += r.player_gold;
    return sum / results_.size();
}

double BacktestAnalyzer::winRate() const {
    if (results_.empty()) return 0;
    int wins = 0;
    for (auto& r : results_)
        if (r.player_gold > r.enemy_gold) wins++;
    return (double)wins / results_.size();
}
