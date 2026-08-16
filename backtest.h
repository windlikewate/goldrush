// backtest.h — 回测分析
#pragma once
#include <vector>
#include <string>

// ─── 单场景结果 ───
struct ScenarioResult {
    int seed;
    int rounds;
    int player_gold;
    int enemy_gold;
    double avg_gold_per_round;
    double p90_latency;
};

// ─── 因子分析结果 ───
struct FactorAnalysis {
    std::string name;
    double ic;              // Spearman 相关系数
    double ic_std;          // IC 标准差(跨场景)
    double icir;            // IC / IC_std (信息比率)
    double mean_by_quartile[4];  // 按因子分4组,每组平均收益
};

// ─── 回测分析器 ───
class BacktestAnalyzer {
public:
    // 从CSV文件加载数据
    bool loadCSV(const std::string& filename);

    // 计算所有因子的IC
    std::vector<FactorAnalysis> computeIC();

    // 打印分析报告
    void printReport(const std::vector<FactorAnalysis>& analysis) const;

    // 导出为CSV
    void exportReport(const std::vector<FactorAnalysis>& analysis,
                      const std::string& filename) const;

    // 策略对比: 两个CSV的差异
    static void compareStrategies(const std::string& fileA,
                                  const std::string& nameA,
                                  const std::string& fileB,
                                  const std::string& nameB);

    // 获取统计
    int totalScenarios() const { return (int)results_.size(); }
    double avgGold() const;
    double winRate() const;  // 胜率

private:
    struct RoundData {
        int round;
        int p1_gold_this, p2_gold_this;
        int p1_cum, p2_cum, total_cum;
        int p1_pos_r, p1_pos_c, p2_pos_r, p2_pos_c;
        int p1_bombs, p2_bombs;
        int enemy_cum, gold_on_map;
        int k, order, vp;
        int p1_steps, p2_steps;
        int p1_collisions, p2_collisions;
        int vision_cost, cum_vision_cost;
        int snap_valid, snap_win_begin, snap_win_end;
        int snap_r1_gold, snap_r1_occ;
        int snap_r2_gold, snap_r2_occ;
        int snap_r3_gold, snap_r3_occ;
        int snap_r4_gold, snap_r4_occ;
        int snap_r5_gold, snap_r5_occ;
    };

    std::vector<RoundData> rounds_;
    std::vector<ScenarioResult> results_;

    // 计算Spearman秩相关
    static double spearmanCorrelation(const std::vector<double>& x,
                                      const std::vector<double>& y);
};
