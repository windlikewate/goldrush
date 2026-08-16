// factors_enhanced.h — 增强因子评价计算 (追加视野与行动力因子)
#pragma once
#include "factors.h"

// ─── 增强因子值 (继承基础因子) ───
struct EnhancedFactorValues : public FactorValues {
    double vision_efficiency; // F9: 视野利用率 (评估视野内金币饱和度与购买行为的匹配度)
    double action_balance;    // F10: 行动力均衡 (评估当前 k 分配下双角色的实际有效移动)
};

// ─── 增强因子权重 (前8个完全继承 DEFAULT_WEIGHTS) ───
extern const double ENHANCED_WEIGHTS[10];

// 计算所有增强因子
// 注意：需额外传入候选的 vp (vision purchase) 决策，以供视野利用率因子评估
EnhancedFactorValues computeEnhancedFactors(const GameInput* input, 
                                            const int actions[S], 
                                            int k, 
                                            int vp);

// 增强综合评分
double scoreEnhancedFactors(const EnhancedFactorValues& f, const double weights[10]);